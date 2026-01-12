# Windows上编译Android Vulkan支持 - 完整解决方案

## 问题分析

你遇到的错误：
```
E:/MyGithub/llama.cpp/ggml/src/ggml-vulkan/ggml-vulkan.cpp:22:10: fatal error: 'vulkan/vulkan.hpp' file not found
```

**根本原因**：
- `vulkan.hpp` 是 Vulkan C++ bindings（Vulkan-Hpp），不在 Android NDK 中
- Android NDK 只包含运行时头文件（`vulkan_core.h`）
- 编译 Vulkan 着色器需要 `glslc` 工具，也不在 Android NDK 中
- 你需要在 **Windows 主机**上安装完整的 Vulkan SDK

## 解决方案

### 步骤1: 安装 Vulkan SDK for Windows

#### 1.1 下载 Vulkan SDK

访问官网下载：
```
https://vulkan.lunarg.com/sdk/home#windows
```

或直接下载最新版本：
```
https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe
```

大小约 500MB

#### 1.2 安装 Vulkan SDK

1. 运行 `vulkan-sdk.exe`
2. **重要**：安装时必须选择以下组件：
   - ✅ **Shader Toolchain Debug Libraries** （包含 glslc）
   - ✅ **Core SDK Components** （包含 vulkan.hpp）
   - ✅ **Debuggable Shader API Libraries**

3. 默认安装路径：
   ```
   C:\VulkanSDK\1.3.xxx.x
   ```

4. 安装程序会自动添加环境变量到 PATH

#### 1.3 验证安装

打开 **新的** PowerShell 或 Git Bash 窗口（必须重启终端）：

```powershell
# 验证 glslc
glslc --version

# 验证 vulkan.hpp
dir "C:\VulkanSDK\*\Include\vulkan\vulkan.hpp"
```

预期输出：
```
glslc 1.3.xxx.x
Target: SPIR-V 1.0

vulkan.hpp 找到
```

### 步骤2: 设置环境变量

确保以下环境变量已设置（通常 Android Studio 已自动设置）：

```powershell
# 检查环境变量
echo $env:ANDROID_HOME
echo $env:ANDROID_SDK_ROOT
echo $env:VULKAN_SDK
```

如果 `ANDROID_HOME` 为空，手动设置：
```powershell
$env:ANDROID_HOME = "E:\android\android_sdk"
```

如果 `VULKAN_SDK` 为空，手动设置：
```powershell
$env:VULKAN_SDK = "C:\VulkanSDK\1.3.xxx.x"
```

**永久设置**（推荐）：
1. 按 Win+R，输入 `sysdm.cpl`
2. 高级 → 环境变量
3. 添加系统变量：
   - `VULKAN_SDK` = `C:\VulkanSDK\1.3.xxx.x`
   - `PATH` 添加 `%VULKAN_SDK%\Bin`

### 步骤3: 清理并重新编译

#### 3.1 清理旧的构建文件

```powershell
cd E:\MyGithub\llama.cpp\examples\llama.android

# 清理 Gradle 缓存
.\gradlew clean

# 清理 CMake 缓存（重要！）
Remove-Item -Recurse -Force lib\.cxx -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force .gradle -ErrorAction SilentlyContinue
```

#### 3.2 重新编译

```powershell
# 重新编译（使用 Gradle）
.\gradlew :lib:assembleRelease

# 或者使用 Debug 版本
.\gradlew :lib:assembleDebug
```

#### 3.3 监控编译进度

成功的话你会看到：
```
> Task :lib:buildCMakeRelease[arm64-v8a]
[1/923] Building Vulkan shaders...
[2/923] Compiling vulkan-shaders-gen...
[150/923] Compiling mul_mat_vec.comp.spv...
[300/923] Compiling matmul_f32.comp.spv...
...
[923/923] Linking libai-chat.so

BUILD SUCCESSFUL
```

### 步骤4: 验证编译结果

检查生成的库文件：

```powershell
# 检查 Vulkan 库是否生成
dir "E:\MyGithub\llama.cpp\examples\llama.android\lib\.cxx\Release\*\arm64-v8a\build-llama\ggml\src\ggml-vulkan\libggml-vulkan.so"
```

应该看到：
```
libggml-vulkan.so  (约 5-10 MB)
```

完整的库文件列表：
```
build-llama/ggml/src/
├── libggml-base.so
├── libggml-cpu.so
├── ggml-vulkan/
│   └── libggml-vulkan.so    ← 关键文件
└── libggml.so

build-llama/src/
└── libllama.so
```

### 步骤5: 复制库文件到 GGUFChat

#### 5.1 找到编译产物路径

```powershell
# 进入 GGUFChat 项目
cd E:\MyGithub\llama.cpp\GGUFChat

# 创建 jniLibs 目录
New-Item -ItemType Directory -Force -Path "llama-android\src\main\jniLibs\arm64-v8a"
```

#### 5.2 复制所有必需的 so 文件

```powershell
# 设置源路径（根据你的实际路径调整）
$SOURCE = "E:\MyGithub\llama.cpp\examples\llama.android\lib\.cxx\Release\1i2j2dco\arm64-v8a"
$DEST = "E:\MyGithub\llama.cpp\GGUFChat\llama-android\src\main\jniLibs\arm64-v8a"

# 复制基础库
Copy-Item "$SOURCE\build-llama\ggml\src\libggml-base.so" -Destination $DEST
Copy-Item "$SOURCE\build-llama\ggml\src\libggml-cpu.so" -Destination $DEST
Copy-Item "$SOURCE\build-llama\ggml\src\libggml.so" -Destination $DEST

# 复制 Vulkan 库（关键！）
Copy-Item "$SOURCE\build-llama\ggml\src\ggml-vulkan\libggml-vulkan.so" -Destination $DEST

# 复制 Llama 库
Copy-Item "$SOURCE\build-llama\src\libllama.so" -Destination $DEST
```

#### 5.3 验证复制结果

```powershell
dir $DEST
```

应该看到：
```
libggml-base.so
libggml-cpu.so
libggml-vulkan.so    ← 新增
libggml.so
libllama.so
```

### 步骤6: 修改 GGUFChat 的 CMakeLists.txt

#### 6.1 打开文件

```
E:\MyGithub\llama.cpp\GGUFChat\llama-android\src\main\cpp\CMakeLists.txt
```

#### 6.2 修改 SECTION 2（添加 Vulkan 库检查）

找到第 39-44 行，修改为：

```cmake
set(REQUIRED_LIBS
        "libggml-base.so"
        "libggml-cpu.so"
        "libggml-vulkan.so"     # ← 添加这行
        "libggml.so"
        "libllama.so"
)
```

#### 6.3 修改 SECTION 6（导入 Vulkan 库）

在第 102 行后添加：

```cmake
# ggml-vulkan (depends on ggml-base)
add_library(ggml_vulkan_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_vulkan_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-vulkan.so"
        INTERFACE_LINK_LIBRARIES ggml_base_prebuilt
)
message(STATUS "Imported ggml_vulkan_prebuilt (depends on ggml-base)")
```

#### 6.4 修改 ggml 库的依赖（第 105-110 行）

修改为：

```cmake
# ggml (depends on ggml-base, ggml-cpu, and ggml-vulkan)
add_library(ggml_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml.so"
        INTERFACE_LINK_LIBRARIES "ggml_base_prebuilt;ggml_cpu_prebuilt;ggml_vulkan_prebuilt"  # ← 添加 vulkan
)
message(STATUS "Imported ggml_prebuilt (depends on ggml-base, ggml-cpu, ggml-vulkan)")
```

#### 6.5 修改 llama 库的依赖（第 113-118 行）

修改为：

```cmake
# llama (depends on all ggml libraries)
add_library(llama_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(llama_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libllama.so"
        INTERFACE_LINK_LIBRARIES "ggml_prebuilt;ggml_cpu_prebuilt;ggml_vulkan_prebuilt;ggml_base_prebuilt"  # ← 添加 vulkan
)
message(STATUS "Imported llama_prebuilt (depends on all ggml libraries)")
```

### 步骤7: 编译 GGUFChat

```powershell
cd E:\MyGithub\llama.cpp\GGUFChat

# 清理并编译
.\gradlew clean
.\gradlew :llama-android:assembleRelease

# 或编译整个 App
.\gradlew assembleRelease
```

### 步骤8: 验证 Vulkan 支持

#### 8.1 检查 APK 中的库

```powershell
# 解压 APK 查看
Expand-Archive -Path "app\build\outputs\apk\release\app-release.apk" -DestinationPath "temp_apk" -Force

# 检查是否包含 Vulkan 库
dir "temp_apk\lib\arm64-v8a\*.so"
```

应该看到：
```
libggml-base.so
libggml-cpu.so
libggml-vulkan.so        ← 关键！
libggml.so
libllama.so
libllama-android.so
```

#### 8.2 运行时测试

安装 APK 到设备后，查看日志：

```bash
adb logcat | grep -i vulkan
```

成功的话会看到：
```
ggml_vulkan: Found 1 Vulkan devices
ggml_vulkan: Using Qualcomm Adreno (TM) 740 | uma: 1 | fp16: 1
```

## 常见问题排查

### Q1: glslc 仍然找不到

**症状**：
```
CMake Error: Could not find glslc
```

**解决方案**：
1. 关闭所有终端窗口
2. 重新打开 PowerShell
3. 验证：`glslc --version`
4. 如果还是找不到，手动添加到 PATH：
   ```powershell
   $env:PATH = "C:\VulkanSDK\1.3.xxx.x\Bin;" + $env:PATH
   ```

### Q2: vulkan.hpp 仍然找不到

**症状**：
```
fatal error: 'vulkan/vulkan.hpp' file not found
```

**解决方案**：
1. 确认文件存在：
   ```powershell
   dir "C:\VulkanSDK\*\Include\vulkan\vulkan.hpp"
   ```
2. 如果不存在，重新安装 Vulkan SDK，确保选择 "Core SDK Components"
3. 清理 CMake 缓存：
   ```powershell
   Remove-Item -Recurse -Force lib\.cxx
   ```

### Q3: Ninja 找不到

**症状**：
```
CMake Error: CMake was unable to find a build program corresponding to "Ninja"
```

**解决方案**：
项目已经修复了这个问题（见 `examples/llama.android/lib/src/main/cpp/CMakeLists.txt` 第 33-66 行）

如果仍然出错，手动设置：
```powershell
$env:CMAKE_MAKE_PROGRAM = "$env:ANDROID_HOME\cmake\3.31.6\bin\ninja.exe"
```

### Q4: 链接错误 - 找不到 Vulkan 库

**症状**：
```
ld: error: cannot find -lvulkan
```

**原因**：不应该在 Android 上静态链接 Vulkan SDK 的库，而是使用 Android NDK 的 Vulkan

**解决方案**：确保 CMakeLists.txt 中使用的是：
```cmake
target_link_libraries(ggml-vulkan PRIVATE Vulkan::Vulkan)
```
而不是：
```cmake
target_link_libraries(ggml-vulkan PRIVATE vulkan)
```

### Q5: 运行时崩溃 - dlopen failed

**症状**：
```
dlopen failed: library "libggml-vulkan.so" not found
```

**解决方案**：
1. 确认 so 文件在 jniLibs 中
2. 检查 CMakeLists.txt 的依赖顺序（Vulkan 必须在 ggml 之前）
3. 使用 `readelf` 检查依赖：
   ```bash
   readelf -d libllama-android.so | grep NEEDED
   ```

### Q6: 设备不支持 Vulkan

**检查设备支持**：
```bash
adb shell getprop ro.vulkan.level
```

返回值：
- `0` = 不支持 Vulkan
- `1` = Vulkan 1.0.3
- `2` = Vulkan 1.1+

如果不支持，应用会自动 fallback 到 CPU 后端。

## 编译参数说明

在 `examples/llama.android/lib/build.gradle.kts` 中的关键参数：

```kotlin
arguments(
    "-DGGML_VULKAN=ON",              // 启用 Vulkan
    "-DGGML_CPU_KLEIDIAI=OFF",       // 禁用 KleidiAI（避免网络问题）
    "-DANDROID_STL=c++_shared",      // 使用共享 C++ 运行时
)
```

## 性能优化建议

1. **量化格式**：Q4_0, Q4_K_M 在移动 GPU 上表现最好
2. **上下文大小**：减小 context size（如 2048）减少显存占用
3. **批处理**：batch_size=512 可以提高 GPU 利用率
4. **温度设置**：降低采样复杂度（temperature=0.7）

## 总结

解决 Windows 上编译 Android Vulkan 支持的核心步骤：

1. ✅ 安装 Vulkan SDK for Windows
2. ✅ 验证 glslc 和 vulkan.hpp 可用
3. ✅ 清理旧的构建缓存
4. ✅ 重新编译 llama.android（生成 libggml-vulkan.so）
5. ✅ 复制所有 so 文件到 GGUFChat
6. ✅ 修改 GGUFChat 的 CMakeLists.txt 添加 Vulkan 依赖
7. ✅ 编译并测试

祝你成功！🚀

## 参考资料

- [Vulkan SDK 下载](https://vulkan.lunarg.com/sdk/home#windows)
- [Vulkan-Hpp GitHub](https://github.com/KhronosGroup/Vulkan-Hpp)
- [llama.cpp Vulkan 文档](https://github.com/ggerganov/llama.cpp/blob/master/docs/build.md#vulkan)
- [Android Vulkan 开发指南](https://developer.android.com/ndk/guides/graphics/getting-started)
