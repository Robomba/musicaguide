cmake_minimum_required(VERSION 3.22)

# These MUST be set before project() and before any target exists.
# CMAKE_OSX_ARCHITECTURES is read when a target is created, so setting it further
# down the file only took effect on a SECOND configure over a warm cache. Every cold
# configure - CI, a fresh clone, a wiped build dir - silently produced an arm64-only
# binary that will not load on Intel Macs, while the cache still read "x86_64;arm64".
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
endif()

project(Musicaguide VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch JUCE ──────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Plugin target ────────────────────────────────────────────────────────────
juce_add_plugin(Musicaguide
    COMPANY_NAME                "Musicaguide"
    COMPANY_WEBSITE             "https://musicaguide.ai"
    PLUGIN_MANUFACTURER_CODE    Magu
    PLUGIN_CODE                 Mgai
    FORMATS                     VST3 AU Standalone
    VST3_CATEGORIES             "Fx"
    PRODUCT_NAME                "Musicaguide"
    BUNDLE_ID                   "ai.musicaguide.plugin"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

target_sources(Musicaguide PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/ChatComponent.cpp
    source/SettingsComponent.cpp
    source/LLMProvider.cpp
    source/ModelManifest.cpp
    source/FreemiumManager.cpp
    source/SecureStore.cpp
    source/Analytics.cpp
)

target_include_directories(Musicaguide PRIVATE source)

# Compiled-in snapshot of the model manifest (offline fallback). The live copy is
# https://www.leosolproducts.com/musicaguide/models.json - keep them in step.
juce_add_binary_data(MusicaguideData SOURCES resources/models.json)

target_compile_definitions(Musicaguide PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

# Linux needs libcurl for HTTPS
if(UNIX AND NOT APPLE)
    target_compile_definitions(Musicaguide PUBLIC JUCE_USE_CURL=1)
    find_package(CURL REQUIRED)
    target_link_libraries(Musicaguide PRIVATE CURL::libcurl)
endif()

# Security framework for Keychain. (Architecture/deployment target are set at the top
# of this file, before project() - they have no effect once targets exist.)
if(APPLE)
    target_link_libraries(Musicaguide PRIVATE "-framework Security")
endif()

# Windows Credential Manager libs
if(WIN32)
    target_link_libraries(Musicaguide PRIVATE Advapi32 Crypt32)
endif()

target_link_libraries(Musicaguide
    PRIVATE
        MusicaguideData
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Generate JuceHeader.h (source files include <JuceHeader.h>)
juce_generate_juce_header(Musicaguide)

# ── Unit tests (off by default; CI turns them on) ─────────────────────────────
option(MUSICAGUIDE_TESTS "Build the MusicaguideTests console app" OFF)
if(MUSICAGUIDE_TESTS)
    juce_add_console_app(MusicaguideTests PRODUCT_NAME "MusicaguideTests")
    target_sources(MusicaguideTests PRIVATE
        tests/WireTests.cpp
        source/ModelManifest.cpp
        source/LLMProvider.cpp)
    target_include_directories(MusicaguideTests PRIVATE source)
    target_compile_definitions(MusicaguideTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MusicaguideTests PRIVATE
        MusicaguideData
        juce::juce_core
        juce::juce_events
        juce::juce_data_structures
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    juce_generate_juce_header(MusicaguideTests)
endif()
