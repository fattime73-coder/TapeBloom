cmake_minimum_required(VERSION 3.22)
project(TapeBloom VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
add_executable(tape_tests tests/CoreTests.cpp)
add_test(NAME tape_core COMMAND tape_tests)
option(TAPEBLOOM_CORE_ONLY "Build DSP tests without JUCE" OFF)
if(TAPEBLOOM_CORE_ONLY)
  return()
endif()
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
endif()
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
set(formats Standalone)
if(APPLE)
  list(APPEND formats AU)
endif()
juce_add_plugin(TapeBloom
 COMPANY_NAME "TapeBloom Audio" BUNDLE_ID "audio.tapebloom.instrument"
 PLUGIN_MANUFACTURER_CODE TbAu PLUGIN_CODE TpBl
 FORMATS ${formats} PRODUCT_NAME "TapeBloom"
 IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
 AU_MAIN_TYPE kAudioUnitType_MusicEffect
 MICROPHONE_PERMISSION_ENABLED TRUE
 MICROPHONE_PERMISSION_TEXT "TapeBloom records your instrument to create a playable tape loop."
 COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(TapeBloom)
target_sources(TapeBloom PRIVATE Source/Processor.cpp Source/Editor.cpp)
target_compile_definitions(TapeBloom PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TapeBloom PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
