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
}

juce::PropertiesFile* FreemiumManager::prefs() const { return propsFile; }

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
    auto defaultModel = prov == "anthropic" ? "claude-haiku-4-5-20251001" : "gpt-4o-mini";
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
