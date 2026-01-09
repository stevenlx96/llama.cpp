# How to Copy Official Libraries

## Problem

The official llama.cpp libraries include OpenCL support (`libggml-opencl.so`), which depends on the system library `libOpenCL.so`.

APK applications run in an isolated namespace and **cannot access** `/system/vendor/lib64/libOpenCL.so`.

## Solution

**Do NOT copy `libggml-opencl.so`** - only copy CPU and Hexagon backends.

## Windows Instructions

1. **Open Command Prompt** in the GGUFChat directory:
   ```cmd
   cd E:\MyGithub\llama.cpp\GGUFChat
   ```

2. **Run the copy script**:
   ```cmd
   copy-official-libs.cmd
   ```

3. **Verify files were copied**:
   ```cmd
   dir app\src\main\jniLibs\arm64-v8a\
   ```

   You should see 9 files (NO libggml-opencl.so):
   - libggml-base.so
   - libggml-cpu.so
   - libggml-hexagon.so
   - libggml-htp-v73.so
   - libggml-htp-v75.so
   - libggml-htp-v79.so
   - libggml-htp-v81.so
   - libggml.so
   - libllama.so

## If libggml.so Still Complains About OpenCL

If you get an error like `libggml.so: undefined symbol: ggml_opencl_...`, it means `libggml.so` has a hard dependency on OpenCL.

### Workaround Options:

1. **Re-compile without OpenCL** (recommended):
   - Edit `docs/backend/hexagon/CMakeUserPresets.json`
   - Change `"GGML_OPENCL": "ON"` to `"GGML_OPENCL": "OFF"`
   - Rebuild and reinstall

2. **Extract system OpenCL library** (advanced):
   ```bash
   adb pull /system/vendor/lib64/libOpenCL.so
   # Copy to jniLibs/arm64-v8a/
   ```
   ⚠️ This may violate system integrity checks on some devices.

## Why This Works

llama.cpp backends are **dynamically loaded** based on the `--device` parameter. When we use `--device HTP0` (Hexagon), it won't try to load the OpenCL backend.

Our JNI code explicitly selects the Hexagon device, so OpenCL is never initialized.
