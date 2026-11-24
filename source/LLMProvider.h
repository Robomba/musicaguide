#pragma once
#include <JuceHeader.h>
#include <functional>

class LLMProvider
{
public:
    struct Message
    {
        juce::String role;     // "user" | "assistant"
        juce::String content;
    };

    // usedModel is the model that actually answered. It differs from the requested
    // model only when the requested one was retired and the manifest fallback answered.
    using ResponseCallback = std::function<void (const juce::String& response,
                                                  bool success,
                                                  const juce::String& errorMsg,
                                                  const juce::String& usedModel)>;

    ~LLMProvider();

    void send (const juce::String& apiKey,
               const juce::String& provider,  // "openai" | "anthropic" | "groq"
               const juce::String& model,
               const juce::Array<Message>& messages,
               ResponseCallback callback);

    void cancel();
    bool isBusy() const;

private:
    class RequestThread : public juce::Thread
    {
    public:
        RequestThread (const juce::String& key,
                       const juce::String& prov,
                       const juce::String& mdl,
                       const juce::Array<LLMProvider::Message>& msgs,
                       ResponseCallback cb)
            : juce::Thread ("LLM Request"),
              apiKey (key), provider (prov), model (mdl),
              messages (msgs), callback (std::move (cb))
        {}

        void run() override;

    private:
        struct Attempt
        {
            int          status = 0;
            bool         connected = false;
            juce::String body;
        };

        Attempt post (const juce::String& modelId);

        void fireCallback (const juce::String& response, bool success,
                           const juce::String& err, const juce::String& used)
        {
            auto cb = callback;
            juce::MessageManager::callAsync ([cb, response, success, err, used]()
            {
                cb (response, success, err, used);
            });
        }

        juce::String apiKey, provider, model;
        juce::Array<LLMProvider::Message> messages;
        ResponseCallback callback;
    };

    std::unique_ptr<RequestThread> thread;
};
