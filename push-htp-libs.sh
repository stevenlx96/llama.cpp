#!/bin/bash

# Script to push HTP libraries to device for Hexagon NPU support
# This script tries multiple methods to make HTP libs accessible to DSP

set -e

echo "========================================="
echo "Hexagon NPU - HTP Library Deployment"
echo "========================================="

HTP_LIBS_DIR="GGUFChat/app/src/main/jniLibs/arm64-v8a"
TARGET_PATH="/vendor/dsp/cdsp"
APP_PKG="com.stdemo.ggufchat"

# Check if HTP libraries exist
echo ""
echo "Checking HTP libraries..."
for lib in libggml-htp-v73.so libggml-htp-v75.so libggml-htp-v79.so libggml-htp-v81.so; do
    if [ ! -f "$HTP_LIBS_DIR/$lib" ]; then
        echo "❌ ERROR: $lib not found in $HTP_LIBS_DIR"
        exit 1
    fi
    echo "  ✓ Found $lib"
done

echo ""
echo "========================================="
echo "Method 1: Try adb root (requires unlocked bootloader or eng build)"
echo "========================================="

if adb root 2>/dev/null; then
    echo "✓ adb root successful!"

    # Wait for device
    adb wait-for-device

    # Try to remount
    echo "Attempting to remount /vendor as read-write..."
    if adb remount 2>/dev/null; then
        echo "✓ Remount successful!"

        # Create directory if needed
        echo "Creating $TARGET_PATH directory..."
        adb shell mkdir -p "$TARGET_PATH" 2>/dev/null || true

        # Push HTP libraries
        echo "Pushing HTP libraries to $TARGET_PATH..."
        for lib in libggml-htp-v73.so libggml-htp-v75.so libggml-htp-v79.so libggml-htp-v81.so; do
            echo "  Pushing $lib..."
            adb push "$HTP_LIBS_DIR/$lib" "$TARGET_PATH/$lib"
            adb shell chmod 644 "$TARGET_PATH/$lib"
            echo "  ✓ $lib pushed"
        done

        echo ""
        echo "========================================="
        echo "✅ SUCCESS! HTP libraries deployed to $TARGET_PATH"
        echo "========================================="
        echo ""
        echo "Next steps:"
        echo "1. Reboot your device: adb reboot"
        echo "2. After reboot, rebuild and run GGUFChat app"
        echo "3. Check Logcat for 'Hexagon NPU' initialization"
        echo ""
        echo "Note: Libraries will persist until next system update or factory reset"
        echo ""
        exit 0
    else
        echo "⚠ Remount failed - /vendor partition may be protected"
    fi
else
    echo "⚠ adb root failed - device may not be rooted or bootloader locked"
fi

echo ""
echo "========================================="
echo "Method 2: Try pushing to alternative paths"
echo "========================================="

# Try alternative paths that might be accessible
ALT_PATHS=(
    "/data/local/tmp/dsp"
    "/sdcard/dsp"
    "/data/vendor/dsp"
)

for alt_path in "${ALT_PATHS[@]}"; do
    echo "Trying $alt_path..."

    if adb shell mkdir -p "$alt_path" 2>/dev/null; then
        echo "  Created directory $alt_path"

        # Push libraries
        success=true
        for lib in libggml-htp-v73.so libggml-htp-v75.so libggml-htp-v79.so libggml-htp-v81.so; do
            if ! adb push "$HTP_LIBS_DIR/$lib" "$alt_path/$lib" 2>/dev/null; then
                success=false
                break
            fi
        done

        if [ "$success" = true ]; then
            adb shell chmod -R 755 "$alt_path" 2>/dev/null || true
            echo "  ✓ Libraries pushed to $alt_path"
            echo ""
            echo "⚠ Alternative path used: $alt_path"
            echo "This may not work as DSP expects libraries in $TARGET_PATH"
            echo "But it's worth testing - try running the app and check Logcat"
            echo ""
        fi
    fi
done

echo ""
echo "========================================="
echo "Method 3: App-local library loading (requires code modification)"
echo "========================================="

APP_DATA_PATH="/data/data/$APP_PKG/files/htp-libs"

echo "Trying app data directory: $APP_DATA_PATH"
echo "This requires the app to be installed first."

# Check if app is installed
if adb shell pm list packages | grep -q "$APP_PKG"; then
    echo "✓ App is installed"

    # Try to create directory
    if adb shell run-as "$APP_PKG" mkdir -p files/htp-libs 2>/dev/null; then
        echo "  Created app-local directory"

        # Note: We can't directly push to app's private directory without root
        # User needs to do this from within the app
        echo ""
        echo "⚠ Cannot push to app private directory without root"
        echo "Alternative: Modify app code to:"
        echo "1. Copy HTP libs from assets/ to files/htp-libs/"
        echo "2. Set ADSP_LIBRARY_PATH environment variable to point to this directory"
        echo "3. Call dlopen() to pre-load HTP libraries before initializing Hexagon backend"
        echo ""
    fi
else
    echo "⚠ App not installed - install GGUFChat first"
fi

echo ""
echo "========================================="
echo "Summary & Recommendations"
echo "========================================="
echo ""
echo "Status:"
echo "  - Method 1 (adb root): Failed - device not rooted or remount protected"
echo "  - Method 2 (alt paths): May not work (DSP expects /vendor/dsp/cdsp/)"
echo "  - Method 3 (app-local): Requires code modification"
echo ""
echo "Recommendations:"
echo ""
echo "Option A - Root your device (RECOMMENDED for testing):"
echo "  1. Unlock bootloader (Samsung S25 may require ODIN)"
echo "  2. Flash Magisk or SuperSU"
echo "  3. Run this script again with root"
echo "  4. HTP libs will be accessible to DSP"
echo ""
echo "Option B - Modify llama.cpp Hexagon backend:"
echo "  1. Edit ggml/src/ggml-hexagon/ggml-hexagon.cpp"
echo "  2. Add code to load HTP libs from app directory to shared memory"
echo "  3. Use FastRPC remote_mmap() to share memory with DSP"
echo "  4. Recompile libggml-hexagon.so"
echo ""
echo "Option C - Try temporary root (if device supports):"
echo "  - Some Samsung devices allow temporary root via engineering builds"
echo "  - Search for 'Samsung S25 temporary root' or 'adb root enable'"
echo ""
echo "For now, the app will run but fall back to CPU mode."
echo "Check Logcat after running app - it should show:"
echo "  'Hexagon NPU not available, falling back to CPU'"
echo ""
echo "========================================="
