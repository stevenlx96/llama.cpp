#!/bin/bash
# Copy official llama.cpp libraries to GGUFChat project
# Excludes OpenCL to avoid system library dependencies

set -e

# Source directory (official pkg-adb)
SRC_LIB="../pkg-adb/llama.cpp/lib"

# Destination directory (GGUFChat jniLibs)
DEST_LIB="app/src/main/jniLibs/arm64-v8a"

echo "========================================="
echo "Copying Official llama.cpp Libraries"
echo "========================================="

# Create destination directory
mkdir -p "$DEST_LIB"

# Libraries to copy (EXCLUDE libggml-opencl.so!)
LIBS=(
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

# Copy each library
for lib in "${LIBS[@]}"; do
    if [ -f "$SRC_LIB/$lib" ]; then
        cp -v "$SRC_LIB/$lib" "$DEST_LIB/"
        echo "✓ Copied $lib"
    else
        echo "✗ NOT FOUND: $lib"
        exit 1
    fi
done

echo "========================================="
echo "✓ All libraries copied successfully!"
echo "========================================="
echo ""
echo "Copied to: $DEST_LIB"
echo ""
echo "Libraries:"
ls -lh "$DEST_LIB"/*.so
echo ""
echo "Next step: ./gradlew assembleDebug"
