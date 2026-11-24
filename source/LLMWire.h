#pragma once
#include <JuceHeader.h>
#include "LLMProvider.h"
#include "ModelManifest.h"

// Pure request/response helpers for the three providers, split out so they can be
// unit-tested without a network or a plugin host (see tests/WireTests.cpp).
namespace LLMWire
{
    juce::String buildOpenAIRequest    (const juce::String& model,
                                        const ModelManifest::ProviderConfig& cfg,
                                        const juce::Array<LLMProvider::Message>& messages);
    juce::String buildAnthropicRequest (const juce::String& model,
                                        const ModelManifest::ProviderConfig& cfg,
                                        const juce::Array<LLMProvider::Message>& messages);

    juce::String apiErrorMessage (const juce::var& parsed);
    juce::String apiErrorCode    (const juce::var& parsed);

    // True when the provider says the model is gone or unusable (retry on the fallback);
    // false for bad keys, rate limits and outages.
    bool isModelUnavailableError (int status, const juce::var& parsed);

    juce::String extractOpenAIContent    (const juce::var& parsed, bool& hitLengthLimit);
    juce::String extractAnthropicContent (const juce::var& parsed, bool& hitLengthLimit);
}
