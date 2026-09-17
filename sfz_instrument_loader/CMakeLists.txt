cmake_minimum_required(VERSION 3.15)
project(sfz_instrument_loader VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
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

juce_add_console_app(sfz_instrument_loader
    PRODUCT_NAME "sfz_instrument_loader"
)

target_sources(sfz_instrument_loader PRIVATE
    main.cpp
    SfzInstrumentLoader.cpp
)

target_compile_definitions(sfz_instrument_loader PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(sfz_instrument_loader PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
