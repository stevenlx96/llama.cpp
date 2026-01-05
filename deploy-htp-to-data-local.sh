#!/bin/bash

# Deploy HTP libraries to /data/local/tmp/htp/ where DSP might be able to access
# This avoids needing root to write to /vendor/dsp/

set -e

echo "========================================="
echo "Deploying HTP libs to /data/local/tmp/"
echo "========================================="

HTP_LIBS_DIR="GGUFChat/app/src/main/jniLibs/arm64-v8a"
TARGET_DIR="/data/local/tmp/htp"

# Check if libraries exist
echo "Checking HTP libraries..."
for lib in libggml-htp-v73.so libggml-htp-v75.so libggml-htp-v79.so libggml-htp-v81.so; do
    if [ ! -f "$HTP_LIBS_DIR/$lib" ]; then
        echo "❌ ERROR: $lib not found"
        exit 1
    fi
    echo "  ✓ Found $lib"
done

# Create directory on device
echo ""
echo "Creating directory on device: $TARGET_DIR"
adb shell mkdir -p "$TARGET_DIR" || {
    echo "❌ Failed to create directory"
    exit 1
}

# Push libraries
echo ""
echo "Pushing HTP libraries..."
for lib in libggml-htp-v73.so libggml-htp-v75.so libggml-htp-v79.so libggml-htp-v81.so; do
    echo "  Pushing $lib..."
    adb push "$HTP_LIBS_DIR/$lib" "$TARGET_DIR/$lib"
done

# Set permissions
echo ""
echo "Setting permissions..."
adb shell chmod 755 "$TARGET_DIR"
adb shell chmod 644 "$TARGET_DIR"/*.so

# Verify
echo ""
echo "Verifying deployment..."
adb shell ls -lh "$TARGET_DIR/"

echo ""
echo "========================================="
echo "✅ HTP libraries deployed to $TARGET_DIR"
echo "========================================="
echo ""
echo "Libraries are now at:"
adb shell ls "$TARGET_DIR/" | while read lib; do
    echo "  - $TARGET_DIR/$lib"
done

echo ""
echo "⚠️  WARNING: This may not work!"
echo "DSP might not be able to access /data/local/tmp/"
echo ""
echo "If this doesn't work, try running the app and check Logcat."
echo "If you still see 'HTP library not found', we'll need to:"
echo "  1. Root the device and push to /vendor/dsp/cdsp/"
echo "  2. Or modify libggml-hexagon.so to use shared memory"
echo ""
