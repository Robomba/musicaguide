#pragma once
#include <JuceHeader.h>
#include "FreemiumManager.h"

class SettingsComponent : public juce::Component
{
public:
    explicit SettingsComponent (FreemiumManager& fm);
    ~SettingsComponent() override;

    void resized() override;
    void paint  (juce::Graphics& g) override;

    std::function<void()> onSettingsSaved;

private:
    void saveSettings();
    void clearSettings();
    void updateProviderModels();
    void loadCurrentSettings();
    void loadLiveModels();
    juce::String selectedProvider() const;

    FreemiumManager& freemiumManager;

    juce::Label   titleLabel     { {}, "API Key Setup" };
    juce::Label   provLabel      { {}, "Provider" };
    juce::ComboBox providerCombo;
    juce::Label   modelLabel     { {}, "Model" };
    juce::ComboBox modelCombo;
    juce::TextButton loadModelsButton { "Load models" };
    juce::Label   modelStatus;
    std::unique_ptr<juce::Thread> modelListThread;
    juce::Label   keyLabel       { {}, "API Key" };
    juce::TextEditor keyEditor;
    juce::ToggleButton showKeyToggle { "Show" };
    juce::TextButton saveButton   { "Save Settings" };
    juce::TextButton clearButton  { "Clear API Key" };

    juce::ToggleButton analyticsToggle { "Share anonymous usage data to help improve Musicaguide" };

    juce::Label  linkLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsComponent)
};
