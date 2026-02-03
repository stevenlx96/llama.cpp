#!/bin/bash

# Build script for llama.cpp Android with OpenCL only (no Hexagon)
# This removes the libggml-hexagon.so dependency

set -e

# ============================================================================
# Configuration - MODIFY THESE PATHS FOR YOUR SYSTEM
# ============================================================================

# Set your Android NDK path
if [ -z "$ANDROID_NDK_HOME" ]; then
    # Try common paths
    if [ -d "$HOME/Android/Sdk/ndk/26.1.10909125" ]; then
        export ANDROID_NDK_HOME="$HOME/Android/Sdk/ndk/26.1.10909125"
    elif [ -d "$HOME/Android/Sdk/ndk/25.2.9519653" ]; then
        export ANDROID_NDK_HOME="$HOME/Android/Sdk/ndk/25.2.9519653"
    else
        echo "ERROR: ANDROID_NDK_HOME not set. Please set it to your NDK path."
        echo "Example: export ANDROID_NDK_HOME=\$HOME/Android/Sdk/ndk/26.1.10909125"
        exit 1
    fi
fi

echo "Using NDK: $ANDROID_NDK_HOME"

# ============================================================================
# Build Configuration
# ============================================================================

BUILD_DIR="build-android-opencl"
ABI="arm64-v8a"
ANDROID_PLATFORM="android-28"

# Output directory for the .so files
OUTPUT_DIR="GGUFChat/app/src/main/jniLibs/${ABI}"

# ============================================================================
# Clean and Create Build Directory
# ============================================================================

echo "========================================"
echo "Building llama.cpp for Android"
echo "  - OpenCL: ENABLED"
echo "  - Hexagon: DISABLED"
echo "========================================"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# ============================================================================
# CMake Configuration
# ============================================================================

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DGGML_OPENMP=OFF \
    -DGGML_OPENCL=ON \
    -DGGML_OPENCL_USE_ADRENO_KERNELS=ON \
    -DGGML_HEXAGON=OFF \
    -DGGML_VULKAN=OFF \
    -DGGML_CUDA=OFF \
    -DGGML_METAL=OFF \
    -DLLAMA_BUILD_EXAMPLES=OFF \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_SERVER=OFF

# ============================================================================
# Build
# ============================================================================

echo "Building..."
cmake --build . --config Release -j$(nproc)

# ============================================================================
# Copy Libraries
# ============================================================================

cd ..

echo "========================================"
echo "Copying libraries to $OUTPUT_DIR"
echo "========================================"

mkdir -p "$OUTPUT_DIR"

# Core libraries (required)
LIBS_TO_COPY=(
    "libggml-base.so"
    "libggml-cpu.so"
    "libggml-opencl.so"
    "libggml.so"
    "libllama.so"
)

for lib in "${LIBS_TO_COPY[@]}"; do
    found=$(find "$BUILD_DIR" -name "$lib" -type f 2>/dev/null | head -1)
    if [ -n "$found" ]; then
        echo "  Copying $lib"
        cp "$found" "$OUTPUT_DIR/"
    else
        echo "  WARNING: $lib not found!"
    fi
done

echo "========================================"
echo "Build complete!"
echo "========================================"
echo ""
echo "Libraries copied to: $OUTPUT_DIR"
echo ""
echo "The following libraries are included:"
ls -la "$OUTPUT_DIR"/*.so 2>/dev/null || echo "No .so files found"
echo ""
echo "NOTE: libggml-hexagon.so is NOT included (Hexagon disabled)"
echo ""
echo "Now rebuild your Android app in Android Studio."
