cmake_minimum_required(VERSION 3.22)
project(Musicaguide VERSION 0.1.0)

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
    source/FreemiumManager.cpp
    source/SecureStore.cpp
    source/Analytics.cpp
)

target_include_directories(Musicaguide PRIVATE source)

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

# Universal binary on Apple Silicon Macs + Security framework for Keychain
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
    target_link_libraries(Musicaguide PRIVATE "-framework Security")
endif()

# Windows Credential Manager libs
if(WIN32)
    target_link_libraries(Musicaguide PRIVATE Advapi32 Crypt32)
endif()

target_link_libraries(Musicaguide
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Generate JuceHeader.h (source files include <JuceHeader.h>)
juce_generate_juce_header(Musicaguide)
