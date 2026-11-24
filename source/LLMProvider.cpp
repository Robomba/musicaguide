#include "LLMProvider.h"
#include "ModelManifest.h"
#include "LLMWire.h"

static constexpr const char* kSystemPrompt =
    "You are Musicaguide, an expert music theory and production assistant embedded inside a DAW. "
    "Help musicians with: scales, chords, progressions, modes, intervals, arrangement, mixing, "
    "sound design, and production techniques. Be concise and practical. "
    "When suggesting chord progressions or scales, give concrete note names "
    "(e.g. \"ii-V-I in C major: Dm7 - G7 - Cmaj7\"). "
    "Avoid lengthy preambles. Get straight to the answer.";

namespace LLMWire
{
// ─── Request bodies ─────────────────────────────────────────────────────────
// Which token-limit field to send, how many tokens, and whether to send temperature all
// come from the model manifest, because providers change these per model generation.
// (OpenAI's GPT-5+ models reject "max_tokens" and any non-default temperature outright;
// sending them turned every request into a 400.)
juce::String buildOpenAIRequest (const juce::String& model,
                                         const ModelManifest::ProviderConfig& cfg,
                                         const juce::Array<LLMProvider::Message>& messages)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("model", model);
    root->setProperty (cfg.tokenParam, cfg.maxOutputTokens);
    if (cfg.sendTemperature)
        root->setProperty ("temperature", cfg.temperature);

    juce::Array<juce::var> msgs;

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

juce::String buildAnthropicRequest (const juce::String& model,
                                             const ModelManifest::ProviderConfig& cfg,
                                             const juce::Array<LLMProvider::Message>& messages)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("model",      model);
    root->setProperty ("max_tokens", cfg.maxOutputTokens);  // Anthropic only accepts max_tokens
    root->setProperty ("system",     kSystemPrompt);
    if (cfg.sendTemperature)
        root->setProperty ("temperature", cfg.temperature);

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

// ─── Response parsing ───────────────────────────────────────────────────────
juce::String apiErrorMessage (const juce::var& parsed)
{
    if (auto* obj = parsed.getDynamicObject())
        if (obj->hasProperty ("error"))
        {
            if (auto* errObj = obj->getProperty ("error").getDynamicObject())
                return errObj->getProperty ("message").toString();
            return obj->getProperty ("error").toString();
        }
    return {};
}

juce::String apiErrorCode (const juce::var& parsed)
{
    if (auto* obj = parsed.getDynamicObject())
        if (auto* errObj = obj->getProperty ("error").getDynamicObject())
            return errObj->getProperty ("code").toString() + " " + errObj->getProperty ("type").toString();
    return {};
}

// True when the provider is saying "this model doesn't exist / was retired / can't be
// used here", as opposed to a bad key, a rate limit or an outage. Only these errors are
// worth retrying on the fallback model.
bool isModelUnavailableError (int status, const juce::var& parsed)
{
    if (status == 404)
        return true;
    if (status != 400 && status != 403 && status != 410)
        return false;

    auto code = apiErrorCode (parsed).toLowerCase();
    if (code.contains ("model_not_found") || code.contains ("model_decommissioned")
        || code.contains ("model_deprecated") || code.contains ("not_found_error"))
        return true;

    auto msg = apiErrorMessage (parsed).toLowerCase();
    if (! msg.contains ("model"))
        return false;
    return msg.contains ("not found") || msg.contains ("does not exist")
        || msg.contains ("decommissioned") || msg.contains ("deprecated")
        || msg.contains ("no longer") || msg.contains ("retired")
        || msg.contains ("not supported") || msg.contains ("invalid model")
        || msg.contains ("do not have access");
}

juce::String extractOpenAIContent (const juce::var& parsed, bool& hitLengthLimit)
{
    hitLengthLimit = false;
    if (auto* obj = parsed.getDynamicObject())
        if (auto* choices = obj->getProperty ("choices").getArray())
            if (choices->size() > 0)
                if (auto* choice = (*choices)[0].getDynamicObject())
                {
                    hitLengthLimit = choice->getProperty ("finish_reason").toString() == "length";
                    if (auto* msgObj = choice->getProperty ("message").getDynamicObject())
                    {
                        auto content = msgObj->getProperty ("content");
                        return content.isString() ? content.toString().trim() : juce::String();
                    }
                }
    return {};
}

// Newer Claude models can return several content blocks (e.g. a "thinking" block before
// the answer). Reading only block 0 made a perfectly good reply look empty. Join every
// text block instead and skip everything else.
juce::String extractAnthropicContent (const juce::var& parsed, bool& hitLengthLimit)
{
    hitLengthLimit = false;
    juce::StringArray parts;
    if (auto* obj = parsed.getDynamicObject())
    {
        hitLengthLimit = obj->getProperty ("stop_reason").toString() == "max_tokens";
        if (auto* content = obj->getProperty ("content").getArray())
            for (auto& blockVar : *content)
                if (auto* block = blockVar.getDynamicObject())
                    if (block->getProperty ("type").toString() == "text")
                        parts.add (block->getProperty ("text").toString());
    }
    return parts.joinIntoString ("").trim();
}

} // namespace LLMWire

using namespace LLMWire;

// ─── Request thread ─────────────────────────────────────────────────────────
LLMProvider::RequestThread::Attempt LLMProvider::RequestThread::post (const juce::String& modelId)
{
    Attempt result;
    const auto cfg = ModelManifest::get().forProvider (provider);
    const bool isAnthropic = provider == "anthropic";

    // Endpoints are hardcoded here on purpose and never read from the manifest: the
    // API key goes in these headers, so only these three hosts may ever receive it.
    juce::String url, body, extraHeaders;
    if (isAnthropic)
    {
        url  = "https://api.anthropic.com/v1/messages";
        body = buildAnthropicRequest (modelId, cfg, messages);
        extraHeaders = "x-api-key: " + apiKey + "\r\n"
                       "anthropic-version: 2023-06-01\r\n"
                       "content-type: application/json";
    }
    else
    {
        // Groq is OpenAI-wire-compatible: same body shape, same bearer auth.
        url  = provider == "groq"
                 ? "https://api.groq.com/openai/v1/chat/completions"
                 : "https://api.openai.com/v1/chat/completions";
        body = buildOpenAIRequest (modelId, cfg, messages);
        extraHeaders = "Authorization: Bearer " + apiKey + "\r\n"
                       "Content-Type: application/json";
    }

    juce::URL httpUrl = juce::URL (url).withPOSTData (body);

    // Reasoning models can take longer than the old 30 s before the first byte.
    auto stream = httpUrl.createInputStream (
        juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
            .withExtraHeaders (extraHeaders)
            .withStatusCode (&result.status)
            .withConnectionTimeoutMs (60000));

    if (stream == nullptr)
        return result;

    result.connected = true;
    result.body = stream->readEntireStreamAsString();
    return result;
}

void LLMProvider::RequestThread::run()
{
    const bool isAnthropic = provider == "anthropic";
    const auto fallback = ModelManifest::get().forProvider (provider).fallbackModel;

    juce::String usedModel = model;
    auto attempt = post (usedModel);
    if (threadShouldExit()) return;

    auto parsed = juce::JSON::parse (attempt.body);

    // The chosen model is gone: retry exactly once on the manifest's fallback, and tell
    // the caller which model answered so it can save the switch.
    if (attempt.connected && isModelUnavailableError (attempt.status, parsed)
        && fallback.isNotEmpty() && ! fallback.equalsIgnoreCase (model))
    {
        usedModel = fallback;
        attempt = post (usedModel);
        if (threadShouldExit()) return;
        parsed = juce::JSON::parse (attempt.body);
    }

    if (! attempt.connected)
    {
        fireCallback ({}, false, "Network error: could not connect to API", usedModel);
        return;
    }

    if (attempt.status == 429)
    {
        fireCallback ({}, false,
                      "Rate limited: the provider's per-minute cap was hit. "
                      "Wait ~30 seconds and try again. (Free tiers hit this cap "
                      "sooner; a paid key raises it.)", usedModel);
        return;
    }

    if (attempt.status == 401)
    {
        fireCallback ({}, false,
                      "API key rejected (401). Check in Settings that the key belongs to the "
                      "selected provider.", usedModel);
        return;
    }

    auto apiErr = apiErrorMessage (parsed);
    if (apiErr.isNotEmpty() || attempt.status >= 400)
    {
        auto msg = "API error (" + juce::String (attempt.status) + "): "
                   + (apiErr.isNotEmpty() ? apiErr : juce::String ("no details"));
        if (isModelUnavailableError (attempt.status, parsed))
            msg << "\nPick another model in Settings (\"Load models\" lists what your key can use).";
        fireCallback ({}, false, msg, usedModel);
        return;
    }

    bool hitLengthLimit = false;
    auto content = isAnthropic ? extractAnthropicContent (parsed, hitLengthLimit)
                               : extractOpenAIContent  (parsed, hitLengthLimit);

    if (content.isEmpty())
    {
        fireCallback ({}, false,
                      hitLengthLimit
                        ? "The model used its whole output budget before answering "
                          "(common with reasoning models). Try again, or pick a lighter model."
                        : "Empty or unrecognised response from API",
                      usedModel);
        return;
    }

    fireCallback (content, true, {}, usedModel);
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
