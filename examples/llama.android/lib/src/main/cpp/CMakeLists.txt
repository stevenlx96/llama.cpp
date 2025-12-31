cmake_minimum_required(VERSION 3.31.6)

project("ai-chat" VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED true)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}"   CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)

# --------------------------------------------------------------------------
# AI Chat library
# --------------------------------------------------------------------------

if(DEFINED ANDROID_ABI)
    message(STATUS "Detected Android ABI: ${ANDROID_ABI}")
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(GGML_SYSTEM_ARCH "ARM")
        set(GGML_CPU_KLEIDIAI OFF)
        set(GGML_OPENMP ON)
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(GGML_SYSTEM_ARCH "x86")
        set(GGML_CPU_KLEIDIAI OFF)
        set(GGML_OPENMP OFF)
    else()
        message(FATAL_ERROR "Unsupported ABI: ${ANDROID_ABI}")
    endif()
endif()

# Detect Ninja for Vulkan shader compilation on Windows
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CROSSCOMPILING)
    # Try multiple paths to find Ninja
    set(NINJA_SEARCH_PATHS "")

    if (DEFINED ENV{ANDROID_HOME})
        list(APPEND NINJA_SEARCH_PATHS "$ENV{ANDROID_HOME}/cmake/3.31.6/bin/ninja.exe")
    endif()
    if (DEFINED ENV{ANDROID_SDK_ROOT})
        list(APPEND NINJA_SEARCH_PATHS "$ENV{ANDROID_SDK_ROOT}/cmake/3.31.6/bin/ninja.exe")
    endif()
    if (DEFINED ANDROID_NDK)
        get_filename_component(SDK_ROOT "${ANDROID_NDK}/.." ABSOLUTE)
        list(APPEND NINJA_SEARCH_PATHS "${SDK_ROOT}/cmake/3.31.6/bin/ninja.exe")
    endif()

    # Common installation paths as fallback
    list(APPEND NINJA_SEARCH_PATHS
        "C:/Android/Sdk/cmake/3.31.6/bin/ninja.exe"
        "E:/android/android_sdk/cmake/3.31.6/bin/ninja.exe"
    )

    foreach(NINJA_PATH IN LISTS NINJA_SEARCH_PATHS)
        if (EXISTS "${NINJA_PATH}")
            set(CMAKE_MAKE_PROGRAM "${NINJA_PATH}" CACHE FILEPATH "Ninja executable for shader compilation" FORCE)
            message(STATUS "Found Ninja for shader compilation: ${NINJA_PATH}")
            break()
        endif()
    endforeach()

    if (NOT CMAKE_MAKE_PROGRAM)
        message(WARNING "Ninja not found in any searched paths. Shader compilation may fail.")
    endif()
endif()

set(LLAMA_SRC ${CMAKE_CURRENT_LIST_DIR}/../../../../../../)
add_subdirectory(${LLAMA_SRC} build-llama)

add_library(${CMAKE_PROJECT_NAME} SHARED
        ai_chat.cpp)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        GGML_SYSTEM_ARCH=${GGML_SYSTEM_ARCH}
        GGML_CPU_KLEIDIAI=$<BOOL:${GGML_CPU_KLEIDIAI}>
        GGML_OPENMP=$<BOOL:${GGML_OPENMP}>
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${LLAMA_SRC}
        ${LLAMA_SRC}/common
        ${LLAMA_SRC}/include
        ${LLAMA_SRC}/ggml/include
        ${LLAMA_SRC}/ggml/src)

target_link_libraries(${CMAKE_PROJECT_NAME}
        llama
        common
        android
        log)
