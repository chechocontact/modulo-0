cmake_minimum_required(VERSION 3.25)

project(VintageMaker VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VM_BUILD_PLUGIN "Build the JUCE VST3 plugin (downloads JUCE)" ON)
option(VM_BUILD_TESTS "Build the JUCE-free Core tests" ON)
option(VM_ENABLE_SANITIZERS "Build Core and tests with ASan/UBSan (GCC/Clang only)" OFF)

# ---------------------------------------------------------------------------
# VintageCore: all DSP and signal flow, WITHOUT JUCE. Shared by plugin and tests.
# ---------------------------------------------------------------------------
add_library(VintageCore STATIC
    Source/Core/AudioBufferView.h
    Source/Core/DenormalGuard.h
    Source/Core/ModuleProcessor.h
    Source/Core/ParameterIds.h
    Source/Core/SmoothedValue.h
    Source/Core/ModuleSlot.h
    Source/Core/ModuleSlot.cpp
    Source/Core/AudioEngine.h
    Source/Core/AudioEngine.cpp
    Source/Core/MasterProcessor.h
    Source/Core/MasterProcessor.cpp
    Source/DSP/Test/TestProcessor.h
    Source/DSP/Test/TestProcessor.cpp
    Source/DSP/Texture/TextureParameters.h
    Source/Core/DelayLine.h
    Source/Core/NoiseGenerator.h
    Source/Core/VinylNoise.h
    Source/Core/VinylNoise.cpp
    Source/DSP/Texture/WowFlutter.h
    Source/DSP/Texture/WowFlutter.cpp
    Source/DSP/Texture/VinylDrop.h
    Source/DSP/Texture/DegradeProcessor.h
    Source/DSP/Texture/DegradeProcessor.cpp
    Source/DSP/Texture/TextureProcessor.h
    Source/DSP/Texture/TextureProcessor.cpp
    Source/DSP/Color/ColorParameters.h
    Source/DSP/Color/SaturationProcessor.h
    Source/DSP/Color/SaturationProcessor.cpp
    Source/DSP/Color/ToneProcessor.h
    Source/DSP/Color/ToneProcessor.cpp
    Source/DSP/Color/ChorusProcessor.h
    Source/DSP/Color/ChorusProcessor.cpp
    Source/DSP/Color/ColorProcessor.h
    Source/DSP/Color/ColorProcessor.cpp
)

target_include_directories(VintageCore PUBLIC Source)
set_target_properties(VintageCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(VintageCore PRIVATE /W4 /permissive-)
else()
    target_compile_options(VintageCore PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

if(VM_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(VintageCore PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(VintageCore PUBLIC -fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Tests (console, no JUCE)
# ---------------------------------------------------------------------------
if(VM_BUILD_TESTS)
    enable_testing()

    foreach(test_name CoreTests TextureTests ColorTests)
        add_executable(${test_name} Tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE VintageCore)

        if(MSVC)
            target_compile_options(${test_name} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()

# ---------------------------------------------------------------------------
# VST3 plugin
# ---------------------------------------------------------------------------
if(VM_BUILD_PLUGIN)
    include(FetchContent)

    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    TRUE
        GIT_CONFIG     core.longpaths=true)

    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(VintageMaker
        COMPANY_NAME             "Vintage Maker"
        PRODUCT_NAME             "Vintage Maker"
        BUNDLE_ID                com.vintagemaker.VintageMaker
        PLUGIN_MANUFACTURER_CODE Vmkr
        PLUGIN_CODE              Vmk1
        FORMATS                  VST3
        IS_SYNTH                 FALSE
        NEEDS_MIDI_INPUT         FALSE
        NEEDS_MIDI_OUTPUT        FALSE
        IS_MIDI_EFFECT           FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES          Fx
        COPY_PLUGIN_AFTER_BUILD  FALSE)

    target_sources(VintageMaker PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/Plugin/ParameterManager.h
        Source/Plugin/ParameterManager.cpp)

    target_compile_definitions(VintageMaker PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(VintageMaker
        PRIVATE
            VintageCore
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
