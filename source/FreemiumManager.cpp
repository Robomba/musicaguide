#include "FreemiumManager.h"

FreemiumManager::FreemiumManager()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName     = "Musicaguide";
    opts.folderName          = "Musicaguide";
    opts.filenameSuffix      = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    appProps.setStorageParameters (opts);
    propsFile = appProps.getUserSettings();

    migrateRetiredModels();
}

juce::PropertiesFile* FreemiumManager::prefs() const { return propsFile; }

void FreemiumManager::migrateRetiredModels()
{
    auto* p = prefs();
    if (p == nullptr) return;

    // Groq retired every Llama chat model from production (verified against
    // console.groq.com/docs/models, 2026-08-18).
    //
    // A saved model ID always beats the provider default in getModel(), so the new
    // gpt-oss-120b default only reaches FRESH installs. Anyone who already used Groq
    // has a dead Llama ID on disk and would upgrade into exactly the breakage this
    // release exists to fix. Repoint them once, here, on load.
    //
    // Deliberately a denylist of the retired family, not an allowlist of known-good
    // IDs: the model box is editable so that users can follow provider rotations
    // without waiting on us, and an allowlist would stomp every ID we haven't heard
    // of yet - including the valid ones.
    if (p->getValue ("provider") != "groq") return;

    if (p->getValue ("model").startsWithIgnoreCase ("llama"))
    {
        p->setValue ("model", "openai/gpt-oss-120b");
        p->saveIfNeeded();
    }
}

bool FreemiumManager::hasApiKey() const
{
    return SecureStore::getKey().isNotEmpty();
}

void FreemiumManager::setApiKey (const juce::String& key,
                                  const juce::String& provider,
                                  const juce::String& model)
{
    SecureStore::setKey (key);

    if (auto* p = prefs())
    {
        p->setValue ("provider", provider);
        p->setValue ("model",    model);
        p->saveIfNeeded();
    }
}

juce::String FreemiumManager::getApiKey() const
{
    return SecureStore::getKey();
}

juce::String FreemiumManager::getProvider() const
{
    auto* p = prefs();
    return p ? p->getValue ("provider", "openai") : "openai";
}

juce::String FreemiumManager::getModel() const
{
    auto* p = prefs();
    if (!p) return "gpt-4o-mini";
    auto prov = p->getValue ("provider", "openai");
    juce::String defaultModel = "gpt-4o-mini";
    if      (prov == "anthropic") defaultModel = "claude-haiku-4-5-20251001";
    else if (prov == "groq")      defaultModel = "openai/gpt-oss-120b";
    return p->getValue ("model", defaultModel);
}

void FreemiumManager::clearApiKey()
{
    SecureStore::clearKey();

    if (auto* p = prefs())
    {
        p->removeValue ("provider");
        p->removeValue ("model");
        p->saveIfNeeded();
    }
}

bool FreemiumManager::canSendMessage() const
{
    return hasApiKey();
}

juce::String FreemiumManager::getInstallId() const
{
    auto* p = prefs();
    if (!p) return {};
    auto id = p->getValue ("install_id");
    if (id.isEmpty())
    {
        id = juce::Uuid().toString();
        p->setValue ("install_id", id);
        p->saveIfNeeded();
    }
    return id;
}

bool FreemiumManager::isAnalyticsEnabled() const
{
    auto* p = prefs();
    return p ? p->getBoolValue ("analytics_enabled", true) : false;
}

void FreemiumManager::setAnalyticsEnabled (bool enabled)
{
    if (auto* p = prefs())
    {
        p->setValue ("analytics_enabled", enabled);
        p->saveIfNeeded();
    }
}

bool FreemiumManager::hasShownAnalyticsDisclosure() const
{
    auto* p = prefs();
    return p ? p->getBoolValue ("analytics_disclosure_shown", false) : true;
}

void FreemiumManager::markAnalyticsDisclosureShown()
{
    if (auto* p = prefs())
    {
        p->setValue ("analytics_disclosure_shown", true);
        p->saveIfNeeded();
    }
}
