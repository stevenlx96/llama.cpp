# 为GGUFChat添加Vulkan/NPU支持指南

## 背景

GGUFChat当前只支持CPU推理，本指南将帮助你添加Vulkan支持以利用GPU/NPU加速。

## 前提条件

1. **Android NDK**: 版本 >= 26（推荐29+）
   ```bash
   export ANDROID_NDK="$HOME/Android/Sdk/ndk/29.0.13113456"
   ```

2. **Vulkan SDK** (可选，NDK已包含Vulkan头文件)
   - Android NDK r21+已包含Vulkan 1.1支持
   - NDK r29包含Vulkan 1.3支持

3. **目标设备**: Android 7.0+ (API 26+)，支持Vulkan 1.1+
   - 检查设备支持: `adb shell getprop ro.vulkan.level`

## 步骤1: 编译启用Vulkan的llama.cpp

### 1.1 运行编译脚本

```bash
cd /home/user/llama.cpp

# 编辑脚本，设置你的NDK路径
nano build_vulkan_android.sh

# 运行编译（arm64-v8a）
./build_vulkan_android.sh

# 如果需要编译armeabi-v7a，修改脚本中的ABI变量
# ABI="armeabi-v7a"
```

### 1.2 编译完成后的产物

编译成功后，会在 `build-android-vulkan-arm64-v8a/` 目录下生成以下库文件：

**必需的so文件：**
```
build-android-vulkan-arm64-v8a/ggml/src/
├── libggml-base.so          # GGML基础库
├── libggml-cpu.so           # CPU后端
├── ggml-vulkan/
│   └── libggml-vulkan.so    # ⭐ Vulkan后端（新增）
└── libggml.so               # GGML主库

build-android-vulkan-arm64-v8a/src/
└── libllama.so              # Llama主库
```

**可选（用于调试）：**
```
libggml-vulkan-opt.so        # Vulkan优化版本（如果启用）
```

## 步骤2: 复制库文件到GGUFChat项目

### 2.1 创建jniLibs目录结构

```bash
cd /home/user/llama.cpp/GGUFChat/llama-android/src/main

# 创建jniLibs目录
mkdir -p jniLibs/arm64-v8a
mkdir -p jniLibs/armeabi-v7a  # 如果需要支持32位
```

### 2.2 复制编译产物

```bash
# 假设你在 /home/user/llama.cpp 目录

# 复制arm64-v8a的库文件
cp build-android-vulkan-arm64-v8a/ggml/src/libggml-base.so \
   GGUFChat/llama-android/src/main/jniLibs/arm64-v8a/

cp build-android-vulkan-arm64-v8a/ggml/src/libggml-cpu.so \
   GGUFChat/llama-android/src/main/jniLibs/arm64-v8a/

cp build-android-vulkan-arm64-v8a/ggml/src/ggml-vulkan/libggml-vulkan.so \
   GGUFChat/llama-android/src/main/jniLibs/arm64-v8a/

cp build-android-vulkan-arm64-v8a/ggml/src/libggml.so \
   GGUFChat/llama-android/src/main/jniLibs/arm64-v8a/

cp build-android-vulkan-arm64-v8a/src/libllama.so \
   GGUFChat/llama-android/src/main/jniLibs/arm64-v8a/
```

## 步骤3: 修改CMakeLists.txt

编辑 `/home/user/llama.cpp/GGUFChat/llama-android/src/main/cpp/CMakeLists.txt`

### 3.1 在SECTION 2中添加vulkan库检查

在第38-51行的 `REQUIRED_LIBS` 列表中添加：

```cmake
set(REQUIRED_LIBS
        "libggml-base.so"
        "libggml-cpu.so"
        "libggml-vulkan.so"     # ⭐ 新增
        "libggml.so"
        "libllama.so"
)
```

### 3.2 在SECTION 6中导入Vulkan库

在第102行后添加：

```cmake
# ggml-vulkan (depends on ggml-base)
add_library(ggml_vulkan_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_vulkan_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-vulkan.so"
        INTERFACE_LINK_LIBRARIES ggml_base_prebuilt
)
message(STATUS "Imported ggml_vulkan_prebuilt (depends on ggml-base)")
```

### 3.3 修改ggml库的依赖关系

将第105-110行修改为：

```cmake
# ggml (depends on ggml-base, ggml-cpu, and ggml-vulkan)
add_library(ggml_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml.so"
        INTERFACE_LINK_LIBRARIES "ggml_base_prebuilt;ggml_cpu_prebuilt;ggml_vulkan_prebuilt"  # ⭐ 添加vulkan
)
message(STATUS "Imported ggml_prebuilt (depends on ggml-base, ggml-cpu, ggml-vulkan)")
```

### 3.4 修改llama库的依赖关系

将第113-118行修改为：

```cmake
# llama (depends on all ggml libraries)
add_library(llama_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(llama_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libllama.so"
        INTERFACE_LINK_LIBRARIES "ggml_prebuilt;ggml_cpu_prebuilt;ggml_vulkan_prebuilt;ggml_base_prebuilt"  # ⭐ 添加vulkan
)
message(STATUS "Imported llama_prebuilt (depends on all ggml libraries)")
```

## 步骤4: 更新Vulkan头文件

复制Vulkan相关的头文件到include目录：

```bash
cd /home/user/llama.cpp

# 复制ggml-vulkan头文件
cp ggml/include/ggml-vulkan.h \
   GGUFChat/llama-android/src/main/cpp/include/

# 复制ggml-backend头文件（如果还没有）
cp ggml/include/ggml-backend.h \
   GGUFChat/llama-android/src/main/cpp/include/
```

## 步骤5: 修改build.gradle.kts（可选）

如果需要在Gradle中传递CMake参数，编辑 `GGUFChat/llama-android/build.gradle.kts`：

在 `externalNativeBuild` 块中添加：

```kotlin
externalNativeBuild {
    cmake {
        path = file("src/main/cpp/CMakeLists.txt")
        version = "3.22.1"
    }
}

// 如果需要传递编译参数
android {
    defaultConfig {
        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=c++_shared"
                // Vulkan相关参数已经在编译so时设置，这里不需要
            }
        }
    }
}
```

## 步骤6: 编译GGUFChat

```bash
cd /home/user/llama.cpp/GGUFChat

# 使用Gradle编译
./gradlew :llama-android:assembleRelease

# 或使用Android Studio打开项目编译
```

## 步骤7: 验证Vulkan支持

### 7.1 检查APK中的so文件

```bash
# 解压APK
unzip -l app/build/outputs/apk/release/app-release.apk | grep "\.so$"

# 应该看到：
# lib/arm64-v8a/libggml-base.so
# lib/arm64-v8a/libggml-cpu.so
# lib/arm64-v8a/libggml-vulkan.so    # ⭐ 关键
# lib/arm64-v8a/libggml.so
# lib/arm64-v8a/libllama.so
# lib/arm64-v8a/libllama-android.so
```

### 7.2 运行时验证

在你的应用日志中查找：

```
adb logcat | grep -i vulkan
```

如果看到类似输出说明Vulkan已加载：
```
ggml_vulkan: Found 1 Vulkan devices
ggml_vulkan: Using Qualcomm Adreno (TM) 740 | uma: 1 | fp16: 1
```

## 步骤8: 修改Java/Kotlin代码以使用Vulkan（可选）

如果llama.cpp支持动态后端加载（`GGML_BACKEND_DL=ON`），不需要修改代码。

但为了确保使用Vulkan，可以在初始化时设置：

```kotlin
// 在 LlamaEngine.kt 的 nativeInit 中
// 需要修改JNI层以支持传递后端参数

// 或者通过环境变量（需要在JNI层实现）
// setenv("GGML_VULKAN", "1", 1);
```

## 常见问题

### Q1: 编译时找不到Vulkan头文件

**A**: 确保NDK版本 >= 21，Vulkan头文件在 `$NDK/sysroot/usr/include/vulkan/`

### Q2: 运行时崩溃 "dlopen failed: library not found"

**A**: 检查：
1. 所有依赖的so文件都复制到jniLibs
2. CMakeLists.txt中的依赖关系正确
3. 使用 `readelf -d libllama-android.so` 检查动态链接

### Q3: 设备不支持Vulkan

**A**: 检查设备Vulkan版本：
```bash
adb shell getprop ro.vulkan.level
# 返回 0 = 不支持
# 返回 1 = Vulkan 1.0.3
# 返回 2 = Vulkan 1.1
```

如果不支持，应用会自动fallback到CPU后端。

### Q4: Vulkan性能没有提升

**A**: 可能原因：
1. 模型太小，GPU开销大于收益
2. 量化格式不适合GPU（某些量化格式只在CPU优化）
3. NPU没有暴露为Vulkan设备（需要检查厂商驱动）

### Q5: 如何强制使用CPU或Vulkan？

**A**: 需要在llama.cpp初始化时指定后端：
```cpp
// 在 llama-android-jni.cpp 中
llama_backend_init();  // 默认加载所有后端

// 强制使用特定后端需要修改llama_context_params
// 或使用 ggml_backend_dev 相关API
```

## 性能优化建议

1. **量化格式**: Q4_0, Q4_K_M 在移动GPU上通常表现最好
2. **上下文大小**: 减小context size可以减少显存占用
3. **批处理**: 增加batch size可以提高GPU利用率
4. **温度/采样**: 降低采样复杂度可以减少计算

## 进一步优化

### 使用QNN（高通NPU）

如果你的设备是高通芯片且想直接使用HTP（Hexagon Tensor Processor），需要：
1. 集成Qualcomm QNN SDK
2. 转换GGUF模型为QNN格式（目前llama.cpp不直接支持）

### 使用NNAPI

虽然llama.cpp不直接支持NNAPI，但可以考虑：
1. 使用NNAPI Delegate（需要大量开发）
2. 等待社区支持

## 参考资料

- [llama.cpp Vulkan文档](https://github.com/ggerganov/llama.cpp/blob/master/docs/build.md#vulkan)
- [Android Vulkan开发指南](https://developer.android.com/ndk/guides/graphics/getting-started)
- [GGML Backend API](https://github.com/ggerganov/llama.cpp/blob/master/ggml/include/ggml-backend.h)

## 总结

添加Vulkan支持的核心步骤：
1. ✅ 用NDK编译启用Vulkan的llama.cpp
2. ✅ 复制 libggml-vulkan.so 到 jniLibs
3. ✅ 修改 CMakeLists.txt 添加vulkan库依赖
4. ✅ 重新编译GGUFChat
5. ✅ 测试和验证

祝成功！🚀
