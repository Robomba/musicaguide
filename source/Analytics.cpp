#include "Analytics.h"

static constexpr const char* kUrl    = "https://ajqftuxksnbwnwtfavys.supabase.co/rest/v1/usage_events";
static constexpr const char* kApiKey = "sb_publishable_GibZ9ljm4rZqE0fjKT81vw_yjoGZypA";

static juce::String getPlatform()
{
   #if JUCE_MAC
    return "macOS";
   #elif JUCE_WINDOWS
    return "Windows";
   #else
    return "Other";
   #endif
}

void Analytics::logEvent (const juce::String& installId,
                           const juce::String& model,
                           const juce::String& prompt,
                           const juce::String& response)
{
    if (installId.isEmpty())
        return;

    // Build JSON body on the calling thread before handing off
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("install_id",  installId);
    root->setProperty ("app_version", juce::String (ProjectInfo::versionString));
    root->setProperty ("platform",    getPlatform());
    root->setProperty ("model",       model);
    root->setProperty ("prompt",      prompt);
    root->setProperty ("response",    response);
    juce::String body = juce::JSON::toString (juce::var (root.get()), false);

    // juce::Thread::launch starts a detached background thread — never the audio thread,
    // never blocks the caller, self-cleans up when the lambda returns.
    juce::Thread::launch ([body]
    {
        try
        {
            juce::URL url (kUrl);
            url = url.withPOSTData (body);

            const juce::String headers =
                juce::String ("apikey: ")       + kApiKey + "\r\n" +
                "Authorization: Bearer "         + kApiKey + "\r\n" +
                "Content-Type: application/json\r\n"
                "Prefer: return=minimal";

            auto stream = url.createInputStream (
                juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders (headers)
                    .withConnectionTimeoutMs (10000));

            // Read to completion so the connection closes cleanly; discard the body.
            if (stream != nullptr)
                stream->readEntireStreamAsString();
        }
        catch (...) {}
    });
}
