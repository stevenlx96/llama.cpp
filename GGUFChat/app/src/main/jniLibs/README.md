# How to Add llama.cpp Libraries (CPU-Only)

This directory should contain the **pre-compiled llama.cpp libraries** for CPU inference.

## Required Files

Copy these `.so` files to `arm64-v8a/`:

```
arm64-v8a/
├── libggml-base.so          (Required - GGML base library)
├── libggml-cpu.so           (Required - CPU backend)
├── libggml.so               (Required - GGML main library)
├── libllama.so              (Required - LLaMA inference)
└── libomp.so                (Optional - OpenMP for parallel CPU)
```

## Optional: ONNX Runtime (for Intent Recognition)

If you want to enable intent recognition feature:

```
arm64-v8a/
└── libonnxruntime.so        (Optional - ONNX Runtime)
```

## Copy Command (Windows)

```cmd
cd E:\MyGithub\llama.cpp
xcopy /Y pkg-adb\llama.cpp\lib\libggml-base.so GGUFChat\app\src\main\jniLibs\arm64-v8a\
xcopy /Y pkg-adb\llama.cpp\lib\libggml-cpu.so GGUFChat\app\src\main\jniLibs\arm64-v8a\
xcopy /Y pkg-adb\llama.cpp\lib\libggml.so GGUFChat\app\src\main\jniLibs\arm64-v8a\
xcopy /Y pkg-adb\llama.cpp\lib\libllama.so GGUFChat\app\src\main\jniLibs\arm64-v8a\
xcopy /Y pkg-adb\llama.cpp\lib\libomp.so GGUFChat\app\src\main\jniLibs\arm64-v8a\
```

## Copy Command (Linux/Mac)

```bash
cd ~/llama.cpp
cp pkg-adb/llama.cpp/lib/libggml-base.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-cpu.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libllama.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libomp.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
```

## Verify Files

After copying, verify all files are present:

```bash
ls -lh GGUFChat/app/src/main/jniLibs/arm64-v8a/
```

You should see 5 `.so` files (or 6 with ONNX Runtime).

## Build After Copying

After copying the `.so` files, rebuild the app:

```bash
cd GGUFChat
./gradlew assembleDebug
```

The build will verify all required libraries are present and link them to the JNI wrapper.
