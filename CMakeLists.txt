cmake_minimum_required(VERSION 3.22)
project(DarkArp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DarkArp
    COMPANY_NAME "DarkArp"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Dkap
    PLUGIN_CODE Dka1
    FORMATS VST3
    PRODUCT_NAME "DarkArp")

juce_generate_juce_header(DarkArp)

target_sources(DarkArp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DarkArp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DarkArp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional headless DSP test (not built in CI):  cmake -B build -DDARKARP_BUILD_TESTS=ON
option(DARKARP_BUILD_TESTS "Build headless test" OFF)
if(DARKARP_BUILD_TESTS)
    juce_add_console_app(DarkArpTest PRODUCT_NAME "DarkArpTest")
    juce_generate_juce_header(DarkArpTest)
    target_sources(DarkArpTest PRIVATE Test/main.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(DarkArpTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(DarkArpTest PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
