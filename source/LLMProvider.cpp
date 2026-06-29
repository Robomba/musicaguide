#include "LLMProvider.h"

static constexpr const char* kSystemPrompt =
    "You are Musicaguide, an expert music theory and production assistant embedded inside a DAW. "
    "Help musicians with: scales, chords, progressions, modes, intervals, arrangement, mixing, "
    "sound design, and production techniques. Be concise and practical. "
    "When suggesting chord progressions or scales, give concrete note names "
    "(e.g. \"ii-V-I in C major: Dm7 - G7 - Cmaj7\"). "
    "Avoid lengthy preambles — get straight to the answer.";

// ─── Helper: build JSON manually (avoids external deps) ────────────────────
static juce::String buildOpenAIRequest (const juce::String& model,
                                         const juce::Array<LLMProvider::Message>& messages)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("model", model);
    root->setProperty ("max_tokens", 1024);
    root->setProperty ("temperature", 0.7);

    juce::Array<juce::var> msgs;

    // system message
    juce::DynamicObject::Ptr sys = new juce::DynamicObject();
    sys->setProperty ("role",    "system");
    sys->setProperty ("content", kSystemPrompt);
    msgs.add (juce::var (sys.get()));

    for (auto& m : messages)
    {
        juce::DynamicObject::Ptr msg = new juce::DynamicObject();
        msg->setProperty ("role",    m.role);
        msg->setProperty ("content", m.content);
        msgs.add (juce::var (msg.get()));
    }

    root->setProperty ("messages", msgs);
    return juce::JSON::toString (juce::var (root.get()), false);
}

static juce::String buildAnthropicRequest (const juce::String& model,
                                             const juce::Array<LLMProvider::Message>& messages)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("model",      model);
    root->setProperty ("max_tokens", 1024);
    root->setProperty ("system",     kSystemPrompt);

    juce::Array<juce::var> msgs;
    for (auto& m : messages)
    {
        juce::DynamicObject::Ptr msg = new juce::DynamicObject();
        msg->setProperty ("role",    m.role);
        msg->setProperty ("content", m.content);
        msgs.add (juce::var (msg.get()));
    }
    root->setProperty ("messages", msgs);
    return juce::JSON::toString (juce::var (root.get()), false);
}

static juce::String extractOpenAIContent (const juce::String& responseText)
{
    auto parsed = juce::JSON::parse (responseText);
    if (auto* obj = parsed.getDynamicObject())
    {
        if (obj->hasProperty ("error"))
        {
            if (auto* errObj = obj->getProperty ("error").getDynamicObject())
                return "API error: " + errObj->getProperty ("message").toString();
            return "API error";
        }
        if (auto* choices = obj->getProperty ("choices").getArray())
        {
            if (choices->size() > 0)
            {
                if (auto* choice = (*choices)[0].getDynamicObject())
                {
                    if (auto* msgObj = choice->getProperty ("message").getDynamicObject())
                        return msgObj->getProperty ("content").toString().trim();
                }
            }
        }
    }
    return {};
}

static juce::String extractAnthropicContent (const juce::String& responseText)
{
    auto parsed = juce::JSON::parse (responseText);
    if (auto* obj = parsed.getDynamicObject())
    {
        if (obj->hasProperty ("error"))
        {
            if (auto* errObj = obj->getProperty ("error").getDynamicObject())
                return "API error: " + errObj->getProperty ("message").toString();
            return "API error";
        }
        if (auto* content = obj->getProperty ("content").getArray())
        {
            if (content->size() > 0)
            {
                if (auto* block = (*content)[0].getDynamicObject())
                {
                    if (block->getProperty ("type").toString() == "text")
                        return block->getProperty ("text").toString().trim();
                }
            }
        }
    }
    return {};
}

// ─── Request thread ─────────────────────────────────────────────────────────
void LLMProvider::RequestThread::run()
{
    juce::String url, body, extraHeaders;
    bool isAnthropic = provider == "anthropic";

    if (isAnthropic)
    {
        url  = "https://api.anthropic.com/v1/messages";
        body = buildAnthropicRequest (model, messages);
        // Anthropic uses x-api-key per their API spec (not Authorization Bearer)
        extraHeaders = "x-api-key: " + apiKey + "\r\n"
                       "anthropic-version: 2023-06-01\r\n"
                       "content-type: application/json";
    }
    else
    {
        url  = "https://api.openai.com/v1/chat/completions";
        body = buildOpenAIRequest (model, messages);
        extraHeaders = "Authorization: Bearer " + apiKey + "\r\n"
                       "Content-Type: application/json";
    }

    // Enforce HTTPS — reject any non-encrypted endpoint
    if (!url.startsWith ("https://"))
    {
        fireCallback ({}, false, "Security: only HTTPS endpoints are permitted");
        return;
    }

    juce::URL httpUrl (url);
    httpUrl = httpUrl.withPOSTData (body);

    // connectionTimeoutMs covers connect + transfer on most platforms (30 s)
    auto stream = httpUrl.createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
            .withExtraHeaders (extraHeaders)
            .withConnectionTimeoutMs (30000));

    if (threadShouldExit())
        return;

    if (stream == nullptr)
    {
        fireCallback ({}, false, "Network error: could not connect to API");
        return;
    }

    auto responseText = stream->readEntireStreamAsString();

    if (threadShouldExit())
        return;

    juce::String content = isAnthropic
                             ? extractAnthropicContent (responseText)
                             : extractOpenAIContent (responseText);

    if (content.isEmpty())
    {
        fireCallback ({}, false, "Empty or unrecognised response from API");
        return;
    }

    bool isError = content.startsWith ("API error:");
    fireCallback (content, !isError, isError ? content : juce::String{});
}

// ─── LLMProvider public API ─────────────────────────────────────────────────
LLMProvider::~LLMProvider() { cancel(); }

void LLMProvider::send (const juce::String& apiKey,
                         const juce::String& provider,
                         const juce::String& model,
                         const juce::Array<Message>& messages,
                         ResponseCallback callback)
{
    cancel();
    thread = std::make_unique<RequestThread> (apiKey, provider, model, messages, std::move (callback));
    thread->startThread();
}

void LLMProvider::cancel()
{
    if (thread)
    {
        thread->signalThreadShouldExit();
        thread->waitForThreadToExit (3000);
        thread.reset();
    }
}

bool LLMProvider::isBusy() const
{
    return thread != nullptr && thread->isThreadRunning();
}
