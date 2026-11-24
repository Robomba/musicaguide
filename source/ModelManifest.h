#pragma once
#include <JuceHeader.h>

// Which model IDs to suggest, which to fall back to, and how to talk to each provider.
//
// Providers retire model IDs every few months. When that knowledge lives only in the
// compiled binary, every retirement bricks every installed copy until a new notarized
// build ships. So the knowledge lives in a small JSON file instead:
//
//   https://www.leosolproducts.com/musicaguide/models.json
//
// The plugin fetches it in the background at startup, caches it on disk, and falls back
// to the copy compiled in below when offline. Updating that one file updates every
// install, no rebuild.
//
// Security: the manifest can only change model IDs and a few numeric request settings.
// It can NEVER change an endpoint URL or a header, so a tampered manifest cannot send a
// user's API key anywhere. Every field is validated; anything malformed is ignored and
// the last good copy stays in use. The fetch is a plain GET with no identifiers.
class ModelManifest
{
public:
    struct ProviderConfig
    {
        juce::String      defaultModel;
        juce::String      fallbackModel;    // tried once when the chosen model is gone
        juce::StringArray suggested;        // shown first in the Settings dropdown
        juce::String      tokenParam { "max_tokens" };  // or "max_completion_tokens"
        int               maxOutputTokens = 1024;
        bool              sendTemperature = false;
        double            temperature     = 0.7;
        juce::StringArray listInclude;      // live /models filter: ID must start with one (empty = any)
        juce::StringArray listExclude;      // live /models filter: ID must not contain any
    };

    static ModelManifest& get();

    // Kick off a background refresh from the website. Safe to call more than once;
    // it fetches at most once per process.
    void refreshInBackground();

    ProviderConfig forProvider (const juce::String& provider) const;

    // Replacement for a retired model ID, or empty if the ID is not known to be retired.
    // Keys ending in '*' match by prefix (e.g. "llama*").
    juce::String replacementFor (const juce::String& provider, const juce::String& model) const;

    juce::String getUpdatedDate() const;

    static bool isValidModelId (const juce::String& id);

    // True if json is a manifest this build would accept (used by tests).
    static bool validate (const juce::String& json) { Data d; return parse (json, d); }

    ~ModelManifest();

private:
    ModelManifest();
    class FetchThread;

    struct Data
    {
        std::map<juce::String, ProviderConfig> providers;
        std::map<juce::String, std::vector<std::pair<juce::String, juce::String>>> retired;
        juce::String updated;
    };

    static bool parse (const juce::String& json, Data& out);
    static juce::File cacheFile();

    mutable juce::CriticalSection lock;
    Data data;
    std::atomic<bool> refreshStarted { false };
    std::unique_ptr<FetchThread> fetchThread;

    JUCE_DECLARE_NON_COPYABLE (ModelManifest)
};
