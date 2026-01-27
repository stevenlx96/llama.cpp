cmake_minimum_required(VERSION 3.14)
project(intent_jni)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android NDK setup
if(ANDROID)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
endif()

# Find ONNX Runtime for Android
# You need to download ONNX Runtime Android AAR and extract it
# Or build from source for Android

find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS
        ${ONNXRUNTIME_ROOT_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/onnxruntime/include
    PATH_SUFFIXES onnxruntime core/session
)

find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS
        ${ONNXRUNTIME_ROOT_DIR}/lib/${ANDROID_ABI}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/onnxruntime/lib/${ANDROID_ABI}
)

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
    message(WARNING "ONNX Runtime not found for Android")
    message(WARNING "  ONNXRUNTIME_INCLUDE_DIR: ${ONNXRUNTIME_INCLUDE_DIR}")
    message(WARNING "  ONNXRUNTIME_LIBRARY: ${ONNXRUNTIME_LIBRARY}")
    message(WARNING "")
    message(WARNING "To build for Android:")
    message(WARNING "  1. Download ONNX Runtime Android AAR from:")
    message(WARNING "     https://github.com/microsoft/onnxruntime/releases")
    message(WARNING "  2. Extract and set ONNXRUNTIME_ROOT_DIR in your build.gradle")
    return()
endif()

message(STATUS "Found ONNX Runtime for Android:")
message(STATUS "  Include dir: ${ONNXRUNTIME_INCLUDE_DIR}")
message(STATUS "  Library: ${ONNXRUNTIME_LIBRARY}")
message(STATUS "  ABI: ${ANDROID_ABI}")

# Include intent_recognizer source
set(INTENT_RECOGNIZER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Create JNI library
add_library(intent_jni SHARED
    intent_jni.cpp
    ${INTENT_RECOGNIZER_DIR}/src/intent_recognizer.cpp
)

target_include_directories(intent_jni PRIVATE
    ${INTENT_RECOGNIZER_DIR}/include
    ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(intent_jni
    ${ONNXRUNTIME_LIBRARY}
    android
    log
)

# Copy ONNX Runtime shared library to output
if(ANDROID)
    add_custom_command(TARGET intent_jni POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_LIBRARY}
        $<TARGET_FILE_DIR:intent_jni>/libonnxruntime.so
    )
endif()
