#pragma once
#include <JuceHeader.h>
#include "SecureStore.h"

class FreemiumManager
{
public:
    FreemiumManager();

    bool hasApiKey() const;

    // Key is stored in the OS secure store; provider/model in PropertiesFile.
    void         setApiKey  (const juce::String& key,
                             const juce::String& provider,
                             const juce::String& model);
    juce::String getApiKey  () const;
    juce::String getProvider() const;
    juce::String getModel   () const;
    void         clearApiKey();

    // True only when the user has entered their own API key.
    bool canSendMessage() const;

    // Analytics state — all persisted in PropertiesFile (non-secret)
    juce::String getInstallId() const;      // lazily created UUID, never changes
    bool isAnalyticsEnabled() const;        // default: true (opt-out model)
    void setAnalyticsEnabled (bool enabled);
    bool hasShownAnalyticsDisclosure() const;
    void markAnalyticsDisclosureShown();

private:
    juce::ApplicationProperties appProps;
    juce::PropertiesFile* propsFile = nullptr;

    juce::PropertiesFile* prefs() const;
};
