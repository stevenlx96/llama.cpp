cmake_minimum_required(VERSION 3.22.1)
project(llama-android)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "========================================")
message(STATUS "CMAKE BUILD START (AAR Library)")
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

# ============================================================================
# Create JNI Wrapper
# ============================================================================

add_library(llama-android SHARED llama-android-jni.cpp)

target_include_directories(llama-android PRIVATE "${INCLUDE_DIR}")

# ============================================================================
# Link System Libraries
# ============================================================================

target_link_libraries(llama-android PRIVATE
        android
        log
        m
        atomic
)

# ============================================================================
# Import Prebuilt Libraries
# ============================================================================

add_library(llama_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(llama_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libllama.so"
)

add_library(ggml_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml.so"
)

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(llama-android PRIVATE llama_prebuilt ggml_prebuilt)

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
message(STATUS "========================================")
