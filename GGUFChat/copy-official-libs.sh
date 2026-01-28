#!/bin/bash
# Copy llama.cpp libraries to GGUFChat project (CPU-only)

set -e

# Source directory (official pkg-adb)
SRC_LIB="../pkg-adb/llama.cpp/lib"

# Destination directory (GGUFChat jniLibs)
DEST_LIB="app/src/main/jniLibs/arm64-v8a"

echo "========================================="
echo "Copying llama.cpp Libraries (CPU-Only)"
echo "========================================="

# Create destination directory
mkdir -p "$DEST_LIB"

# Required CPU-only libraries
LIBS=(
    "libggml-base.so"
    "libggml-cpu.so"
    "libggml.so"
    "libllama.so"
)

# Copy each required library
for lib in "${LIBS[@]}"; do
    if [ -f "$SRC_LIB/$lib" ]; then
        cp -v "$SRC_LIB/$lib" "$DEST_LIB/"
        echo "Copied $lib"
    else
        echo "NOT FOUND: $lib"
        exit 1
    fi
done

# Optional: Copy OpenMP library if exists
if [ -f "$SRC_LIB/libomp.so" ]; then
    cp -v "$SRC_LIB/libomp.so" "$DEST_LIB/"
    echo "Copied libomp.so (OpenMP)"
fi

echo "========================================="
echo "All libraries copied successfully!"
echo "========================================="
echo ""
echo "Copied to: $DEST_LIB"
echo ""
echo "Libraries:"
ls -lh "$DEST_LIB"/*.so
echo ""
echo "Next step: ./gradlew assembleDebug"
