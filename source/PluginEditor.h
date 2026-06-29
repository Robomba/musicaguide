#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ChatComponent.h"
#include "SettingsComponent.h"

class MusicaguideAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MusicaguideAudioProcessorEditor (MusicaguideAudioProcessor&);
    ~MusicaguideAudioProcessorEditor() override;

    void paint  (juce::Graphics& g) override;
    void resized() override;

private:
    void showPanel (int index);   // 0 = chat, 1 = settings

    MusicaguideAudioProcessor& processor;

    // Header
    juce::Label     logoLabel;
    juce::TextButton chatTabBtn    { "Chat" };
    juce::TextButton settingsTabBtn{ "Settings" };

    // Panels
    ChatComponent     chatPanel;
    SettingsComponent settingsPanel;

    int activePanel = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicaguideAudioProcessorEditor)
};
