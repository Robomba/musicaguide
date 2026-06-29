# Shipping Musicaguide VST3

---

## 1. Build release artifacts

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Artifacts: `build/Musicaguide_artefacts/Release/`

---

## 2. macOS: codesign + notarize

Apple requires both for Gatekeeper to accept third-party plugins.

### Codesign the bundle

```bash
codesign --force --deep --strict \
  --options runtime \
  --sign "Developer ID Application: YOUR NAME (TEAMID)" \
  build/Musicaguide_artefacts/Release/VST3/Musicaguide.vst3
```

`--options runtime` is required for notarization.

### Zip for notarization

```bash
ditto -c -k --keepParent \
  build/Musicaguide_artefacts/Release/VST3/Musicaguide.vst3 \
  Musicaguide-mac.zip
```

### Submit to Apple Notary Service

```bash
xcrun notarytool submit Musicaguide-mac.zip \
  --apple-id "you@example.com" \
  --team-id  "TEAMID" \
  --password "@keychain:AC_PASSWORD" \
  --wait
```

### Staple the ticket

```bash
xcrun stapler staple \
  build/Musicaguide_artefacts/Release/VST3/Musicaguide.vst3
```

Verify: `spctl -a -vvv -t install Musicaguide.vst3`

---

## 3. Windows: optional codesign

Sign with an EV or OV certificate via `signtool` (Microsoft WHQL not required for VST3):

```powershell
signtool sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 `
  /a "Musicaguide.vst3\Contents\x86_64-win\Musicaguide.vst3"
```

---

## 4. Linux

No codesign step. Build on the oldest supported distro (Ubuntu 22.04 LTS recommended) for broad glibc compatibility.

---

## 5. Package for distribution

### macOS `.zip`

```
Musicaguide-mac-1.0.0.zip
└── Musicaguide.vst3/          (stapled, signed)
```

### Windows `.zip`

```
Musicaguide-win-1.0.0.zip
└── Musicaguide.vst3/
    └── Contents/
        └── x86_64-win/
            └── Musicaguide.vst3
```

### Linux `.tar.gz`

```
Musicaguide-linux-1.0.0.tar.gz
└── Musicaguide.vst3/
```

Include a one-page install note pointing to the correct VST3 folder per OS (see README).

---

## 6. Freemium / BYOK flow for end users

| Scenario | What happens |
|----------|--------------|
| First install, no key | User gets **10 free messages per session** (shared key, rate-limited) |
| Free quota exhausted | Chat locks; banner prompts user to go to Settings and enter their key |
| User enters their key | Key saved persistently; unlimited use, all costs billed to their account |
| User clears key | Returns to free tier (quota resets each session) |

**No server infrastructure required.** The plugin calls the LLM APIs directly from the user's machine using their key. You never see or store user keys.

### Supported providers at launch

| Provider | Example models |
|----------|---------------|
| OpenAI   | `gpt-4o`, `gpt-4o-mini` |
| Anthropic | `claude-opus-4-5`, `claude-sonnet-4-5`, `claude-haiku-4-5` |

To add providers later, extend `LLMProvider.cpp` and add entries to the `providerCombo` in `SettingsComponent`.
