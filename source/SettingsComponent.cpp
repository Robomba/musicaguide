#include "SettingsComponent.h"
#include "UIColors.h"
#include "ModelManifest.h"

// Fetches the model IDs the user's own key can use, straight from the provider
// (GET /v1/models). Runs off the message thread; the result is posted back through a
// SafePointer so closing Settings mid-request is harmless. The key only ever goes to
// the provider's own host, same as a chat request.
namespace
{
    class ModelListThread : public juce::Thread
    {
    public:
        using Done = std::function<void (juce::StringArray ids, juce::String error)>;

        ModelListThread (juce::String prov, juce::String key, Done cb)
            : juce::Thread ("Musicaguide model list"),
              provider (std::move (prov)), apiKey (std::move (key)), done (std::move (cb)) {}

        void run() override
        {
            juce::String url, headers;
            if (provider == "anthropic")
            {
                url = "https://api.anthropic.com/v1/models?limit=100";
                headers = "x-api-key: " + apiKey + "\r\nanthropic-version: 2023-06-01";
            }
            else
            {
                url = provider == "groq" ? "https://api.groq.com/openai/v1/models"
                                         : "https://api.openai.com/v1/models";
                headers = "Authorization: Bearer " + apiKey;
            }

            int status = 0;
            auto stream = juce::URL (url).createInputStream (
                juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders (headers)
                    .withStatusCode (&status)
                    .withConnectionTimeoutMs (15000));

            if (threadShouldExit()) return;
            if (stream == nullptr) { finish ({}, "Could not reach the provider."); return; }

            auto parsed = juce::JSON::parse (stream->readEntireStreamAsString());
            if (status == 401) { finish ({}, "Key rejected (401) for this provider."); return; }
            if (status != 200) { finish ({}, "Provider returned " + juce::String (status) + "."); return; }

            const auto cfg = ModelManifest::get().forProvider (provider);
            juce::StringArray ids;
            if (auto* obj = parsed.getDynamicObject())
                if (auto* arr = obj->getProperty ("data").getArray())
                    for (auto& m : *arr)
                        if (auto* mo = m.getDynamicObject())
                        {
                            auto id = mo->getProperty ("id").toString();
                            if (! ModelManifest::isValidModelId (id)) continue;

                            // Skip models the provider has scheduled for removal or retired.
                            auto lifecycle = mo->getProperty ("lifecycle").toString();
                            if (lifecycle.isNotEmpty() && lifecycle != "active") continue;
                            auto shutdown = mo->getProperty ("shutdown_date");
                            if (! shutdown.isVoid() && shutdown.toString().isNotEmpty()) continue;
                            if (mo->hasProperty ("active") && ! (bool) mo->getProperty ("active")) continue;

                            bool included = cfg.listInclude.isEmpty();
                            for (auto& pre : cfg.listInclude)
                                if (id.startsWithIgnoreCase (pre)) { included = true; break; }
                            if (! included) continue;

                            bool excluded = false;
                            for (auto& bad : cfg.listExclude)
                                if (id.containsIgnoreCase (bad)) { excluded = true; break; }
                            if (excluded) continue;

                            if (ModelManifest::get().replacementFor (provider, id).isNotEmpty()) continue;
                            ids.addIfNotAlreadyThere (id);
                        }

            ids.sortNatural();
            finish (ids, ids.isEmpty() ? juce::String ("No chat models found for this key.") : juce::String());
        }

    private:
        void finish (juce::StringArray ids, juce::String err)
        {
            auto cb = done;
            juce::MessageManager::callAsync ([cb, ids, err] { cb (ids, err); });
        }

        juce::String provider, apiKey;
        Done done;
    };
}

static void styleLabel (juce::Label& l, float size = 13.f, bool bold = false)
{
    l.setFont (juce::Font (size, bold ? juce::Font::bold : 0));
    l.setColour (juce::Label::textColourId, Colors::textMain);
}

static void styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId, Colors::surface2);
    c.setColour (juce::ComboBox::textColourId,       Colors::textMain);
    c.setColour (juce::ComboBox::outlineColourId,    Colors::border);
    c.setColour (juce::ComboBox::arrowColourId,      Colors::accentLt);
}

SettingsComponent::SettingsComponent (FreemiumManager& fm)
    : freemiumManager (fm)
{
    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (18.f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, Colors::textMain);

    addAndMakeVisible (provLabel);
    styleLabel (provLabel);

    addAndMakeVisible (providerCombo);
    styleCombo (providerCombo);
    providerCombo.addItem ("OpenAI",    1);
    providerCombo.addItem ("Anthropic", 2);
    providerCombo.addItem ("Groq (free tier)", 3);
    providerCombo.setSelectedId (1, juce::dontSendNotification);
    providerCombo.onChange = [this] { updateProviderModels(); };

    addAndMakeVisible (modelLabel);
    styleLabel (modelLabel);

    addAndMakeVisible (modelCombo);
    styleCombo (modelCombo);
    // Editable on purpose. Providers retire model IDs on their own schedule, and a fixed
    // list compiled into the binary means a deprecation bricks every installed copy until
    // a rebuild ships. The dropdown is suggestions, not a cage: any valid ID can be typed.
    modelCombo.setEditableText (true);
    modelCombo.setTextWhenNothingSelected ("type any model ID your provider supports");

    // Suggestions come from models.json; "Load models" asks the provider directly which
    // models this key can use right now, so new releases show up with no update.
    addAndMakeVisible (loadModelsButton);
    loadModelsButton.setColour (juce::TextButton::buttonColourId,  Colors::surface2);
    loadModelsButton.setColour (juce::TextButton::textColourOnId,  Colors::textMain);
    loadModelsButton.setColour (juce::TextButton::textColourOffId, Colors::textMain);
    loadModelsButton.onClick = [this] { loadLiveModels(); };

    addAndMakeVisible (modelStatus);
    modelStatus.setFont (juce::Font (11.f));
    modelStatus.setColour (juce::Label::textColourId, Colors::textSub);

    addAndMakeVisible (keyLabel);
    styleLabel (keyLabel);

    addAndMakeVisible (keyEditor);
    keyEditor.setPasswordCharacter (0x2022);
    keyEditor.setMultiLine (false);
    keyEditor.setTextToShowWhenEmpty ("sk-...  /  sk-ant-...  /  gsk_...", Colors::textSub);
    keyEditor.setFont (juce::Font (13.f));
    keyEditor.setColour (juce::TextEditor::backgroundColourId,     Colors::surface2);
    keyEditor.setColour (juce::TextEditor::textColourId,           Colors::textMain);
    keyEditor.setColour (juce::TextEditor::outlineColourId,        Colors::border);
    keyEditor.setColour (juce::TextEditor::focusedOutlineColourId, Colors::accent);

    addAndMakeVisible (showKeyToggle);
    showKeyToggle.setColour (juce::ToggleButton::textColourId,         Colors::textSub);
    showKeyToggle.setColour (juce::ToggleButton::tickColourId,         Colors::accent);
    showKeyToggle.setColour (juce::ToggleButton::tickDisabledColourId, Colors::border);
    showKeyToggle.onStateChange = [this]
    {
        keyEditor.setPasswordCharacter (showKeyToggle.getToggleState() ? 0 : 0x2022);
    };

    addAndMakeVisible (saveButton);
    saveButton.setColour (juce::TextButton::buttonColourId,  Colors::accent);
    saveButton.setColour (juce::TextButton::textColourOnId,  Colors::textMain);
    saveButton.setColour (juce::TextButton::textColourOffId, Colors::textMain);
    saveButton.onClick = [this] { saveSettings(); };

    addAndMakeVisible (clearButton);
    clearButton.setColour (juce::TextButton::buttonColourId,  Colors::surface2);
    clearButton.setColour (juce::TextButton::textColourOnId,  Colors::textMain);
    clearButton.setColour (juce::TextButton::textColourOffId, Colors::textMain);
    clearButton.onClick = [this] { clearSettings(); };

    addAndMakeVisible (analyticsToggle);
    analyticsToggle.setColour (juce::ToggleButton::textColourId,         Colors::textSub);
    analyticsToggle.setColour (juce::ToggleButton::tickColourId,         Colors::accent);
    analyticsToggle.setColour (juce::ToggleButton::tickDisabledColourId, Colors::border);
    analyticsToggle.onStateChange = [this]
    {
        freemiumManager.setAnalyticsEnabled (analyticsToggle.getToggleState());
    };

    addAndMakeVisible (linkLabel);
    linkLabel.setFont (juce::Font (11.f));
    linkLabel.setColour (juce::Label::textColourId, Colors::accentLt);
    // UTF-8 wrapper: a plain char* literal with "·" trips JUCE's ASCII assert and shows as
    // garbage characters in release builds.
    linkLabel.setText (juce::String (juce::CharPointer_UTF8 (
                           "Get a key:  platform.openai.com   \xc2\xb7   console.anthropic.com"
                           "   \xc2\xb7   console.groq.com/keys (free)")),
                       juce::dontSendNotification);
    linkLabel.setJustificationType (juce::Justification::centred);

    updateProviderModels();
    loadCurrentSettings();
}

SettingsComponent::~SettingsComponent()
{
    if (modelListThread != nullptr)
        modelListThread->stopThread (2000);
}

juce::String SettingsComponent::selectedProvider() const
{
    const auto id = providerCombo.getSelectedId();
    return id == 2 ? "anthropic" : id == 3 ? "groq" : "openai";
}

void SettingsComponent::updateProviderModels()
{
    modelCombo.clear (juce::dontSendNotification);
    modelStatus.setText ({}, juce::dontSendNotification);

    const auto cfg = ModelManifest::get().forProvider (selectedProvider());
    int itemId = 1;
    for (auto& m : cfg.suggested)
        modelCombo.addItem (m, itemId++);

    modelCombo.setSelectedId (1, juce::dontSendNotification);
}

void SettingsComponent::loadLiveModels()
{
    auto key = keyEditor.getText().trim();
    if (key.isEmpty())
    {
        modelStatus.setText ("Enter your API key first.", juce::dontSendNotification);
        return;
    }

    if (modelListThread != nullptr)
        modelListThread->stopThread (2000);

    loadModelsButton.setEnabled (false);
    modelStatus.setText ("Asking the provider...", juce::dontSendNotification);

    const auto provider = selectedProvider();
    juce::Component::SafePointer<SettingsComponent> safe (this);

    modelListThread = std::make_unique<ModelListThread> (provider, key,
        [safe, provider] (juce::StringArray ids, juce::String error)
        {
            if (safe == nullptr) return;
            auto& self = *safe;
            self.loadModelsButton.setEnabled (true);

            // Provider was switched while we waited: this list belongs to the old one.
            if (self.selectedProvider() != provider) return;

            if (error.isNotEmpty())
            {
                self.modelStatus.setText (error, juce::dontSendNotification);
                return;
            }

            const auto current = self.modelCombo.getText();
            self.updateProviderModels();
            const auto suggested = ModelManifest::get().forProvider (provider).suggested;

            int itemId = self.modelCombo.getNumItems() + 1;
            bool addedHeader = false;
            for (auto& id : ids)
            {
                if (suggested.contains (id)) continue;
                if (! addedHeader)
                {
                    self.modelCombo.addSeparator();
                    self.modelCombo.addSectionHeading ("All models on your key");
                    addedHeader = true;
                }
                self.modelCombo.addItem (id, itemId++);
            }

            if (current.isNotEmpty())
                self.modelCombo.setText (current, juce::dontSendNotification);
            self.modelStatus.setText (juce::String (ids.size()) + " models available on this key.",
                                      juce::dontSendNotification);
        });
    modelListThread->startThread();
}

void SettingsComponent::loadCurrentSettings()
{
    auto prov = freemiumManager.getProvider();
    providerCombo.setSelectedId (prov == "anthropic" ? 2 : prov == "groq" ? 3 : 1,
                                 juce::dontSendNotification);
    updateProviderModels();

    auto savedModel = freemiumManager.getModel();
    bool matchedSaved = false;
    for (int i = 0; i < modelCombo.getNumItems(); ++i)
        if (modelCombo.getItemText (i) == savedModel)
        {
            modelCombo.setSelectedId (modelCombo.getItemId (i), juce::dontSendNotification);
            matchedSaved = true;
        }
    // A custom (typed) model is not in the suggestion list. Keep showing it rather than
    // silently snapping back to the default, which would lose the user's choice.
    if (! matchedSaved && savedModel.isNotEmpty())
        modelCombo.setText (savedModel, juce::dontSendNotification);

    // Load key from secure store into editor (shows current stored value)
    if (freemiumManager.hasApiKey())
        keyEditor.setText (freemiumManager.getApiKey(), juce::dontSendNotification);

    analyticsToggle.setToggleState (freemiumManager.isAnalyticsEnabled(),
                                    juce::dontSendNotification);
}

void SettingsComponent::saveSettings()
{
    auto key   = keyEditor.getText().trim();
    const auto prov = selectedProvider();
    auto model = modelCombo.getText().trim();

    if (model.isEmpty())
        model = ModelManifest::get().forProvider (prov).defaultModel;

    if (! ModelManifest::isValidModelId (model))
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, "Model ID",
            "That model ID has characters no provider uses. Pick one from the list.");
        return;
    }

    // Typed (or kept) a model the provider has retired: save the replacement instead.
    auto replacement = ModelManifest::get().replacementFor (prov, model);
    if (replacement.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon, "Model updated",
            model + " has been retired by the provider, so " + replacement + " was saved instead.");
        model = replacement;
        modelCombo.setText (model, juce::dontSendNotification);
    }

    if (key.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Empty Key",
            "Enter an API key, or press Clear API Key to remove a saved one.");
        return;
    }

    freemiumManager.setApiKey (key, prov, model);

    if (onSettingsSaved) onSettingsSaved();

    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "Saved",
        "API key saved to system secure store. Switch to Chat to start.");
}

void SettingsComponent::clearSettings()
{
    freemiumManager.clearApiKey();
    keyEditor.clear();
    if (onSettingsSaved) onSettingsSaved();
}

void SettingsComponent::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bg);

    auto divY = getHeight() - 60;
    g.setColour (Colors::border);
    g.drawHorizontalLine (divY, 24.f, (float) getWidth() - 24.f);
}

void SettingsComponent::resized()
{
    auto area = getLocalBounds().reduced (32, 20);

    titleLabel.setBounds (area.removeFromTop (30));
    area.removeFromTop (18);

    auto row = [&] (int h) -> juce::Rectangle<int>
    {
        auto r = area.removeFromTop (h);
        area.removeFromTop (10);
        return r;
    };

    // Provider
    {
        auto r = row (26);
        provLabel.setBounds (r.removeFromLeft (80));
        providerCombo.setBounds (r.removeFromLeft (160));
    }
    // Model
    {
        auto r = row (26);
        modelLabel.setBounds (r.removeFromLeft (80));
        modelCombo.setBounds (r.removeFromLeft (220));
        r.removeFromLeft (8);
        loadModelsButton.setBounds (r.removeFromLeft (110));
        r.removeFromLeft (8);
        modelStatus.setBounds (r);
    }
    // API Key
    {
        auto r = row (26);
        keyLabel.setBounds (r.removeFromLeft (80));
        showKeyToggle.setBounds (r.removeFromRight (60));
        r.removeFromRight (4);
        keyEditor.setBounds (r);
    }
    // Buttons
    {
        auto r = row (32);
        saveButton.setBounds  (r.removeFromLeft (140));
        r.removeFromLeft (8);
        clearButton.setBounds (r.removeFromLeft (120));
    }
    // Analytics opt-out toggle
    {
        auto r = row (24);
        analyticsToggle.setBounds (r);
    }

    area.removeFromTop (12);
    linkLabel.setBounds (area.removeFromTop (16));
}
