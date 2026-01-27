cmake_minimum_required(VERSION 3.14)
project(intent_recognition)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ONNX Runtime
# You can specify ONNXRUNTIME_ROOT_DIR to help CMake find it
# Example: cmake -DONNXRUNTIME_ROOT_DIR=/path/to/onnxruntime ..

find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS
        ${ONNXRUNTIME_ROOT_DIR}/include
        /usr/include/onnxruntime
        /usr/local/include/onnxruntime
        $ENV{HOME}/.local/include/onnxruntime
    PATH_SUFFIXES onnxruntime core/session
)

find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS
        ${ONNXRUNTIME_ROOT_DIR}/lib
        /usr/lib
        /usr/local/lib
        $ENV{HOME}/.local/lib
)

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
    message(WARNING "ONNX Runtime not found. Please install it or set ONNXRUNTIME_ROOT_DIR")
    message(WARNING "  ONNXRUNTIME_INCLUDE_DIR: ${ONNXRUNTIME_INCLUDE_DIR}")
    message(WARNING "  ONNXRUNTIME_LIBRARY: ${ONNXRUNTIME_LIBRARY}")
    message(WARNING "")
    message(WARNING "To install ONNX Runtime:")
    message(WARNING "  1. Download from: https://github.com/microsoft/onnxruntime/releases")
    message(WARNING "  2. Extract and set: cmake -DONNXRUNTIME_ROOT_DIR=/path/to/onnxruntime ..")
    message(WARNING "  3. Or install system-wide: sudo apt install libonnxruntime-dev (Linux)")
    return()
endif()

message(STATUS "Found ONNX Runtime:")
message(STATUS "  Include dir: ${ONNXRUNTIME_INCLUDE_DIR}")
message(STATUS "  Library: ${ONNXRUNTIME_LIBRARY}")

# Find nlohmann/json (for JSON parsing)
find_package(nlohmann_json 3.2.0 QUIET)

if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found, will try to use system includes")
    # Try to find json.hpp in common locations
    find_path(NLOHMANN_JSON_INCLUDE_DIR
        NAMES nlohmann/json.hpp
        PATHS
            /usr/include
            /usr/local/include
            $ENV{HOME}/.local/include
    )

    if(NOT NLOHMANN_JSON_INCLUDE_DIR)
        message(WARNING "nlohmann/json not found. Please install it:")
        message(WARNING "  Ubuntu/Debian: sudo apt install nlohmann-json3-dev")
        message(WARNING "  Or download json.hpp from: https://github.com/nlohmann/json/releases")
        return()
    endif()
    message(STATUS "Found nlohmann/json: ${NLOHMANN_JSON_INCLUDE_DIR}")
endif()

# Library
add_library(intent_recognizer
    src/intent_recognizer.cpp
)

target_include_directories(intent_recognizer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ONNXRUNTIME_INCLUDE_DIR}
)

if(nlohmann_json_FOUND)
    target_link_libraries(intent_recognizer PUBLIC
        ${ONNXRUNTIME_LIBRARY}
        nlohmann_json::nlohmann_json
    )
else()
    target_include_directories(intent_recognizer PUBLIC
        ${NLOHMANN_JSON_INCLUDE_DIR}
    )
    target_link_libraries(intent_recognizer PUBLIC
        ${ONNXRUNTIME_LIBRARY}
    )
endif()

# Executable
add_executable(intent-recognition
    src/main.cpp
)

target_link_libraries(intent-recognition PRIVATE
    intent_recognizer
)

# Installation
install(TARGETS intent-recognition DESTINATION bin)
install(TARGETS intent_recognizer DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/intent_recognition)

# Print build info
message(STATUS "===========================================")
message(STATUS "Intent Recognition Example Configuration")
message(STATUS "===========================================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "ONNX Runtime: ${ONNXRUNTIME_LIBRARY}")
message(STATUS "===========================================")
