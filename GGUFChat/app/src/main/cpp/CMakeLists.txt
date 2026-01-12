cmake_minimum_required(VERSION 3.22.1)
project(llama-android)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "========================================")
message(STATUS "CMAKE BUILD FROM SOURCE (like official)")
message(STATUS "========================================")

# ============================================================================
# CRITICAL: Compile from source like official llama.android example!
# Reference: examples/llama.android/lib/src/main/cpp/CMakeLists.txt
# ============================================================================

# Point to llama.cpp source root (4 levels up from app/src/main/cpp/)
set(LLAMA_SRC ${CMAKE_CURRENT_LIST_DIR}/../../../../)
message(STATUS "LLAMA_SRC: ${LLAMA_SRC}")

# Set architecture for ARM64
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(GGML_SYSTEM_ARCH "ARM")
    message(STATUS "Architecture: ARM (arm64-v8a)")
endif()

# Enable Hexagon NPU support
set(GGML_HEXAGON ON CACHE BOOL "Enable Hexagon backend" FORCE)
message(STATUS "GGML_HEXAGON: ON")

# Add llama.cpp as subdirectory - this compiles everything!
add_subdirectory(${LLAMA_SRC} build-llama EXCLUDE_FROM_ALL)

# Create JNI wrapper library
add_library(${CMAKE_PROJECT_NAME} SHARED
        llama-android-jni.cpp)

message(STATUS "Created llama-android JNI wrapper")

# Link to llama libraries (compiled from source)
target_link_libraries(${CMAKE_PROJECT_NAME}
        llama
        android
        log)

message(STATUS "Linked to llama library (from source)")

# Set compiler options
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        -O3
        -fPIC
        -Wall
        -Wextra)

message(STATUS "========================================")
message(STATUS "✓ CMAKE CONFIGURATION COMPLETE")
message(STATUS "Building from SOURCE (official method)")
message(STATUS "========================================")
