cmake_minimum_required(VERSION 3.22)

project(TRoss110 VERSION 0.3.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# JUCE: use a local checkout if -DJUCE_DIR=... is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(T110_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(T110_FORMATS VST3 Standalone)
else()
    set(T110_FORMATS VST3 LV2 Standalone)
endif()

# Copy the built plug-ins into the user's plug-in folders (turned off on the build servers)
option(T110_COPY_PLUGINS "Copy plug-ins to the plug-in folders after building" ON)

juce_add_plugin(TRoss110
    COMPANY_NAME "Ticco Ross"
    PLUGIN_MANUFACTURER_CODE Tcro
    PLUGIN_CODE T110
    FORMATS ${T110_FORMATS}
    LV2URI "urn:ticcoross:tross110"
    PRODUCT_NAME "TRoss 110"
    BUNDLE_ID com.ticcoross.tross110
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Drum
    COPY_PLUGIN_AFTER_BUILD ${T110_COPY_PLUGINS})

target_sources(TRoss110 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MidiExport.h)

target_compile_definitions(TRoss110 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TRoss110
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: headless test app (renders the sequencer, checks state and recording, snapshots the editor)
option(T110_BUILD_TESTS "Build the plugin test app" OFF)
if(T110_BUILD_TESTS)
    juce_add_console_app(TRoss110Test PRODUCT_NAME "TRoss110Test")
    target_sources(TRoss110Test PRIVATE tools/plugin_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(TRoss110Test PRIVATE
        JucePlugin_Name="TRoss 110" JucePlugin_VersionString="${PROJECT_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TRoss110Test PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
