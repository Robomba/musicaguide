# Musicaguide VST3

AI music guide plugin (VST3 + Standalone). Freemium: 10 free messages per session; bring your own API key to go unlimited.

---

## Dependencies

| Platform | Required |
|----------|----------|
| All      | CMake ≥ 3.22, Git, C++17 compiler |
| macOS    | Xcode 14+ (Command Line Tools) |
| Windows  | Visual Studio 2022 (Desktop C++ workload) |
| Linux    | `gcc`/`clang`, `libcurl-dev`, `libasound2-dev`, `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libfreetype-dev` |

JUCE 8.0.4 is fetched automatically via CMake FetchContent — no manual download needed.

---

## Models (why updates don't need a new build)

Model IDs are not hardcoded. They come from `models.json`:

- **Live copy:** https://www.leosolproducts.com/musicaguide/models.json (repo `leosolproducts-site`, folder `musicaguide/`). The plugin fetches it in the background at launch and caches it in the user's app-data folder.
- **Offline fallback:** `resources/models.json`, compiled in. Keep it in step with the live copy when you cut a release.
- The manifest sets each provider's default, fallback, dropdown suggestions, retired-model replacements, which token-limit field to send, the output size, and whether to send temperature. It can never change an endpoint or a header, so it can't redirect anyone's API key.
- A saved model on the retired list is repointed on launch. If a provider rejects a model as gone, the plugin retries once on the fallback and saves it.
- **Settings > Load models** lists every model the user's own key can use right now, straight from the provider.
- A weekly GitHub Action in `leosolproducts-site` checks the providers and opens a pull request when `models.json` needs an update.

Tests: `cmake -B build -DMUSICAGUIDE_TESTS=ON && cmake --build build --target MusicaguideTests && ./build/MusicaguideTests_artefacts/Debug/MusicaguideTests`

---

## Build

```bash
# 1. Clone (or copy) the project
git clone <your-repo> musicaguide && cd musicaguide/from-claude

# 2. Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build --config Release --parallel
```

macOS produces a universal binary (arm64 + x86_64, deployment target 10.15) automatically.

---

## Where the built plugin lands

After a successful build:

```
build/Musicaguide_artefacts/Release/VST3/Musicaguide.vst3   # VST3 bundle
build/Musicaguide_artefacts/Release/Standalone/Musicaguide  # Standalone app
```

To install the VST3, copy `Musicaguide.vst3` to your DAW's VST3 scan folder:

| Platform | Default VST3 folder |
|----------|---------------------|
| macOS    | `~/Library/Audio/Plug-Ins/VST3/` |
| Windows  | `C:\Program Files\Common Files\VST3\` |
| Linux    | `~/.vst3/` or `/usr/lib/vst3/` |

---

## Entering your API key (Settings panel)

1. Open the plugin in your DAW (or run the Standalone).
2. Click the **Settings** tab (gear icon, top-right).
3. Choose **Provider**: `openai` or `anthropic`.
4. Choose **Model** from the dropdown (e.g. `gpt-4o`, `claude-opus-4-5`).
5. Paste your API key into the **API Key** field.
6. Click **Save Settings**.

The key is stored in JUCE's persistent `ApplicationProperties` (OS keychain-equivalent location); it persists across sessions. Use **Clear API Key** to remove it.

Without a key, you get **10 free messages per session** using a built-in shared key (rate-limited). After that, the chat is locked until you supply your own key.
