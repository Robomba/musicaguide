#pragma once
#include <JuceHeader.h>

// OS-level secure credential storage for the API key.
// macOS:   Keychain Services (kSecClassGenericPassword)
// Windows: Credential Manager (CRED_TYPE_GENERIC)
// Linux:   user-local file with 0600 permissions (no OS keyring)
class SecureStore
{
public:
    static bool         setKey   (const juce::String& value);
    static juce::String getKey   ();
    static bool         clearKey ();
};
