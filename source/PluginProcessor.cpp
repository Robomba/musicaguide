#include "PluginProcessor.h"
#include "PluginEditor.h"

MusicaguideAudioProcessor::MusicaguideAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{}

MusicaguideAudioProcessor::~MusicaguideAudioProcessor() {}

bool MusicaguideAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void MusicaguideAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    // Pass audio through unmodified
    juce::ScopedNoDenormals noDenormals;
    (void) buffer;
}

juce::AudioProcessorEditor* MusicaguideAudioProcessor::createEditor()
{
    return new MusicaguideAudioProcessorEditor (*this);
}

void MusicaguideAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::MemoryOutputStream stream (dest, true);
    stream.writeString ("musicaguide-v1");
}

void MusicaguideAudioProcessor::setStateInformation (const void*, int) {}

// Required by JUCE plugin client
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MusicaguideAudioProcessor();
}
