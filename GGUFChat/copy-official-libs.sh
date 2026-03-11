#!/bin/bash
# Copy llama.cpp libraries to GGUFChat project (with optional NPU/GPU acceleration)

set -e

# Source directory (official pkg-adb)
SRC_LIB="../pkg-adb/llama.cpp/lib"

# Destination directories
DEST_APP="app/src/main/jniLibs/arm64-v8a"
DEST_AAR="llama-android/src/main/jniLibs/arm64-v8a"

echo "========================================="
echo "Copying llama.cpp Libraries"
echo "(with NPU/GPU acceleration support)"
echo "========================================="

# Create destination directories
mkdir -p "$DEST_APP"
mkdir -p "$DEST_AAR"

# Required core libraries
LIBS=(
    "libggml-base.so"
    "libggml-cpu.so"
    "libggml.so"
    "libllama.so"
)

# Copy each required library to both targets
for lib in "${LIBS[@]}"; do
    if [ -f "$SRC_LIB/$lib" ]; then
        cp -v "$SRC_LIB/$lib" "$DEST_APP/"
        cp -v "$SRC_LIB/$lib" "$DEST_AAR/"
        echo "Copied $lib"
    else
        echo "NOT FOUND: $lib"
        exit 1
    fi
done

# Optional: Copy OpenMP library if exists
if [ -f "$SRC_LIB/libomp.so" ]; then
    cp -v "$SRC_LIB/libomp.so" "$DEST_APP/"
    cp -v "$SRC_LIB/libomp.so" "$DEST_AAR/"
    echo "Copied libomp.so (OpenMP)"
fi

# Optional: Copy OpenCL backend (GPU acceleration)
if [ -f "$SRC_LIB/libggml-opencl.so" ]; then
    cp -v "$SRC_LIB/libggml-opencl.so" "$DEST_APP/"
    cp -v "$SRC_LIB/libggml-opencl.so" "$DEST_AAR/"
    echo "Copied libggml-opencl.so (GPU acceleration)"
fi

# Optional: Copy Hexagon backend (NPU acceleration)
if [ -f "$SRC_LIB/libggml-hexagon.so" ]; then
    cp -v "$SRC_LIB/libggml-hexagon.so" "$DEST_APP/"
    cp -v "$SRC_LIB/libggml-hexagon.so" "$DEST_AAR/"
    echo "Copied libggml-hexagon.so (NPU acceleration)"
fi

# Optional: Copy Hexagon HTP libraries
for htp_lib in "$SRC_LIB"/libggml-htp*.so; do
    if [ -f "$htp_lib" ]; then
        cp -v "$htp_lib" "$DEST_APP/"
        cp -v "$htp_lib" "$DEST_AAR/"
        echo "Copied $(basename $htp_lib) (Hexagon HTP)"
    fi
done

echo "========================================="
echo "All libraries copied successfully!"
echo "========================================="
echo ""
echo "Copied to:"
echo "  $DEST_APP"
echo "  $DEST_AAR"
echo ""
echo "Libraries:"
ls -lh "$DEST_APP"/*.so
echo ""
echo "Note: Backends (CPU, OpenCL, Hexagon) are loaded"
echo "      dynamically at runtime via ggml_backend_load_all_from_path()"
echo ""
echo "Next step: ./gradlew assembleDebug"
