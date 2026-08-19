#include "SettingsComponent.h"
#include "UIColors.h"

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
    linkLabel.setText ("Get a key:  platform.openai.com   ·   console.anthropic.com"
                       "   ·   console.groq.com/keys (free)",
                       juce::dontSendNotification);
    linkLabel.setJustificationType (juce::Justification::centred);

    updateProviderModels();
    loadCurrentSettings();
}

void SettingsComponent::updateProviderModels()
{
    modelCombo.clear (juce::dontSendNotification);
    const auto prov = providerCombo.getSelectedId();

    if (prov == 2)
    {
        modelCombo.addItem ("claude-haiku-4-5-20251001", 1);
        modelCombo.addItem ("claude-sonnet-5",           2);
        modelCombo.addItem ("claude-opus-5",             3);
    }
    else if (prov == 3)
    {
        // Groq's free tier. Verified against console.groq.com/docs/models (2026-08-18).
        // Groq retired every Llama chat model from production, which is exactly the kind
        // of rotation the editable box above exists to absorb.
        // 120b first: the free plan gives it the SAME limits as 20b (30 RPM / 8K TPM),
        // so the stronger model costs nothing extra - and music theory is symbolic
        // reasoning, where small models produce confidently wrong note spellings.
        modelCombo.addItem ("openai/gpt-oss-120b", 1);   // default: strongest open-weight
        modelCombo.addItem ("openai/gpt-oss-20b",  2);   // faster, weaker
        modelCombo.addItem ("groq/compound",       3);   // agentic w/ web search (70K TPM)
    }
    else
    {
        modelCombo.addItem ("gpt-4o-mini", 1);
        modelCombo.addItem ("gpt-4o",      2);
    }
    modelCombo.setSelectedId (1, juce::dontSendNotification);
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
            modelCombo.setSelectedId (i + 1, juce::dontSendNotification);
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
    const auto provId = providerCombo.getSelectedId();
    const char* prov = provId == 2 ? "anthropic" : provId == 3 ? "groq" : "openai";
    auto model = modelCombo.getText();

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
