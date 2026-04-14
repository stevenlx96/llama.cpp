# 📚 GGUF Chat Android Library - 编译构建指南

## 🎯 快速开始

### 📱 Demo 模式（推荐用于应用开发）

```bash
# 1. 进入项目目录
cd /data/source/ai-api/mobile-application-model/guf-chat

# 2. 执行编译
./gradlew :llama-android:assembleDemoRelease

# 3. 查看 AAR
ls -lh llama-android/build/outputs/aar/llama-android-demo-release.aar
```

**输出**：`llama-android-demo-release.aar` - 标准库模式

### 🎮 UE 模式（用于 UE/Unity 集成）

```bash
# 1. 进入项目目录
cd /data/source/ai-api/mobile-application-model/guf-chat

# 2. 执行编译（启用 fat-aar）
./gradlew :llama-android:assembleUeRelease -PenableFatAar=true

# 3. 查看 AAR
ls -lh llama-android/build/outputs/aar/llama-android-ue-release.aar
```

**输出**：`llama-android-ue-release.aar` (Fat-AAR) - 包含所有依赖

---

## 🔧 环境要求

| 软件 | 版本 | 必需 |
|------|------|------|
| JDK | 17 | ✅ |
| Android SDK | API 34 | ✅ |
| Android NDK | r21+ | ⚪ 可选 |
| Gradle | 7.6.3 | ✅ (自动下载) |
| CMake | 3.22.1+ | ✅ (自动下载) |

---

## 📝 详细步骤

### 编译模式选择

本项目支持两种编译模式：

| 模式 | 命令 | 文件大小 | 适用场景 |
|------|------|----------|----------|
| **Demo** | `./gradlew :llama-android:assembleDemoRelease` | ~2-3 MB | Demo 应用直接依赖 |
| **UE** | `./gradlew :llama-android:assembleUeRelease -PenableFatAar=true` | ~5-10 MB | UE/Unity 集成 |

**重要区别**：
- **Demo 模式**：标准 Android Library，依赖不打包到 AAR
- **UE 模式**：Fat-AAR，将所有依赖打包到 AAR 中，避免 ClassNotFoundException

### 步骤 1: 配置本地属性

创建 `local.properties` 文件：

```bash
cat > local.properties << 'EOF'
sdk.dir=/path/to/Android/Sdk
ndk.dir=/path/to/Android/Sdk/ndk/27.0.12077973
EOF
```

**示例路径**：
- Windows: `sdk.dir=C\\:\\Users\\\\\\Username\\\\AppData\\\\Local\\\\Android\\\\Sdk`
- Linux/Mac: `sdk.dir=/home/username/Android/Sdk`
- WSL: `sdk.dir=/mnt/c/Users/Username/AppData/Local/Android/Sdk`

### 步骤 2: 接受 Android SDK 许可证

```bash
mkdir -p licenses
echo -e "\n24333f8a63b6825ea9c5514f83c2829b004d1fee" > licenses/android-sdk-license
echo -e "\n504667f4c0de7973335447fc6ffe3056f2e7151a" > licenses/android-sdk-preview-license
echo -e "\n33b6a2b64607e8c07ff89e7f0467780c4d4c2a6" > licenses/google-android-ndk-license
```

### 步骤 3: 准备模型文件

在编译前，需要准备 GGUF 模型文件：

```bash
# 创建模型目录
mkdir -p llama-android/src/main/assets/models

# 下载或复制 GGUF 模型到该目录
# 例如：llama-2-7b-chat.Q4_K_M.gguf
```

### 步骤 4: 执行编译

#### Demo 模式编译（推荐用于应用开发）

```bash
# Release 版本
./gradlew :llama-android:assembleDemoRelease

# Debug 版本
./gradlew :llama-android:assembleDemoDebug
```

**输出**：
- `llama-android/build/outputs/aar/llama-android-demo-release.aar`
- `llama-android/build/outputs/aar/llama-android-demo-debug.aar`

#### UE 模式编译（用于 UE/Unity 集成）

```bash
# Release 版本（包含所有依赖）
./gradlew :llama-android:assembleUeRelease -PenableFatAar=true

# Debug 版本（包含所有依赖）
./gradlew :llama-android:assembleUeDebug -PenableFatAar=true
```

**输出**：
- `llama-android/build/outputs/aar/llama-android-ue-release.aar`
- `llama-android/build/outputs/aar/llama-android-ue-debug.aar`

### 步骤 5: 验证输出

```bash
# 查看所有 AAR 文件
ls -lh llama-android/build/outputs/aar/
```

#### Demo 模式验证

```bash
# Demo 模式的 AAR 体积较小（依赖未打包）
unzip -l llama-android/build/outputs/aar/llama-android-demo-release.aar | grep "\.jar"
```

**期望输出**（标准库模式，无嵌入依赖）：
```
classes.jar                          (约 100-200 KB)
```

#### UE 模式验证

```bash
# UE 模式的 AAR 包含所有依赖
unzip -l llama-android/build/outputs/aar/llama-android-ue-release.aar | grep "libs/.*\.jar"
```

**期望输出**（Fat-AAR 模式，包含所有依赖）：
```
libs/kotlin-stdlib-1.5.32.jar        (约 1.5 MB)
libs/kotlinx-coroutines-core-1.5.2.jar  (约 1 MB)
libs/androidx.core:core-ktx-1.8.0.jar   (约 200 KB)
...
```

---

## 🚀 常用命令

### 基础命令

| 命令 | 说明 |
|------|------|
| `./gradlew tasks` | 列出所有任务 |
| `./gradlew :llama-android:assembleDemoRelease` | 编译 Demo Release AAR |
| `./gradlew :llama-android:assembleUeRelease -PenableFatAar=true` | 编译 UE Fat-AAR |
| `./gradlew clean` | 清理构建 |
| `./gradlew :app:assembleDebug` | 编译 Demo 应用 |
| `./gradlew :demo:assembleDebug` | 编译 Demo 应用 |

### 模式切换

```bash
# Demo 模式（默认）
./gradlew :llama-android:assembleDemoRelease

# UE Fat-AAR 模式（需要参数）
./gradlew :llama-android:assembleUeRelease -PenableFatAar=true
```

### 高级命令

```bash
# 强制重新编译（不使用缓存）
./gradlew :llama-android:assembleDemoRelease --rerun-tasks
./gradlew :llama-android:assembleUeRelease -PenableFatAar=true --rerun-tasks

# 查看详细日志
./gradlew :llama-android:assembleDemoRelease --info
./gradlew :llama-android:assembleUeRelease -PenableFatAar=true --info

# 并行构建（加速）
./gradlew :llama-android:assembleDemoRelease --parallel
./gradlew :llama-android:assembleUeRelease -PenableFatAar=true --parallel

# 编译所有变体
./gradlew :llama-android:assemble
```

### 便捷任务

```bash
# 使用便捷任务打包
./gradlew :llama-android:packDemoAar
./gradlew :llama-android:packUeFatAar -PenableFatAar=true

# 查看构建帮助
./gradlew :llama-android:printBuildHelp
```

---

## ❓ 常见问题

### 1. SDK location not found

**错误**：`SDK location not found`

**解决**：
```bash
cat > local.properties << EOF
sdk.dir=/path/to/Android/Sdk
EOF
```

### 2. Could not resolve dependencies

**错误**：`Could not resolve: androidx.core:core-ktx:1.8.0`

**解决**：
```bash
./gradlew clean --refresh-dependencies
./gradlew :llama-android:assembleDemoRelease
```

### 3. OutOfMemoryError

**错误**：`java.lang.OutOfMemoryError: Java heap space`

**解决**：
```bash
echo "org.gradle.jvmargs=-Xmx4096m" >> gradle.properties
```

### 4. gradlew 权限错误

**错误**：`Permission denied: ./gradlew`

**解决**：
```bash
chmod +x gradlew
sed -i 's/\r$//' gradlew
```

### 5. CMake not found

**错误**：`CMake was not found in the SDK`

**解决**：
```bash
# 通过 SDK Manager 安装 CMake
# 或在 local.properties 中指定路径
echo "cmake.dir=/path/to/cmake" >> local.properties
```

### 6. NDK not found

**错误**：`NDK not found`

**解决**：
```bash
# 在 local.properties 中指定 NDK 路径
echo "ndk.dir=/path/to/Android/Sdk/ndk/27.0.12077973" >> local.properties
```

### 7. 模型文件未找到

**错误**：`Model file not found`

**解决**：
```bash
# 确保模型文件在正确位置
ls -la llama-android/src/main/assets/models/

# 或在运行时指定模型路径
```

### 8. gradlew 执行失败

**错误**：`gradlew: command not found`

**解决**：
```bash
# Linux/Mac
chmod +x gradlew
./gradlew tasks

# Windows
gradlew.bat tasks
```

### 9. 如何选择编译模式？

**Demo 模式**：适用于
- Android 应用直接依赖 Library
- 需要最小化 APK 体积
- 正常的 Android 开发流程

**UE 模式**：适用于
- UE/Unity 集成
- 需要独立部署的 AAR
- 避免依赖冲突的场景

### 10. UE 集成 ClassNotFoundException

**问题**：UE 集成时出现 `ClassNotFoundException: kotlinx.coroutines...`

**原因**：使用了标准库 AAR 而非 Fat-AAR

**解决**：
```bash
# 使用 UE 模式编译（包含所有依赖）
./gradlew :llama-android:assembleUeRelease -PenableFatAar=true

# 验证依赖已打包
unzip -l llama-android/build/outputs/aar/llama-android-ue-release.aar | grep "kotlin"
```

---

## 📊 技术规格

### 项目配置

| 配置项 | 值 |
|--------|-----|
| Gradle | 7.6.3 |
| AGP | 8.4.0 |
| Kotlin | 1.5.32 |
| compileSdk | 34 |
| minSdk | 24 |
| targetSdk | 34 |
| JDK | 17 |

### 依赖版本

```groovy
kotlin-stdlib:1.5.32
androidx.core:core-ktx:1.8.0
androidx.appcompat:appcompat:1.4.2
androidx.lifecycle:lifecycle-runtime-ktx:2.5.1
kotlinx-coroutines-android:1.5.2
kotlinx-coroutines-core:1.5.2
```

### 支持架构

- `arm64-v8a` (64 位 ARM)

---

## 🎯 输出文件

### 目录结构

```
llama-android/build/outputs/aar/
├── llama-android-demo-release.aar        # Demo Release (~2-3 MB)
├── llama-android-demo-debug.aar          # Demo Debug (~2-3 MB)
├── llama-android-ue-release.aar          # UE Release (~5-10 MB)
└── llama-android-ue-debug.aar            # UE Debug (~5-10 MB)
```

### 文件对比

| 文件 | 大小 | 模式 | 依赖处理 |
|------|------|------|----------|
| `llama-android-demo-release.aar` | ~2-3 MB | 标准库 | 依赖不打包 |
| `llama-android-ue-release.aar` | ~5-10 MB | Fat-AAR | 依赖已打包 |

### AAR 内容

#### Demo 模式 AAR 结构

```
llama-android-demo-release.aar
├── AndroidManifest.xml
├── classes.jar           # 编译后的代码（不含依赖）
├── assets/               # 模型文件
├── res/                  # 资源文件
├── jni/                  # Native 库
│   └── arm64-v8a/libllama-android.so
└── R.txt                 # 资源索引
```

#### UE 模式 AAR 结构

```
llama-android-ue-release.aar
├── AndroidManifest.xml
├── classes.jar           # 编译后的代码
├── libs/                 # 嵌入的依赖（Fat-AAR）
│   ├── kotlin-stdlib-1.5.32.jar
│   ├── androidx.core:core-ktx-1.8.0.jar
│   ├── kotlinx-coroutines-core-1.5.2.jar
│   └── kotlinx-coroutines-android-1.5.2.jar
├── assets/               # 模型文件
├── res/                  # 资源文件
├── jni/                  # Native 库
│   └── arm64-v8a/libllama-android.so
└── R.txt                 # 资源索引
```

---

## 🔗 相关文档

- [UE 集成调用指南](UE_INTEGRATION_GUIDE.md) - UE 集成完整流程
- [Java API 调用指南](JAVA_API_GUIDE.md) - Java/Kotlin API 详细说明
- [项目总览](README.md) - 项目概述

---

**文档版本**: v1.0
**最后更新**: 2026-04-14
**更新内容**: 初始版本
