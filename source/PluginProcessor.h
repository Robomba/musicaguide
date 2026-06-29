#pragma once
#include <JuceHeader.h>
#include "FreemiumManager.h"
#include "LLMProvider.h"

class MusicaguideAudioProcessor : public juce::AudioProcessor
{
public:
    MusicaguideAudioProcessor();
    ~MusicaguideAudioProcessor() override;

    void prepareToPlay  (double, int) override {}
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& l) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool  acceptsMidi()   const override { return false; }
    bool  producesMidi()  const override { return false; }
    bool  isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    FreemiumManager& getFreemiumManager() { return freemiumManager; }
    LLMProvider&     getLLMProvider()     { return llmProvider; }

private:
    FreemiumManager freemiumManager;
    LLMProvider     llmProvider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicaguideAudioProcessor)
};
