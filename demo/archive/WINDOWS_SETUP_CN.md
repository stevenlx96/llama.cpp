# Windows环境下为GGUFChat添加Vulkan支持

## 📋 前置条件

### 必需软件

1. **Android Studio**（推荐最新版）
   - 下载: https://developer.android.com/studio

2. **Git for Windows**（你已经有了）
   - 确认Git Bash可用

3. **Android SDK 组件**（通过Android Studio安装）：
   - Android SDK Platform Tools
   - Android NDK (Side by side) - 任意版本 >= 26
   - CMake - 任意版本 >= 3.22

---

## 🔧 步骤1: 安装Android Studio组件

### 1.1 打开SDK Manager

1. 启动 Android Studio
2. 点击 **File → Settings** (或 Configure → Settings)
3. 选择 **Appearance & Behavior → System Settings → Android SDK**
4. 切换到 **SDK Tools** 标签

### 1.2 安装必需组件

勾选以下组件并点击"Apply"：

- ✅ **Android SDK Build-Tools**
- ✅ **NDK (Side by side)** - 建议安装最新版本
- ✅ **CMake** - 安装最新版本
- ✅ **Android SDK Platform-Tools**

安装完成后，记下安装路径（通常在 `C:\Users\你的用户名\AppData\Local\Android\Sdk`）

### 1.3 查看安装的版本

安装完成后，在文件资源管理器中打开SDK目录，查看：

**NDK版本**：
```
C:\Users\你的用户名\AppData\Local\Android\Sdk\ndk\
```
会看到类似 `26.1.10909125` 这样的文件夹，这就是你的NDK版本

**CMake版本**：
```
C:\Users\你的用户名\AppData\Local\Android\Sdk\cmake\
```
会看到类似 `3.22.1` 这样的文件夹，这就是你的CMake版本

---

## 🚀 步骤2: 配置编译脚本

### 2.1 编辑脚本

在Git Bash中：

```bash
cd /e/MyGithub/llama.cpp  # 你的项目路径
nano build_vulkan_android_windows.sh  # 或用任何文本编辑器
```

### 2.2 修改配置变量

找到脚本开头的配置区，修改以下3个变量：

```bash
# Android SDK路径
# 改成你的实际路径，注意使用正斜杠 /
ANDROID_SDK_ROOT="C:/Users/你的用户名/AppData/Local/Android/Sdk"

# NDK版本（填写你在步骤1.3中看到的版本）
NDK_VERSION="26.1.10909125"

# CMake版本（填写你在步骤1.3中看到的版本）
CMAKE_VERSION="3.22.1"
```

**查找你的用户名**：
```bash
echo $USERNAME  # 在Git Bash中运行
```

**示例配置**（假设用户名是 Administrator）：
```bash
ANDROID_SDK_ROOT="C:/Users/Administrator/AppData/Local/Android/Sdk"
NDK_VERSION="26.1.10909125"
CMAKE_VERSION="3.22.1"
```

---

## 🏗️ 步骤3: 运行编译

### 3.1 执行编译脚本

```bash
cd /e/MyGithub/llama.cpp
./build_vulkan_android_windows.sh
```

### 3.2 预期输出

如果配置正确，你会看到：

```
=== Android Vulkan编译脚本 (Windows) ===

配置信息：
  Android SDK: C:/Users/Administrator/AppData/Local/Android/Sdk
  NDK路径: /c/Users/Administrator/AppData/Local/Android/Sdk/ndk/26.1.10909125
  CMake路径: /c/Users/Administrator/AppData/Local/Android/Sdk/cmake/3.22.1/bin/cmake

检查依赖...
✓ 所有依赖已找到

开始编译...
  目标ABI: arm64-v8a
  构建目录: build-android-vulkan-arm64-v8a

配置CMake...
[CMake配置输出...]

开始编译（这可能需要5-15分钟）...
[编译输出...]

=== 编译完成！ ===
```

### 3.3 编译时间

- **首次编译**: 5-15分钟（取决于CPU性能）
- **后续编译**: 1-3分钟（增量编译）

---

## 📦 步骤4: 复制库文件

编译成功后，运行：

```bash
./copy_vulkan_libs.sh
```

如果遇到问题，手动复制：

```bash
# 创建目标目录
mkdir -p GGUFChat/llama-android/src/main/jniLibs/arm64-v8a

# 复制文件
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

验证：
```bash
ls -lh GGUFChat/llama-android/src/main/jniLibs/arm64-v8a/
```

应该看到5个.so文件。

---

## 🔨 步骤5: 修改CMakeLists.txt

**方式A - 直接替换（推荐）**：

```bash
# 备份
cp GGUFChat/llama-android/src/main/cpp/CMakeLists.txt \
   GGUFChat/llama-android/src/main/cpp/CMakeLists.txt.backup

# 替换
cp GGUFChat_CMakeLists_VULKAN.txt \
   GGUFChat/llama-android/src/main/cpp/CMakeLists.txt
```

**方式B - 手动修改**：

用文本编辑器打开 `GGUFChat/llama-android/src/main/cpp/CMakeLists.txt`

1. 在第43行附近找到 `REQUIRED_LIBS`，添加：
```cmake
"libggml-vulkan.so"
```

2. 在第102行后添加：
```cmake
# ggml-vulkan
add_library(ggml_vulkan_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_vulkan_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-vulkan.so"
        INTERFACE_LINK_LIBRARIES ggml_base_prebuilt
)
```

3. 修改第108行和第116行，添加 `ggml_vulkan_prebuilt` 到依赖列表

---

## 🎯 步骤6: 编译GGUFChat

### 6.1 使用Android Studio（推荐）

1. 打开Android Studio
2. **File → Open** → 选择 `E:\MyGithub\llama.cpp\GGUFChat`
3. 等待Gradle同步完成
4. **Build → Rebuild Project**
5. 等待编译完成（首次5-10分钟）

### 6.2 使用命令行

```bash
cd GGUFChat

# Windows下使用 gradlew.bat
./gradlew.bat clean
./gradlew.bat :llama-android:assembleRelease

# 或在Git Bash中
./gradlew clean
./gradlew :llama-android:assembleRelease
```

---

## ✅ 步骤7: 验证安装

### 7.1 检查APK内容

```bash
# 找到生成的APK
find GGUFChat -name "*.apk" -type f

# 查看so文件
unzip -l GGUFChat/app/build/outputs/apk/release/app-release.apk | grep "\.so$"
```

应该看到：
```
lib/arm64-v8a/libggml-base.so
lib/arm64-v8a/libggml-cpu.so
lib/arm64-v8a/libggml-vulkan.so      ← 关键！
lib/arm64-v8a/libggml.so
lib/arm64-v8a/libllama.so
lib/arm64-v8a/libllama-android.so
```

### 7.2 设备测试

1. 安装APK到Android设备
2. 运行应用
3. 使用adb查看日志：

```bash
adb logcat | grep -i "vulkan\|ggml"
```

如果看到类似输出，说明成功：
```
ggml_vulkan: Found 1 Vulkan devices
ggml_vulkan: Using Qualcomm Adreno
```

---

## ❌ 常见问题

### 问题1: "cmake: command not found"

**原因**: CMake未安装或路径配置错误

**解决**:
1. 检查CMake是否安装（Android Studio → SDK Manager → SDK Tools）
2. 确认 `build_vulkan_android_windows.sh` 中的 `CMAKE_VERSION` 正确
3. 手动检查文件是否存在：
```bash
ls "C:/Users/$USERNAME/AppData/Local/Android/Sdk/cmake/3.22.1/bin/cmake.exe"
```

### 问题2: "NDK目录不存在"

**原因**: NDK未安装或版本号错误

**解决**:
1. 打开文件资源管理器
2. 导航到 `C:\Users\你的用户名\AppData\Local\Android\Sdk\ndk\`
3. 查看实际的文件夹名（这就是版本号）
4. 更新脚本中的 `NDK_VERSION`

### 问题3: 编译失败 "undefined reference to `vkCreateInstance`"

**原因**: Vulkan库未正确链接

**解决**: 这通常在NDK版本 < 21时出现，更新NDK到最新版本

### 问题4: 运行时崩溃 "dlopen failed"

**原因**: 库依赖关系错误

**解决**:
1. 确认所有5个so文件都已复制
2. 检查CMakeLists.txt中的依赖关系配置
3. 使用 `readelf -d libllama-android.so` 检查链接

### 问题5: Git Bash路径问题

如果遇到路径相关错误，尝试：

```bash
# 转换Windows路径为Unix格式
cygpath -u "C:/Users/Administrator/AppData/Local/Android/Sdk"
```

---

## 🎮 性能测试

编译完成后，建议进行性能对比：

### 测试方法
1. 使用相同模型（如 Qwen-1.5B-Q4_K_M）
2. 测试相同的prompt
3. 记录tokens/秒

### 预期提升
- **高通8Gen2+**: 2-5倍
- **高通8Gen1**: 1.5-3倍
- **联发科天玑9200+**: 2-4倍

---

## 📝 快速参考

### 完整流程（一键复制）

```bash
# 1. 配置脚本（先编辑 build_vulkan_android_windows.sh）
nano build_vulkan_android_windows.sh

# 2. 编译llama.cpp
./build_vulkan_android_windows.sh

# 3. 复制库文件
./copy_vulkan_libs.sh

# 4. 更新CMakeLists.txt
cp GGUFChat_CMakeLists_VULKAN.txt \
   GGUFChat/llama-android/src/main/cpp/CMakeLists.txt

# 5. 编译GGUFChat
cd GGUFChat && ./gradlew :llama-android:assembleRelease
```

### 检查清单

- [ ] Android Studio已安装
- [ ] NDK已安装（查看版本号）
- [ ] CMake已安装（查看版本号）
- [ ] 脚本配置正确（SDK路径、NDK版本、CMake版本）
- [ ] llama.cpp编译成功（生成5个so文件）
- [ ] 文件已复制到jniLibs
- [ ] CMakeLists.txt已更新
- [ ] GGUFChat编译成功
- [ ] APK包含libggml-vulkan.so
- [ ] 设备测试通过

---

## 🔗 相关资源

- [Android Studio下载](https://developer.android.com/studio)
- [Android NDK文档](https://developer.android.com/ndk)
- [Vulkan on Android](https://developer.android.com/ndk/guides/graphics/getting-started)
- [llama.cpp Vulkan文档](https://github.com/ggerganov/llama.cpp/blob/master/docs/build.md#vulkan)

---

## 💬 需要帮助？

如果遇到本文档未覆盖的问题：
1. 查看详细指南: `VULKAN_ANDROID_GUIDE.md`
2. 检查llama.cpp issue: https://github.com/ggerganov/llama.cpp/issues
3. 提供完整的错误日志以便诊断

祝编译成功！🚀
