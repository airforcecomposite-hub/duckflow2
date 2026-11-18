cmake_minimum_required(VERSION 3.22)

project(DuckFlow2 VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(DUCKFLOW_USE_JUCE "Build full resizable GUI VST3/AU/Standalone plugin via JUCE 8" ON)
option(DUCKFLOW_BUILD_NATIVE_MINIMAL_VST3 "Build zero-dependency native C++ VST3 binary" OFF)

if(MSVC)
    add_compile_options(/W4 /fp:fast /MP /utf-8)
else()
    add_compile_options(-Wall -Wextra -O3)
endif()

# ==============================================================================
# Primary Target: Full JUCE 8 VST3 / AU / Standalone with Custom Resizable GUI
# ==============================================================================
if(DUCKFLOW_USE_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    set(PLUGIN_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND PLUGIN_FORMATS AU)
    endif()

    juce_add_plugin(DuckFlow2
        COMPANY_NAME                "DuckFlow Audio"
        COMPANY_WEBSITE             "https://github.com/duckflow-audio/duckflow2"
        COMPANY_EMAIL               "support@duckflow.audio"
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        PLUGIN_MANUFACTURER_CODE    DkFl
        PLUGIN_CODE                 DkF2
        FORMATS                     ${PLUGIN_FORMATS}
        PRODUCT_NAME                "DuckFlow 2"
        VST3_CATEGORIES             "Fx" "Dynamics" "Modulation"
    )

    target_sources(DuckFlow2
        PRIVATE
            Source/PluginProcessor.cpp
            Source/PluginProcessor.h
            Source/PluginEditor.cpp
            Source/PluginEditor.h
            Source/DSP/SimdAndMath.h
            Source/DSP/MultiSegmentEnvelope.h
            Source/DSP/MultibandCrossover.h
            Source/DSP/TriggerDetector.h
            Source/DSP/EnvelopeTracer.h
            Source/DSP/SyncGroupHub.h
            Source/DSP/FactoryPresets.h
            Source/DSP/DuckFlowEngine.h
    )

    target_include_directories(DuckFlow2 PRIVATE Source)

    target_compile_definitions(DuckFlow2
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
    )

    target_link_libraries(DuckFlow2
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_audio_formats
            juce::juce_gui_basics
            juce::juce_graphics
            juce::juce_core
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
    )
endif()

# ==============================================================================
# Optional Target: Zero-Dependency Native VST3 Binary (No external SDK required)
# ==============================================================================
if(DUCKFLOW_BUILD_NATIVE_MINIMAL_VST3)
    add_library(DuckFlow2NativeVST3 SHARED NativeVST3/DuckFlow2VST3.cpp)
    target_include_directories(DuckFlow2NativeVST3 PRIVATE Source)
    set_target_properties(DuckFlow2NativeVST3 PROPERTIES PREFIX "" OUTPUT_NAME "DuckFlow2")
endif()
