cmake_minimum_required(VERSION 3.15)
project(JuceTest VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)
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

juce_add_gui_app(JuceTest
    PRODUCT_NAME "JUCE Installation Test"
    BUNDLE_ID "com.yourcompany.jucetest"
    IS_COMMAND_LINE_APPLICATION FALSE
    ${JUCE_CURL_ARG}
)

target_sources(JuceTest PRIVATE main.cpp)

target_compile_definitions(JuceTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

if(UNIX AND NOT APPLE)
    target_compile_definitions(JuceTest PRIVATE JUCE_JACK=1)
endif()

target_link_libraries(JuceTest PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(UNIX)
    add_custom_command(TARGET JuceTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_FILE_NAME:JuceTest>"
            "$<TARGET_FILE_DIR:JuceTest>/JuceTest"
    )
endif()
