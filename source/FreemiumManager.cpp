#include "FreemiumManager.h"
#include "ModelManifest.h"

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

    // A saved model ID always beats the provider default, so a retired ID on disk would
    // break every request after the provider shuts it off. Repoint it to the replacement
    // the model manifest names. The list of retired IDs lives in models.json (cached from
    // the website), not in this binary, so new retirements are handled without a release.
    //
    // Deliberately a denylist of retired IDs, not an allowlist of known-good ones: the
    // model box is editable so users can follow provider rotations without waiting on us,
    // and an allowlist would stomp every valid ID we haven't heard of yet.
    const auto provider = p->getValue ("provider");
    const auto saved    = p->getValue ("model");
    const auto replacement = ModelManifest::get().replacementFor (provider, saved);
    if (replacement.isNotEmpty())
    {
        p->setValue ("model", replacement);
        p->saveIfNeeded();
    }
}

void FreemiumManager::setModel (const juce::String& model)
{
    if (auto* p = prefs())
    {
        p->setValue ("model", model);
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
    const auto prov = p ? p->getValue ("provider", "openai") : juce::String ("openai");
    const auto& manifest = ModelManifest::get();
    const auto defaultModel = manifest.forProvider (prov).defaultModel;
    if (p == nullptr) return defaultModel;

    auto saved = p->getValue ("model", defaultModel);
    if (saved.isEmpty()) return defaultModel;

    // The manifest may have learned about a retirement since launch (background refresh).
    auto replacement = manifest.replacementFor (prov, saved);
    return replacement.isNotEmpty() ? replacement : saved;
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
