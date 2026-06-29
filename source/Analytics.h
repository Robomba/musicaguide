#pragma once
#include <JuceHeader.h>

class Analytics
{
public:
    // Fire-and-forget POST to Supabase. Safe to call from the message thread.
    // Does nothing if installId is empty (should never happen, but guard anyway).
    static void logEvent (const juce::String& installId,
                          const juce::String& model,
                          const juce::String& prompt,
                          const juce::String& response);
};
