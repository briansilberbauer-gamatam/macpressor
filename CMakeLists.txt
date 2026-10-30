cmake_minimum_required(VERSION 3.22)

project(MacPressor VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The local Linux dependency sysroot is a build-time convenience only. Do not
# bake its absolute paths into distributed binaries.
set(CMAKE_SKIP_RPATH TRUE)

option(MACPRESSOR_BUILD_TESTS "Build the offline DSP test executable" ON)
option(MACPRESSOR_BUILD_SCREENSHOT "Build the headless editor renderer" ON)
option(MACPRESSOR_BUILD_HOST_TEST "Build the VST3 host smoke test" ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(MacPressorAssets
    SOURCES
        Resources/LatoRegular.ttf
        Resources/LatoSemibold.ttf
        Resources/LatoBlack.ttf
        Resources/Lato-OFL.txt)

set(MACPRESSOR_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/Version.h
    Source/dsp/AutoCompressor.h
    Source/dsp/VoiceProfiles.h
    Source/ui/Theme.cpp
    Source/ui/Theme.h
    Source/ui/StudioLookAndFeel.cpp
    Source/ui/StudioLookAndFeel.h
    Source/ui/Controls.cpp
    Source/ui/Controls.h
    Source/ui/EnginePanel.cpp
    Source/ui/EnginePanel.h
    Source/ui/WaveformDisplay.cpp
    Source/ui/WaveformDisplay.h)

set(MACPRESSOR_DEFINITIONS
    MACPRESSOR_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

juce_add_plugin(MacPressor
    COMPANY_NAME "BLS Audio"
    BUNDLE_ID "com.blsaudio.macpressor"
    PLUGIN_MANUFACTURER_CODE Blsa
    PLUGIN_CODE Mcpr
    FORMATS VST3 Standalone
    PRODUCT_NAME "MacPressor"
    DESCRIPTION "Four-voice automatic compressor with a single source switch"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx")

target_sources(MacPressor PRIVATE ${MACPRESSOR_SOURCES})
target_include_directories(MacPressor PRIVATE Source)
target_compile_definitions(MacPressor PUBLIC ${MACPRESSOR_DEFINITIONS})

target_link_libraries(MacPressor
    PRIVATE
        MacPressorAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(MACPRESSOR_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(MacPressorTests PRODUCT_NAME "MacPressorTests")
    target_sources(MacPressorTests PRIVATE Tests/DspTests.cpp)
    target_include_directories(MacPressorTests PRIVATE Source)
    target_compile_definitions(MacPressorTests PRIVATE ${MACPRESSOR_DEFINITIONS})
    target_link_libraries(MacPressorTests
        PRIVATE
            juce::juce_audio_basics
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME macpressor_dsp COMMAND $<TARGET_FILE:MacPressorTests>)

    # End-to-end processor tests: host buffer layouts, channel counts, block
    # sizes, host bypass and the waveform telemetry the editor reads. These are
    # platform-agnostic regressions for behaviour that only shows up in real
    # hosts (notably on Windows), so they run on every CI platform.
    juce_add_console_app(MacPressorProcessorTests PRODUCT_NAME "MacPressorProcessorTests")

    target_sources(MacPressorProcessorTests PRIVATE ${MACPRESSOR_SOURCES} Tests/ProcessorTests.cpp)
    target_include_directories(MacPressorProcessorTests PRIVATE Source)
    target_compile_definitions(MacPressorProcessorTests PRIVATE ${MACPRESSOR_DEFINITIONS})
    target_link_libraries(MacPressorProcessorTests
        PRIVATE
            MacPressorAssets
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME macpressor_processor COMMAND $<TARGET_FILE:MacPressorProcessorTests>)
endif()

if(MACPRESSOR_BUILD_HOST_TEST)
    juce_add_console_app(MacPressorHostSmokeTest PRODUCT_NAME "MacPressorHostSmokeTest")

    target_sources(MacPressorHostSmokeTest PRIVATE Tools/HostSmokeTest.cpp)
    target_compile_definitions(MacPressorHostSmokeTest PRIVATE
        ${MACPRESSOR_DEFINITIONS}
        JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(MacPressorHostSmokeTest
        PRIVATE
            juce::juce_audio_utils
            juce::juce_gui_extra
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_dependencies(MacPressorHostSmokeTest MacPressor_VST3)
endif()

if(MACPRESSOR_BUILD_SCREENSHOT)
    juce_add_gui_app(MacPressorScreenshot
        PRODUCT_NAME "MacPressorScreenshot"
        COMPANY_NAME "BLS Audio"
        BUNDLE_ID "com.blsaudio.macpressor.screenshot")

    target_sources(MacPressorScreenshot PRIVATE ${MACPRESSOR_SOURCES} Tools/RenderEditor.cpp)
    target_include_directories(MacPressorScreenshot PRIVATE Source)
    target_compile_definitions(MacPressorScreenshot PRIVATE ${MACPRESSOR_DEFINITIONS})

    target_link_libraries(MacPressorScreenshot
        PRIVATE
            MacPressorAssets
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
