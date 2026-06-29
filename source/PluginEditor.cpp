#include "PluginEditor.h"

namespace Colors
{
    extern const juce::Colour bg;
    extern const juce::Colour surface;
    extern const juce::Colour surface2;
    extern const juce::Colour accent;
    extern const juce::Colour accentLt;
    extern const juce::Colour textMain;
    extern const juce::Colour textSub;
    extern const juce::Colour border;
}

static constexpr int kEditorW  = 760;
static constexpr int kEditorH  = 500;
static constexpr int kHeaderH  =  48;

MusicaguideAudioProcessorEditor::MusicaguideAudioProcessorEditor (MusicaguideAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor  (p),
      chatPanel     (p.getFreemiumManager(), p.getLLMProvider()),
      settingsPanel (p.getFreemiumManager())
{
    setSize (kEditorW, kEditorH);

    // Logo label
    addAndMakeVisible (logoLabel);
    logoLabel.setText ("Musicaguide", juce::dontSendNotification);
    logoLabel.setFont (juce::Font (16.f, juce::Font::bold));
    logoLabel.setColour (juce::Label::textColourId, Colors::accentLt);
    logoLabel.setJustificationType (juce::Justification::centredLeft);

    // Tab buttons
    auto styleTab = [&] (juce::TextButton& btn, bool active)
    {
        btn.setColour (juce::TextButton::buttonColourId,
                       active ? Colors::accent : Colors::surface2);
        btn.setColour (juce::TextButton::buttonOnColourId, Colors::accent);
        btn.setColour (juce::TextButton::textColourOnId,  Colors::textMain);
        btn.setColour (juce::TextButton::textColourOffId, Colors::textSub);
    };

    addAndMakeVisible (chatTabBtn);
    styleTab (chatTabBtn, true);
    chatTabBtn.onClick = [this, styleTab] ()
    {
        showPanel (0);
        styleTab (chatTabBtn,     true);
        styleTab (settingsTabBtn, false);
    };

    addAndMakeVisible (settingsTabBtn);
    styleTab (settingsTabBtn, false);
    settingsTabBtn.onClick = [this, styleTab] ()
    {
        showPanel (1);
        styleTab (chatTabBtn,     false);
        styleTab (settingsTabBtn, true);
    };

    // Panels
    addAndMakeVisible (chatPanel);
    addChildComponent (settingsPanel);

    settingsPanel.onSettingsSaved = [this, styleTab] ()
    {
        showPanel (0);
        styleTab (chatTabBtn,     true);
        styleTab (settingsTabBtn, false);
        chatPanel.keyStateChanged();
    };

    chatPanel.addWelcomeMessage();
    showPanel (0);
}

MusicaguideAudioProcessorEditor::~MusicaguideAudioProcessorEditor() {}

void MusicaguideAudioProcessorEditor::showPanel (int index)
{
    activePanel = index;
    chatPanel.setVisible     (index == 0);
    settingsPanel.setVisible (index == 1);
}

void MusicaguideAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bg);

    // Header background
    g.setColour (Colors::surface);
    g.fillRect (0, 0, getWidth(), kHeaderH);

    // Bottom border of header
    g.setColour (Colors::border);
    g.drawHorizontalLine (kHeaderH, 0.f, (float) getWidth());
}

void MusicaguideAudioProcessorEditor::resized()
{
    auto header = getLocalBounds().removeFromTop (kHeaderH);
    auto body   = getLocalBounds().withTrimmedTop (kHeaderH);

    header.reduce (12, 8);
    logoLabel.setBounds     (header.removeFromLeft (130));
    header.removeFromLeft   (8);
    settingsTabBtn.setBounds (header.removeFromRight (80));
    header.removeFromRight  (4);
    chatTabBtn.setBounds    (header.removeFromRight (60));

    chatPanel.setBounds    (body);
    settingsPanel.setBounds (body);
}
