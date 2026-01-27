cmake_minimum_required(VERSION 3.22.1)
project(llama-android)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "========================================")
message(STATUS "CMAKE BUILD START (with Intent Recognition support)")
message(STATUS "========================================")

# ============================================================================
# SECTION 1: Paths Setup
# ============================================================================

set(PREBUILT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "Prebuilt libs: ${PREBUILT_LIB_DIR}")
message(STATUS "Include dir: ${INCLUDE_DIR}")
message(STATUS "Source dir: ${CMAKE_CURRENT_SOURCE_DIR}")

# ============================================================================
# SECTION 2: Verify All Required Files Exist
# ============================================================================

# Check C++ source file
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/llama-android-jni.cpp")
    message(FATAL_ERROR "ERROR: llama-android-jni.cpp not found at ${CMAKE_CURRENT_SOURCE_DIR}/llama-android-jni.cpp")
endif()
message(STATUS "✓ Found llama-android-jni.cpp")

# Check include directory
if(NOT EXISTS "${INCLUDE_DIR}")
    message(WARNING "Include directory not found at ${INCLUDE_DIR}")
endif()

# Check prebuilt libraries
set(REQUIRED_LIBS
        "libggml-base.so"
        "libggml-cpu.so"
        "libggml-vulkan.so"
        "libggml.so"
        "libllama.so"
)

foreach(lib ${REQUIRED_LIBS})
    if(NOT EXISTS "${PREBUILT_LIB_DIR}/${lib}")
        message(FATAL_ERROR "ERROR: ${lib} not found at ${PREBUILT_LIB_DIR}/${lib}")
    endif()
    message(STATUS "✓ Found ${lib}")
endforeach()

message(STATUS "All required files verified!")

# ============================================================================
# SECTION 3: Check for ONNX Runtime (Optional - for Intent Recognition)
# ============================================================================

set(ONNXRUNTIME_LIB "${PREBUILT_LIB_DIR}/libonnxruntime.so")
if(EXISTS "${ONNXRUNTIME_LIB}")
    message(STATUS "✓ ONNX Runtime found - enabling intent recognition")
    set(ENABLE_INTENT_RECOGNITION ON)
else()
    message(STATUS "⚠ ONNX Runtime not found - intent recognition disabled")
    message(STATUS "  To enable intent recognition:")
    message(STATUS "    1. Download ONNX Runtime Android AAR (v1.17.0) from:")
    message(STATUS "       https://repo1.maven.org/maven2/com/microsoft/onnxruntime/onnxruntime-android/1.17.0/")
    message(STATUS "    2. Extract libonnxruntime.so for ${ANDROID_ABI} and place it in:")
    message(STATUS "       ${PREBUILT_LIB_DIR}/")
    set(ENABLE_INTENT_RECOGNITION OFF)
endif()

# ============================================================================
# SECTION 4: Create JNI Wrapper Library
# ============================================================================

# Base source files
set(LLAMA_ANDROID_SOURCES
    "llama-android-jni.cpp"
)

# Add intent recognition sources if enabled
if(ENABLE_INTENT_RECOGNITION)
    list(APPEND LLAMA_ANDROID_SOURCES
        "intent_jni.cpp"
        "intent_recognizer.cpp"
    )
    message(STATUS "Added intent recognition sources: intent_jni.cpp, intent_recognizer.cpp")
endif()

add_library(llama-android SHARED ${LLAMA_ANDROID_SOURCES})

message(STATUS "Created llama-android target")

# ============================================================================
# SECTION 5: Configure Include Paths
# ============================================================================

target_include_directories(llama-android PRIVATE "${INCLUDE_DIR}")
message(STATUS "Added include directory: ${INCLUDE_DIR}")

# ============================================================================
# SECTION 6: Link System Libraries
# ============================================================================

target_link_libraries(llama-android PRIVATE
        android
        log
        m
        atomic
)

message(STATUS "Linked system libraries: android, log, m, atomic")

# ============================================================================
# SECTION 7: Import Prebuilt Libraries with Dependencies
# ============================================================================

# ggml-base (no dependencies)
add_library(ggml_base_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_base_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-base.so"
)
message(STATUS "Imported ggml_base_prebuilt")

# ggml-cpu (depends on ggml-base)
add_library(ggml_cpu_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_cpu_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-cpu.so"
        INTERFACE_LINK_LIBRARIES ggml_base_prebuilt
)
message(STATUS "Imported ggml_cpu_prebuilt (depends on ggml-base)")

# ggml-vulkan (depends on ggml-base)
add_library(ggml_vulkan_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_vulkan_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-vulkan.so"
        INTERFACE_LINK_LIBRARIES ggml_base_prebuilt
)
message(STATUS "Imported ggml_vulkan_prebuilt (depends on ggml-base)")

# ggml (depends on ggml-base, ggml-cpu, and ggml-vulkan)
add_library(ggml_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml.so"
        INTERFACE_LINK_LIBRARIES "ggml_base_prebuilt;ggml_cpu_prebuilt;ggml_vulkan_prebuilt"
)
message(STATUS "Imported ggml_prebuilt (depends on ggml-base, ggml-cpu, ggml-vulkan)")

# llama (depends on all ggml libraries)
add_library(llama_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(llama_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libllama.so"
        INTERFACE_LINK_LIBRARIES "ggml_prebuilt;ggml_cpu_prebuilt;ggml_vulkan_prebuilt;ggml_base_prebuilt"
)
message(STATUS "Imported llama_prebuilt (depends on all ggml libraries)")

# ============================================================================
# SECTION 8: Link Prebuilt Libraries to JNI Wrapper
# ============================================================================

if(ENABLE_INTENT_RECOGNITION)
    # Link with ONNX Runtime for intent recognition
    target_link_libraries(llama-android PRIVATE
        llama_prebuilt
        "${ONNXRUNTIME_LIB}"
    )
    target_compile_definitions(llama-android PRIVATE ENABLE_INTENT_RECOGNITION)
    message(STATUS "Linked llama_prebuilt and ONNX Runtime to llama-android")
else()
    target_link_libraries(llama-android PRIVATE llama_prebuilt)
    message(STATUS "Linked llama_prebuilt to llama-android (with transitive dependencies)")
endif()

# ============================================================================
# SECTION 9: Compiler Options
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
message(STATUS "Set compiler options: -O3, -DNDEBUG, -fPIC, -fvisibility=default")

# ============================================================================
# SECTION 10: Linker Options
# ============================================================================

target_link_options(llama-android PRIVATE
        -Wl,--allow-shlib-undefined
        -Wl,-soname,libllama-android.so
        -Wl,--export-dynamic
)
message(STATUS "Set linker options: --allow-shlib-undefined, -soname")

# ============================================================================
# SECTION 11: Debug Symbols
# ============================================================================

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(llama-android PRIVATE -g)
    message(STATUS "Debug build: symbols included")
else()
    target_compile_options(llama-android PRIVATE -g)
    message(STATUS "Release build: symbols included for JNI")
endif()

# ============================================================================
# FINAL SUMMARY
# ============================================================================

message(STATUS "========================================")
message(STATUS "✓ CMAKE CONFIGURATION COMPLETE")
message(STATUS "========================================")
message(STATUS "Target: llama-android")
message(STATUS "Type: SHARED library (JNI wrapper)")
message(STATUS "Sources: llama-android-jni.cpp")
if(ENABLE_INTENT_RECOGNITION)
    message(STATUS "         intent_jni.cpp, intent_recognizer.cpp")
    message(STATUS "Intent Recognition: ENABLED")
else()
    message(STATUS "Intent Recognition: DISABLED")
endif()
message(STATUS "Include: ${INCLUDE_DIR}")
message(STATUS "Dependencies: llama + ggml libraries")
if(ENABLE_INTENT_RECOGNITION)
    message(STATUS "              + ONNX Runtime (for intent)")
endif()
message(STATUS "Output: libllama-android.so")
message(STATUS "========================================")
