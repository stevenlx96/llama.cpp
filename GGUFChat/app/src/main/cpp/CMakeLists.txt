cmake_minimum_required(VERSION 3.22.1)
project(llama-android)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "========================================")
message(STATUS "GGUFChat with OpenCL GPU Acceleration")
message(STATUS "========================================")

# ============================================================================
# Paths Setup
# ============================================================================

set(PREBUILT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "Prebuilt libs: ${PREBUILT_LIB_DIR}")
message(STATUS "Include dir: ${INCLUDE_DIR}")

# ============================================================================
# Verify Required Files
# ============================================================================

# Core libraries (required)
set(REQUIRED_LIBS
        "libggml-base.so"
        "libggml-cpu.so"
        "libggml.so"
        "libllama.so"
)

foreach(lib ${REQUIRED_LIBS})
    if(NOT EXISTS "${PREBUILT_LIB_DIR}/${lib}")
        message(FATAL_ERROR "ERROR: ${lib} not found at ${PREBUILT_LIB_DIR}/${lib}")
    endif()
    message(STATUS "Found ${lib}")
endforeach()

# Check for optional acceleration libraries
if(EXISTS "${PREBUILT_LIB_DIR}/libggml-opencl.so")
    message(STATUS "Found libggml-opencl.so (GPU acceleration)")
    set(HAS_OPENCL ON)
else()
    message(STATUS "libggml-opencl.so not found (GPU acceleration disabled)")
    set(HAS_OPENCL OFF)
endif()

# Note: Hexagon NPU is not supported in Android app sandbox (permission issues)
# Only OpenCL GPU acceleration is available

# ============================================================================
# Create JNI Wrapper
# ============================================================================

# Base source files
set(LLAMA_ANDROID_SOURCES
    llama-android-jni.cpp
)

# Check if ONNX Runtime library exists (optional feature)
set(ONNXRUNTIME_LIB "${PREBUILT_LIB_DIR}/libonnxruntime.so")
if(EXISTS "${ONNXRUNTIME_LIB}")
    message(STATUS "ONNX Runtime found - enabling intent recognition")
    list(APPEND LLAMA_ANDROID_SOURCES
        intent_jni.cpp
        intent_recognizer.cpp
    )
    set(ENABLE_INTENT_RECOGNITION ON)
else()
    message(STATUS "ONNX Runtime not found - intent recognition disabled")
    set(ENABLE_INTENT_RECOGNITION OFF)
endif()

add_library(llama-android SHARED ${LLAMA_ANDROID_SOURCES})

target_include_directories(llama-android PRIVATE "${INCLUDE_DIR}")

# ============================================================================
# Link System Libraries
# ============================================================================

# Core system libraries
target_link_libraries(llama-android PRIVATE
        android
        log
        m
        atomic
)

# EGL is required for OpenCL on Qualcomm Adreno GPUs
# Must be linked even if we don't use OpenCL directly
target_link_libraries(llama-android PRIVATE EGL)

# ============================================================================
# Import Prebuilt Libraries
# ============================================================================

# Note: We link to llama.so and ggml.so at compile time.
# Other backends (CPU, OpenCL, Hexagon) are loaded dynamically at runtime
# via ggml_backend_load_all_from_path()

# llama (main library)
add_library(llama_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(llama_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libllama.so"
)

# ggml (required for ggml_backend_* functions)
add_library(ggml_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml.so"
)

# ============================================================================
# Link Libraries
# ============================================================================

if(ENABLE_INTENT_RECOGNITION)
    target_link_libraries(llama-android PRIVATE
        llama_prebuilt
        ggml_prebuilt
        "${ONNXRUNTIME_LIB}"
    )
    target_compile_definitions(llama-android PRIVATE ENABLE_INTENT_RECOGNITION)
    message(STATUS "Intent recognition: ENABLED")
else()
    target_link_libraries(llama-android PRIVATE
        llama_prebuilt
        ggml_prebuilt
    )
    message(STATUS "Intent recognition: DISABLED")
endif()

# ============================================================================
# Compiler Options
# ============================================================================

target_compile_options(llama-android PRIVATE
        -O3
        -DNDEBUG
        -fPIC
        -fvisibility=default
        -Wall
        -Wextra
        -Wno-deprecated-declarations
)

# ============================================================================
# Linker Options
# ============================================================================

target_link_options(llama-android PRIVATE
        -Wl,--allow-shlib-undefined
        -Wl,-soname,libllama-android.so
        -Wl,--export-dynamic
)

message(STATUS "========================================")
message(STATUS "CMAKE CONFIGURATION COMPLETE")
if(HAS_OPENCL)
    message(STATUS "Acceleration: OpenCL GPU (Adreno)")
else()
    message(STATUS "Acceleration: CPU only")
endif()
message(STATUS "========================================")
