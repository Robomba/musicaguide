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
