# GGUFChat Vulkan 支持 - 快速开始指南

## 问题症状

你在 Windows 上编译 Android Vulkan 支持时遇到错误：
```
fatal error: 'vulkan/vulkan.hpp' file not found
```

## 快速解决方案（3 步）

### ✅ 步骤 1: 安装 Vulkan SDK for Windows

1. **下载 Vulkan SDK**：https://vulkan.lunarg.com/sdk/home#windows
2. **安装时必须选择**：
   - ✅ Shader Toolchain Debug Libraries
   - ✅ Core SDK Components
3. **重启终端**（重要！）

**验证安装**：
```powershell
# 运行检查脚本
.\check_vulkan_setup.ps1

# 或手动验证
glslc --version
dir "C:\VulkanSDK\*\Include\vulkan\vulkan.hpp"
```

### ✅ 步骤 2: 编译 llama.android 的 Vulkan 支持

```powershell
cd examples\llama.android

# 清理旧的构建
.\gradlew clean
Remove-Item -Recurse -Force lib\.cxx

# 重新编译（Release 版本）
.\gradlew :lib:assembleRelease
```

**预期结果**：
```
BUILD SUCCESSFUL in 5m 30s
```

生成的库文件在：
```
lib\.cxx\Release\<hash>\arm64-v8a\build-llama\ggml\src\ggml-vulkan\libggml-vulkan.so
```

### ✅ 步骤 3: 复制库文件并编译 GGUFChat

```powershell
# 返回项目根目录
cd ..\..

# 自动复制 Vulkan 库到 GGUFChat（推荐）
.\copy_vulkan_to_ggufchat.ps1

# 编译 GGUFChat
cd GGUFChat
.\gradlew clean
.\gradlew assembleRelease
```

**预期结果**：
APK 中包含以下库：
```
lib/arm64-v8a/
├── libggml-base.so
├── libggml-cpu.so
├── libggml-vulkan.so     ← 新增！
├── libggml.so
├── libllama.so
└── libllama-android.so
```

## 验证 Vulkan 是否工作

安装 APK 到设备后：

```bash
adb install -r app\build\outputs\apk\release\app-release.apk
adb logcat | grep -i vulkan
```

成功的话会看到：
```
ggml_vulkan: Found 1 Vulkan devices
ggml_vulkan: Using <GPU名称> | uma: 1 | fp16: 1
```

## 常见问题

### Q: glslc 找不到？

**A**:
1. 关闭所有终端窗口
2. 重新打开 PowerShell
3. 运行 `glslc --version`
4. 如果还是不行，手动添加到 PATH：
   ```powershell
   $env:PATH = "C:\VulkanSDK\1.3.xxx.x\Bin;" + $env:PATH
   ```

### Q: 编译时仍然找不到 vulkan.hpp？

**A**:
1. 确认文件存在：`dir "C:\VulkanSDK\*\Include\vulkan\vulkan.hpp"`
2. 如果不存在，重新安装 Vulkan SDK，确保选择 "Core SDK Components"
3. 清理 CMake 缓存：
   ```powershell
   Remove-Item -Recurse -Force examples\llama.android\lib\.cxx
   ```
4. 重新编译

### Q: libggml-vulkan.so 没有生成？

**A**: 检查 `examples\llama.android\lib\build.gradle.kts` 中是否包含：
```kotlin
arguments(
    "-DGGML_VULKAN=ON",
    // ...
)
```

如果没有，手动添加并重新编译。

### Q: GGUFChat 编译失败 - 找不到 libggml-vulkan.so？

**A**:
1. 确认文件已复制：
   ```powershell
   dir GGUFChat\llama-android\src\main\jniLibs\arm64-v8a\libggml-vulkan.so
   ```
2. 如果不存在，运行：
   ```powershell
   .\copy_vulkan_to_ggufchat.ps1
   ```

### Q: 运行时崩溃 - dlopen failed?

**A**: 检查依赖顺序，确保 `CMakeLists.txt` 中：
```cmake
INTERFACE_LINK_LIBRARIES "ggml_prebuilt;ggml_cpu_prebuilt;ggml_vulkan_prebuilt;ggml_base_prebuilt"
```

### Q: 设备不支持 Vulkan？

**A**: 检查设备 Vulkan 版本：
```bash
adb shell getprop ro.vulkan.level
```
- 0 = 不支持
- 1 = Vulkan 1.0
- 2 = Vulkan 1.1+

如果不支持，应用会自动 fallback 到 CPU。

## 脚本说明

### `check_vulkan_setup.ps1`
检查 Vulkan 开发环境是否正确配置：
- Vulkan SDK 安装
- glslc 编译器
- vulkan.hpp 头文件
- Android SDK/NDK
- Ninja 构建工具
- llama.android 配置
- GGUFChat 配置

### `copy_vulkan_to_ggufchat.ps1`
自动复制编译好的库文件到 GGUFChat 项目：
```powershell
# 复制 Release 版本（默认）
.\copy_vulkan_to_ggufchat.ps1

# 复制 Debug 版本
.\copy_vulkan_to_ggufchat.ps1 -BuildType Debug

# 复制 32 位版本
.\copy_vulkan_to_ggufchat.ps1 -ABI armeabi-v7a
```

## 手动步骤（如果脚本不工作）

### 手动复制库文件

```powershell
# 1. 查找编译产物
$SOURCE = "examples\llama.android\lib\.cxx\Release\<hash>\arm64-v8a\build-llama"
$DEST = "GGUFChat\llama-android\src\main\jniLibs\arm64-v8a"

# 2. 创建目标目录
New-Item -ItemType Directory -Force -Path $DEST

# 3. 复制文件
Copy-Item "$SOURCE\ggml\src\libggml-base.so" -Destination $DEST
Copy-Item "$SOURCE\ggml\src\libggml-cpu.so" -Destination $DEST
Copy-Item "$SOURCE\ggml\src\ggml-vulkan\libggml-vulkan.so" -Destination $DEST
Copy-Item "$SOURCE\ggml\src\libggml.so" -Destination $DEST
Copy-Item "$SOURCE\src\libllama.so" -Destination $DEST
```

### 手动修改 CMakeLists.txt

文件：`GGUFChat\llama-android\src\main\cpp\CMakeLists.txt`

已自动修改完成 ✓

## 技术原理

### 为什么需要 Windows 上的 Vulkan SDK？

1. **vulkan.hpp**：Vulkan C++ bindings，不在 Android NDK 中
2. **glslc**：着色器编译器，将 `.comp` 文件编译为 SPIR-V 字节码
3. **主机编译**：着色器编译在 Windows 主机上进行，不是在 Android 设备上

### 编译流程

```
Windows 主机                            Android 目标
━━━━━━━━━━━━━━━━                       ━━━━━━━━━━━━━
1. CMake 配置
   ├─ 查找 Vulkan SDK
   ├─ 查找 glslc
   └─ 生成 host-toolchain.cmake

2. 编译 vulkan-shaders-gen
   ├─ 使用 Windows 编译器
   ├─ 使用 Ninja 构建
   └─ 生成着色器编译工具

3. 编译 Vulkan 着色器
   ├─ 运行 vulkan-shaders-gen
   ├─ 调用 glslc 编译 150+ 个 .comp 文件
   ├─ 生成 .spv (SPIR-V) 字节码
   └─ 嵌入到 C++ 头文件

4. 交叉编译到 Android
   ├─ 使用 Android NDK 编译器
   ├─ 链接编译好的着色器
   └─ 生成 libggml-vulkan.so  ─────────>  APK
```

### Vulkan vs CPU vs NPU

| 后端 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| **CPU** | 兼容性好，实现简单 | 速度慢，功耗高 | 小模型，老设备 |
| **Vulkan** | 速度快，兼容性好 | 需要 GPU 支持 | 中大模型，现代设备 |
| **NPU** | 速度最快，功耗低 | 需要厂商支持，兼容性差 | 特定芯片（高通/联发科） |

**Vulkan 的优势**：
- ✓ 跨平台（Android/Windows/Linux）
- ✓ 可以利用 GPU 加速
- ✓ 某些设备上 NPU 也暴露为 Vulkan 设备
- ✓ llama.cpp 官方支持

## 性能优化

### 量化格式选择

```kotlin
// 推荐的量化格式（按速度排序）
val quantFormats = listOf(
    "Q4_0",      // 最快，质量略低
    "Q4_K_M",    // 平衡
    "Q5_K_M",    // 较慢，质量好
    "Q8_0"       // 最慢，质量最好
)
```

### 运行时参数

```kotlin
val params = LlamaParams(
    contextSize = 2048,      // 减小以节省显存
    batchSize = 512,         // GPU 推荐 512
    threads = 4,             // CPU fallback 时使用
    temperature = 0.7,       // 降低采样复杂度
    topP = 0.9
)
```

## 详细文档

- 完整指南：`WINDOWS_VULKAN_ANDROID_FIX_CN.md`
- Vulkan 配置：`VULKAN_ANDROID_GUIDE.md`
- 原始问题：`VULKAN_WINDOWS_FIX.md`

## 成功标志

✅ **编译成功**：
```
BUILD SUCCESSFUL in 5m 30s
```

✅ **库文件存在**：
```powershell
PS> dir GGUFChat\llama-android\src\main\jniLibs\arm64-v8a\*.so

libggml-base.so
libggml-cpu.so
libggml-vulkan.so    ← 关键文件
libggml.so
libllama.so
```

✅ **运行时日志**：
```
ggml_vulkan: Found 1 Vulkan devices
ggml_vulkan: Using Adreno (TM) 740
```

✅ **性能提升**：
- CPU: ~5-10 tokens/s
- Vulkan: ~15-30 tokens/s（提升 2-3 倍）

祝你成功！🚀
