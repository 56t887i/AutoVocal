
cmake_minimum_required(VERSION 3.22)

project(AutoVocal VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(AutoVocal
    COMPANY_NAME "Digital Studio"
    BUNDLE_ID "com.digitalstudio.autovocal"
    PLUGIN_MANUFACTURER_CODE DgSt
    PLUGIN_CODE AtV1
    PRODUCT_NAME "AUTO VOCAL"

    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(AutoVocal
    PRIVATE
        Source/PluginEntry.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/DynamicsProcessor.cpp
        Source/DSP/PitchCorrector.cpp
        Source/DSP/NoiseReducer.cpp
        Source/DSP/VocalCharacter.cpp
        Source/DSP/SpaceProcessor.cpp
)

juce_generate_juce_header(AutoVocal)

target_compile_definitions(AutoVocal
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AutoVocal
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(AutoVocal PRIVATE /utf-8)
endif()
