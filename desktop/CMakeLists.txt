# SPDX-License-Identifier: GPL-3.0-only
# EPICYKLOS for the computer (para el ordenador): the firmware's sound core as a VST3 / AU plugin and a standalone app.
# The core is the same code the FM-1 runs (app/dsp); only this wrapper and its interface are new.
# JUCE is fetched at build time; it is used under its AGPLv3 option, which combines with GPL-3.0 (see LICENSING.md).
cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
project(EpicyklosDesktop VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Epicyklos
    COMPANY_NAME "Cyklos"
    PRODUCT_NAME "EPICYKLOS"
    BUNDLE_ID "com.cyklos.epicyklos"
    PLUGIN_MANUFACTURER_CODE Cykl
    PLUGIN_CODE Epik
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(Epicyklos PRIVATE
    src/Processor.cpp
    src/Editor.cpp
    ../app/dsp/engine.cpp
    ../app/dsp/factory.cpp)
target_include_directories(Epicyklos PRIVATE src ../app/dsp ../base/include)
target_compile_definitions(Epicyklos PUBLIC
    PLAT_HOST=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Epicyklos
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# A tool for the build machine: the window drawn into a PNG, to see it without a Mac (captura de la ventana)
juce_add_console_app(EpkShot PRODUCT_NAME "EpkShot")
target_sources(EpkShot PRIVATE
    src/Shot.cpp
    src/Processor.cpp
    src/Editor.cpp
    ../app/dsp/engine.cpp
    ../app/dsp/factory.cpp)
target_include_directories(EpkShot PRIVATE src ../app/dsp ../base/include)
target_compile_definitions(EpkShot PRIVATE
    PLAT_HOST=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(EpkShot
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
