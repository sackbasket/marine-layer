cmake_minimum_required(VERSION 3.22)
include(plugin.cmake)
project(${PLUGIN_NAME} VERSION ${PLUGIN_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(PLUGIN_BUILD_TESTS "Build engine tests and the UI snapshot tool" OFF)
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")

include(FetchContent)
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- DSP engine: plain C++ (+ juce_dsp), testable without the plug-in wrapper ----
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS Source/dsp/*.cpp)
add_library(plugin_engine STATIC ${ENGINE_SOURCES})
target_include_directories(plugin_engine PUBLIC Source)
target_link_libraries(plugin_engine
    PUBLIC juce::juce_dsp
    PRIVATE juce::juce_recommended_config_flags)
target_compile_definitions(plugin_engine PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
set_target_properties(plugin_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- plug-in ----
juce_add_plugin(${PLUGIN_NAME}
    COMPANY_NAME "${PLUGIN_COMPANY}"
    BUNDLE_ID ${PLUGIN_BUNDLE_ID}
    PLUGIN_MANUFACTURER_CODE ${PLUGIN_MFR_CODE}
    PLUGIN_CODE ${PLUGIN_CODE}
    FORMATS VST3
    PRODUCT_NAME "${PLUGIN_NAME}"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES ${PLUGIN_CATEGORY})

file(GLOB PLUGIN_SOURCES CONFIGURE_DEPENDS Source/*.cpp Source/ui/*.cpp)
target_sources(${PLUGIN_NAME} PRIVATE ${PLUGIN_SOURCES})
target_compile_definitions(${PLUGIN_NAME} PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    PLUGIN_DISPLAY_NAME="${PLUGIN_DISPLAY}" PLUGIN_TAGLINE="${PLUGIN_TAGLINE}")
target_link_libraries(${PLUGIN_NAME}
    PRIVATE plugin_engine juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(PLUGIN_BUILD_TESTS)
    juce_add_console_app(EngineTests PRODUCT_NAME "EngineTests")
    target_sources(EngineTests PRIVATE Tests/EngineTests.cpp)
    target_link_libraries(EngineTests PRIVATE plugin_engine juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)
    target_compile_definitions(EngineTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

    juce_add_console_app(Snapshot PRODUCT_NAME "Snapshot")
    target_sources(Snapshot PRIVATE Tests/Snapshot.cpp)
    target_compile_definitions(Snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(Snapshot PRIVATE ${PLUGIN_NAME} juce::juce_audio_utils)
endif()
