cmake_minimum_required(VERSION 3.22.1)
project(llama-android)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "========================================")
message(STATUS "CMAKE BUILD WITH PREBUILT LIBS (matching Docker flags)")
message(STATUS "========================================")

# ============================================================================
# CRITICAL: Match Docker compilation flags EXACTLY!
# Reference: docs/backend/hexagon/CMakeUserPresets.json
# ============================================================================

# CRITICAL: Use SAME flags as Docker build to ensure ABI compatibility!
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8.7a+fp16 -fvectorize -ffp-model=fast -fno-finite-math-only -D_GNU_SOURCE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8.7a+fp16 -fvectorize -ffp-model=fast -fno-finite-math-only -D_GNU_SOURCE")

message(STATUS "C FLAGS: ${CMAKE_C_FLAGS}")
message(STATUS "CXX FLAGS: ${CMAKE_CXX_FLAGS}")

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
        "libggml-hexagon.so"
        "libggml-htp-v73.so"
        "libggml-htp-v75.so"
        "libggml-htp-v79.so"
        "libggml-htp-v81.so"
        "libggml.so"
        "libllama.so"
)

foreach(lib ${REQUIRED_LIBS})
    if(NOT EXISTS "${PREBUILT_LIB_DIR}/${lib}")
        message(FATAL_ERROR "ERROR: ${lib} not found at ${PREBUILT_LIB_DIR}/${lib}")
    endif()
    message(STATUS "✓ Found ${lib}")
endforeach()

# ============================================================================
# Create JNI Wrapper with MATCHING FLAGS
# ============================================================================

add_library(llama-android SHARED llama-android-jni.cpp)

target_include_directories(llama-android PRIVATE "${INCLUDE_DIR}")

# Link system libraries
target_link_libraries(llama-android PRIVATE
        android
        log
        m
        atomic
)

# ============================================================================
# Import Prebuilt Libraries
# ============================================================================

# ggml-base
add_library(ggml_base_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_base_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-base.so"
)

# ggml-cpu
add_library(ggml_cpu_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_cpu_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-cpu.so"
        INTERFACE_LINK_LIBRARIES ggml_base_prebuilt
)

# ggml-hexagon
add_library(ggml_hexagon_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_hexagon_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-hexagon.so"
        INTERFACE_LINK_LIBRARIES ggml_base_prebuilt
)

# ggml
add_library(ggml_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml.so"
        INTERFACE_LINK_LIBRARIES "ggml_base_prebuilt;ggml_cpu_prebuilt;ggml_hexagon_prebuilt"
)

# llama
add_library(llama_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(llama_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libllama.so"
        INTERFACE_LINK_LIBRARIES "ggml_prebuilt;ggml_cpu_prebuilt;ggml_hexagon_prebuilt;ggml_base_prebuilt"
)

target_link_libraries(llama-android PRIVATE llama_prebuilt)

# ============================================================================
# Compiler Options - MATCH DOCKER!
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
message(STATUS "✓ CMAKE CONFIGURATION COMPLETE")
message(STATUS "Using prebuilt .so with MATCHING Docker flags")
message(STATUS "========================================")
