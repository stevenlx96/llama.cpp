# How to Add Official llama.cpp Libraries

This directory should contain the **official pre-compiled llama.cpp libraries** for optimal performance.

## Required Files

Copy these `.so` files from your official `pkg-adb/llama.cpp/lib/` directory to `arm64-v8a/`:

```
arm64-v8a/
├── libggml-base.so          (Required)
├── libggml-cpu.so           (Required)
├── libggml-hexagon.so       (Required - Hexagon NPU support)
├── libggml-htp-v73.so       (Required - HTP backend v73)
├── libggml-htp-v75.so       (Required - HTP backend v75)
├── libggml-htp-v79.so       (Required - HTP backend v79)
├── libggml-htp-v81.so       (Required - HTP backend v81)
├── libggml.so               (Required)
└── libllama.so              (Required)
```

## Copy Command (Windows)

If your official libraries are in `E:\MyGithub\llama.cpp\pkg-adb\llama.cpp\lib\`:

```cmd
cd E:\MyGithub\llama.cpp
xcopy /Y pkg-adb\llama.cpp\lib\*.so GGUFChat\app\src\main\jniLibs\arm64-v8a\
```

## Copy Command (Linux/Mac)

```bash
cd ~/llama.cpp
cp pkg-adb/llama.cpp/lib/*.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
```

## Verify Files

After copying, verify all files are present:

```bash
ls -lh GGUFChat/app/src/main/jniLibs/arm64-v8a/
```

You should see all 9 `.so` files listed above.

## Why Use Official Libraries?

The official llama.cpp libraries are compiled with:
- ✅ Optimized compiler flags
- ✅ Correct Hexagon SDK configuration
- ✅ Proven performance (51+ tokens/s on Hexagon NPU)

Our custom JNI wrapper (`llama-android-jni.cpp`) will link against these libraries to get the same performance as the official `llama-completion` command.

## Build After Copying

After copying the `.so` files, rebuild the app:

```bash
cd GGUFChat
./gradlew assembleDebug
```

The build will verify all required libraries are present and link them to the JNI wrapper.
