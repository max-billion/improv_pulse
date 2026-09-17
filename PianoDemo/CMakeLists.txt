cmake_minimum_required(VERSION 3.15)
project(PianoDemo VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

# Reuse already-fetched JUCE 8.0.6 source if present locally
set(LOCAL_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JuceTest/build/_deps/juce-src")
if(EXISTS "${LOCAL_JUCE_DIR}/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${LOCAL_JUCE_DIR}" CACHE PATH "Local JUCE source" FORCE)
endif()

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
)
FetchContent_MakeAvailable(JUCE)

if(UNIX AND NOT APPLE)
    set(JUCE_CURL_ARG NEEDS_CURL TRUE)
else()
    set(JUCE_CURL_ARG "")
endif()

juce_add_gui_app(PianoDemo
    PRODUCT_NAME "PianoDemo"
    BUNDLE_ID "com.improvpulse.pianodemo"
    IS_COMMAND_LINE_APPLICATION FALSE
    ${JUCE_CURL_ARG}
)

target_sources(PianoDemo PRIVATE main.cpp)

target_compile_definitions(PianoDemo PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

if(UNIX AND NOT APPLE)
    target_compile_definitions(PianoDemo PRIVATE JUCE_JACK=1)
endif()

target_link_libraries(PianoDemo PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
