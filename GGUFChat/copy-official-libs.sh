#!/bin/bash
# Copy llama.cpp libraries to GGUFChat project (CPU only)

set -e

# Source directory (official pkg-adb)
SRC_LIB="../pkg-adb/llama.cpp/lib"

# Destination directories
APP_LIB="app/src/main/jniLibs/arm64-v8a"
AAR_LIB="llama-android/src/main/jniLibs/arm64-v8a"

echo "========================================="
echo "Copying llama.cpp Libraries"
echo "========================================="

# Create destination directories
mkdir -p "$APP_LIB"
mkdir -p "$AAR_LIB"

# Required core libraries
LIBS=(
    "libggml-base.so"
    "libggml-cpu.so"
    "libggml.so"
    "libllama.so"
)

# Copy each required library to both modules
for lib in "${LIBS[@]}"; do
    if [ -f "$SRC_LIB/$lib" ]; then
        cp -v "$SRC_LIB/$lib" "$APP_LIB/"
        cp -v "$SRC_LIB/$lib" "$AAR_LIB/"
        echo "Copied $lib"
    else
        echo "NOT FOUND: $lib"
        exit 1
    fi
done

# Optional: Copy OpenMP library if exists
if [ -f "$SRC_LIB/libomp.so" ]; then
    cp -v "$SRC_LIB/libomp.so" "$APP_LIB/"
    cp -v "$SRC_LIB/libomp.so" "$AAR_LIB/"
    echo "Copied libomp.so (OpenMP)"
fi

echo "========================================="
echo "All libraries copied successfully!"
echo "========================================="
echo ""
echo "Copied to:"
echo "  $APP_LIB"
echo "  $AAR_LIB"
echo ""
echo "Next step: ./gradlew assembleDebug"
