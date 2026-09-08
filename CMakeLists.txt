cmake_minimum_required(VERSION 3.22)

project(ChronoOrbit VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Allow user to specify custom JUCE directory via -DJUCE_DIR=...
if(NOT TARGET juce::juce_audio_processors)
    if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}")
        add_subdirectory("${JUCE_DIR}" JUCE)
    elseif(EXISTS "/Users/mariosalvucci/Documents/Development/JUCE")
        add_subdirectory("/Users/mariosalvucci/Documents/Development/JUCE" JUCE)
    else()
        find_package(JUCE CONFIG REQUIRED)
    endif()
endif()

# Define the plugin target as a pure MIDI Effect / Generator
juce_add_plugin(ChronoOrbit
    COMPANY_NAME "Antigravity Audio"
    PLUGIN_MANUFACTURER_CODE Anti
    PLUGIN_CODE ChOr
    FORMATS VST3 Standalone
    PRODUCT_NAME "ChronoOrbit"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Instrument"
)

# Source files
target_sources(ChronoOrbit
    PRIVATE
        Source/RhythmEngine.h
        Source/RhythmEngine.cpp
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

# Compile definitions
target_compile_definitions(ChronoOrbit
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Include directories
target_include_directories(ChronoOrbit
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# Link JUCE Modules
target_link_libraries(ChronoOrbit
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
        juce::juce_data_structures
        juce::juce_audio_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
