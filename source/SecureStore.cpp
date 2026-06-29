// include Apple Security framework BEFORE JUCE so Carbon MacTypes Point does not clash with juce::Point (via guard)
#if defined(__APPLE__)
 #include <Security/Security.h>
#endif
#include "SecureStore.h"

// ─────────────────────────────────────────────────────────────────────────────
//  macOS — Keychain Services
// ─────────────────────────────────────────────────────────────────────────────
#if JUCE_MAC

#include <Security/Security.h>

static constexpr const char* kService = "ai.musicaguide.plugin";
static constexpr const char* kAccount = "apiKey";

bool SecureStore::setKey (const juce::String& value)
{
    const char* utf8 = value.toRawUTF8();
    CFIndex     len  = static_cast<CFIndex> (strlen (utf8));

    CFDataRef dataRef = CFDataCreate (kCFAllocatorDefault,
                                      reinterpret_cast<const UInt8*> (utf8), len);
    if (dataRef == nullptr)
        return false;

    // Query to locate any existing item
    const void* qKeys[]   = { kSecClass,                  kSecAttrService,        kSecAttrAccount };
    const void* qValues[] = { kSecClassGenericPassword,   CFSTR("ai.musicaguide.plugin"), CFSTR("apiKey") };
    CFDictionaryRef query = CFDictionaryCreate (kCFAllocatorDefault, qKeys, qValues, 3,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);

    // Try to update an existing item first (upsert pattern)
    const void* uKeys[]   = { kSecValueData };
    const void* uValues[] = { dataRef };
    CFDictionaryRef attrs = CFDictionaryCreate (kCFAllocatorDefault, uKeys, uValues, 1,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
    OSStatus status = SecItemUpdate (query, attrs);
    CFRelease (attrs);

    if (status == errSecItemNotFound)
    {
        // No existing item — add a new one
        const void* aKeys[] = {
            kSecClass,               kSecAttrService,               kSecAttrAccount,
            kSecValueData,           kSecAttrAccessible
        };
        const void* aValues[] = {
            kSecClassGenericPassword, CFSTR("ai.musicaguide.plugin"), CFSTR("apiKey"),
            dataRef,                  kSecAttrAccessibleAfterFirstUnlock
        };
        CFDictionaryRef addDict = CFDictionaryCreate (kCFAllocatorDefault, aKeys, aValues, 5,
                                                      &kCFTypeDictionaryKeyCallBacks,
                                                      &kCFTypeDictionaryValueCallBacks);
        status = SecItemAdd (addDict, nullptr);
        CFRelease (addDict);
    }

    CFRelease (query);
    CFRelease (dataRef);
    return status == errSecSuccess;
}

juce::String SecureStore::getKey()
{
    const void* keys[] = {
        kSecClass,                  kSecAttrService,               kSecAttrAccount,
        kSecReturnData,             kSecMatchLimit
    };
    const void* values[] = {
        kSecClassGenericPassword,   CFSTR("ai.musicaguide.plugin"), CFSTR("apiKey"),
        kCFBooleanTrue,             kSecMatchLimitOne
    };
    CFDictionaryRef query = CFDictionaryCreate (kCFAllocatorDefault, keys, values, 5,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);

    CFTypeRef result = nullptr;
    OSStatus  status = SecItemCopyMatching (query, &result);
    CFRelease (query);

    if (status != errSecSuccess || result == nullptr)
        return {};

    auto* data = reinterpret_cast<CFDataRef> (result);
    juce::String key (reinterpret_cast<const char*> (CFDataGetBytePtr (data)),
                      static_cast<int> (CFDataGetLength (data)));
    CFRelease (result);
    return key;
}

bool SecureStore::clearKey()
{
    const void* keys[]   = { kSecClass,                  kSecAttrService,               kSecAttrAccount };
    const void* values[] = { kSecClassGenericPassword,   CFSTR("ai.musicaguide.plugin"), CFSTR("apiKey") };
    CFDictionaryRef query = CFDictionaryCreate (kCFAllocatorDefault, keys, values, 3,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
    OSStatus status = SecItemDelete (query);
    CFRelease (query);
    return status == errSecSuccess || status == errSecItemNotFound;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Windows — Credential Manager
// ─────────────────────────────────────────────────────────────────────────────
#elif JUCE_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincred.h>

static const wchar_t* kTarget = L"Musicaguide/apiKey";

bool SecureStore::setKey (const juce::String& value)
{
    const char* utf8     = value.toRawUTF8();
    DWORD       blobSize = static_cast<DWORD> (value.getNumBytesAsUTF8());

    CREDENTIALW cred          = {};
    cred.Type                 = CRED_TYPE_GENERIC;
    cred.TargetName           = const_cast<LPWSTR> (kTarget);
    cred.CredentialBlobSize   = blobSize;
    cred.CredentialBlob       = const_cast<LPBYTE> (reinterpret_cast<const BYTE*> (utf8));
    cred.Persist              = CRED_PERSIST_LOCAL_MACHINE;

    return CredWriteW (&cred, 0) != FALSE;
}

juce::String SecureStore::getKey()
{
    PCREDENTIALW pCred = nullptr;
    if (!CredReadW (kTarget, CRED_TYPE_GENERIC, 0, &pCred))
        return {};

    juce::String key = juce::String::fromUTF8 (
        reinterpret_cast<const char*> (pCred->CredentialBlob),
        static_cast<int> (pCred->CredentialBlobSize));
    CredFree (pCred);
    return key;
}

bool SecureStore::clearKey()
{
    BOOL ok = CredDeleteW (kTarget, CRED_TYPE_GENERIC, 0);
    return ok != FALSE || GetLastError() == ERROR_NOT_FOUND;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Linux / other — file-based fallback (no OS keyring)
// ─────────────────────────────────────────────────────────────────────────────
#else

#include <sys/stat.h>

static juce::File keyFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("musicaguide")
               .getChildFile (".apikey");
}

bool SecureStore::setKey (const juce::String& value)
{
    auto f = keyFile();
    f.getParentDirectory().createDirectory();
    if (!f.replaceWithText (value))
        return false;
    chmod (f.getFullPathName().toRawUTF8(), 0600);
    return true;
}

juce::String SecureStore::getKey()
{
    auto f = keyFile();
    return f.existsAsFile() ? f.loadFileAsString().trim() : juce::String{};
}

bool SecureStore::clearKey()
{
    auto f = keyFile();
    return !f.existsAsFile() || f.deleteFile();
}

#endif
