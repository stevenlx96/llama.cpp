# ✅ Your TODO Checklist

## On Your Windows Machine (E:\MyGithub\llama.cpp)

### 1️⃣ Pull Latest Changes
```bash
git checkout claude/add-npu-support-P0vk2
git pull origin claude/add-npu-support-P0vk2
```

### 2️⃣ Verify Environment Variable
```bash
echo $ANDROID_HOME
# Should show: E:\android\android_sdk (or similar)
```

### 3️⃣ Clean Everything
```bash
cd examples/llama.android
./gradlew clean
rm -rf lib/.cxx lib/build .gradle/
cd ../..
rm -rf build/
cd examples/llama.android
```

### 4️⃣ Close and Restart Android Studio
- Close Android Studio completely
- Reopen it
- Wait for indexing to complete

### 5️⃣ Build
```bash
./gradlew :lib:assembleRelease
```

### 6️⃣ Wait for Success
```
BUILD SUCCESSFUL in XXm XXs
```

### 7️⃣ Find Your .so Files
```
lib/build/intermediates/cmake/release/obj/arm64-v8a/
├── libggml-vulkan.so  ← This is what you need!
├── libggml.so
├── libllama.so
└── libai-chat.so
```

---

## If Something Goes Wrong

### Problem: Still can't find Ninja
**Check:** What CMake version do you have?
```bash
ls E:\android\android_sdk\cmake\
```

If it's NOT 3.31.6, edit this file:
`ggml/src/ggml-vulkan/cmake/host-toolchain.cmake.in`

Change line 14 and 16 to match your version:
```cmake
set(CMAKE_MAKE_PROGRAM "$ENV{ANDROID_HOME}/cmake/YOUR_VERSION/bin/ninja.exe" ...)
```

### Problem: Build fails at different step
See `FIX_NINJA_PATH_STEPS.md` for detailed troubleshooting

---

## Success? Next Steps

1. Copy .so files to GGUFChat
2. Update GGUFChat CMakeLists.txt
3. Test on Android device
4. Enjoy GPU/NPU acceleration! 🚀
