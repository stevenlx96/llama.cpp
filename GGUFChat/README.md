# 📖 GGUF Chat Android Library - 文档中心

## 🎯 项目概述

这是一个支持 Unreal Engine (UE) 集成的 Android GGUF Chat 聊天库，基于 Llama.cpp 实现。

### ✨ 核心特性

- ✅ **本地推理**：完全本地运行，无需网络连接
- ✅ **GGUF 格式支持**：支持 GGUF 格式的 LLM 模型
- ✅ **多轮对话**：支持上下文对话历史
- ✅ **流式生成**：实时返回生成结果
- ✅ **Fat AAR 打包**：所有依赖已打包，无需额外配置
- ✅ **Java 友好 API**：提供同步/异步方法，支持 UE JNI 调用
- ✅ **线程安全回调**：所有回调在主线程执行
- ✅ **灵活配置**：支持温度、Top-p、最大 Token 等参数配置

### 📊 技术规格

| 项目 | 规格 |
|------|------|
| **Gradle** | 7.6.3 |
| **AGP** | 8.4.0 |
| **Kotlin** | 1.9.23 |
| **JDK** | 17 |
| **compileSdk** | 34 |
| **minSdk** | 24 (Android 7.0) |
| **targetSdk** | 34 (Android 14) |
| **架构支持** | arm64-v8a |
| **AAR 大小** | ~2-5 MB (取决于依赖) |

---

## 📚 文档导航

### 🚀 快速开始

| 文档 | 说明 | 适用场景 |
|------|------|---------|
| **[BUILD_GUIDE.md](docs/BUILD_GUIDE.md)** | 📦 编译构建指南 | 如何编译生成 AAR 文件 |
| **[UE_INTEGRATION_GUIDE.md](docs/UE_INTEGRATION_GUIDE.md)** | 🎮 UE 集成调用指南 | 如何在 UE 中集成使用 |
| **[JAVA_API_GUIDE.md](docs/JAVA_API_GUIDE.md)** | ☕ Java API 调用指南 | Java/Kotlin API 详细说明 |

---

## 🚀 快速使用

### 编译 AAR（3 步）

```bash
# 1. 进入项目目录
cd /data/source/ai-api/mobile-application-model/guf-chat

# 2. 执行编译
./gradlew :llama-android:assembleDemoRelease

# 3. 查看 AAR
ls -lh llama-android/build/outputs/aar/llama-android-demo-release.aar
```

**输出**：`llama-android-demo-release.aar`

### UE 集成（推荐使用 LlamaHelper）

```cpp
// 使用 LlamaHelper（推荐，仅需 3 行代码！）
JNIEnv* Env = FAndroidApplication::GetJavaEnv();
jclass HelperClass = Env->FindClass("com/stdemo/ggufchat/LlamaHelper");
jmethodID Create = Env->GetStaticMethodID(HelperClass, "create",
                                          "()Lcom/stdemo/ggufchat/LlamaHelper;");
jobject Helper = Env->CallStaticObjectMethod(HelperClass, Create);
// 自动获取 Context，无需手动处理！

// 详细文档: [docs/JAVA_API_GUIDE.md](docs/JAVA_API_GUIDE.md)
```

---

## 📂 项目结构

```
guf-chat/
├── README.md                         # 📖 项目总览
├── docs/                             # 📚 文档目录
│   ├── BUILD_GUIDE.md                # 📦 编译构建指南（推荐）
│   ├── UE_INTEGRATION_GUIDE.md       # 🎮 UE 集成调用指南（推荐）
│   ├── JAVA_API_GUIDE.md             # ☕ Java API 调用指南
│   └── OPTIMIZATION_SUMMARY.md       # 🎯 优化总结
├── build.gradle                      # 根构建文件
├── settings.gradle                   # 项目设置
├── gradle.properties                 # Gradle 配置
├── local.properties                  # 本地配置（SDK 路径）
├── gradlew                           # Gradle Wrapper 脚本
├── gradlew.bat                       # Windows 批处理脚本
├── gradle/
│   └── wrapper/
│       ├── gradle-wrapper.jar       # Gradle Wrapper JAR
│       └── gradle-wrapper.properties # Gradle 版本配置
├── llama-android/                    # 🎯 核心库模块
│   ├── build.gradle                  # Library 构建文件
│   ├── src/main/
│   │   ├── java/com/stdemo/ggufchat/
│   │   │   ├── LlamaHelper.kt            # ⭐ UE 集成辅助类（推荐使用）
│   │   │   ├── LlamaManagerJava.kt       # Java 友好的 API（主要接口）
│   │   │   ├── LlamaEngine.kt            # 核心 Chat 引擎
│   │   │   ├── ModelManager.kt           # 模型管理
│   │   │   └── ModelDownloader.kt        # 模型下载
│   │   ├── jniLibs/                # JNI 库（.so 文件）
│   │   ├── cpp/                     # C++ 源代码
│   │   │   └── CMakeLists.txt
│   │   └── res/                    # 资源文件
│   └── build/outputs/aar/
│       ├── llama-android-demo-release.aar  # Demo Release
│       └── llama-android-ue-release.aar     # UE Release (Fat-AAR)
├── app/                              # 应用模块
└── demo/                             # Demo 应用模块
```

---

## 🔧 编译命令速查

### 基础命令

```bash
# 查看 Gradle 任务
./gradlew tasks

# 编译 Demo Release AAR
./gradlew :llama-android:assembleDemoRelease

# 编译 UE Fat-AAR
./gradlew :llama-android:assembleUeRelease -PenableFatAar=true

# 清理构建
./gradlew clean

# 查看项目结构
./gradlew projects
```

### 高级命令

```bash
# 强制重新编译
./gradlew :llama-android:assembleDemoRelease --rerun-tasks

# 查看详细日志
./gradlew :llama-android:assembleDemoRelease --info

# 并行构建（加速）
./gradlew :llama-android:assembleDemoRelease --parallel
```

---

## 🎯 UE 集成步骤概览

### 步骤 1: 添加 AAR 文件

```bash
mkdir -p YourUEProject/Plugins/GGUFChat/Source/ThirdParty/Android/libs
cp llama-android/build/outputs/aar/llama-android-ue-release.aar \
   YourUEProject/Plugins/GGUFChat/Source/ThirdParty/Android/libs/
```

### 步骤 2: 配置 build.gradle

```groovy
dependencies {
    implementation(name: 'llama-android-ue-release', ext: 'aar')
}

repositories {
    flatDir {
        dirs 'src/ThirdParty/Android/libs'
    }
}
```

### 步骤 3: C++ 调用

```cpp
// 使用 LlamaHelper（推荐）
jclass HelperClass = Env->FindClass("com/stdemo/ggufchat/LlamaHelper");
jmethodID CreateMethod = Env->GetStaticMethodID(HelperClass, "create",
                                                "()Lcom/stdemo/ggufchat/LlamaHelper;");
jobject Helper = Env->CallStaticObjectMethod(HelperClass, CreateMethod);

// 初始化
jmethodID InitMethod = Env->GetMethodID(HelperClass, "initialize", "()Z");
jboolean Success = Env->CallBooleanMethod(Helper, InitMethod);

// 发送消息
jmethodID SendMethod = Env->GetMethodID(HelperClass, "sendMessage",
                                        "(Ljava/lang/String;)V");
jstring Message = Env->NewStringUTF("你好，请介绍一下自己");
Env->CallVoidMethod(Helper, SendMethod, Message);
```

详细代码示例请参考 **[UE 集成调用指南](docs/UE_INTEGRATION_GUIDE.md)**。

---

## 📋 API 速查

### LlamaHelper（推荐用于 UE 集成）

| 方法 | 说明 | 返回值 |
|------|------|--------|
| `create()` | 创建实例（自动获取 Context） | LlamaHelper |
| `initialize()` | 初始化 Chat | boolean |
| `initialize(modelPath)` | 使用指定模型初始化 | boolean |
| `sendMessage(message)` | 发送消息 | void |
| `startNewChat()` | 开始新对话 | void |
| `stopGeneration()` | 停止生成 | void |
| `release()` | 释放资源 | void |
| `isInitialized()` | 是否已初始化 | boolean |
| `isGenerating()` | 是否正在生成 | boolean |
| `getInfo()` | 获取引擎信息 | String |
| `setOnResponse(callback)` | 设置结果回调 | void |

详细文档：[docs/JAVA_API_GUIDE.md](docs/JAVA_API_GUIDE.md)

### LlamaManagerJava 主要方法

| 方法 | 说明 | 返回值 |
|------|------|--------|
| `initializeSync()` | 同步初始化 | boolean |
| `initializeSync(modelPath)` | 使用指定模型初始化 | boolean |
| `initializeAsync(callback)` | 异步初始化 | void |
| `sendMessage(message)` | 发送消息 | void |
| `startNewChat()` | 开始新对话 | void |
| `stopGeneration()` | 停止生成 | void |
| `isModelReady()` | 检查模型是否就绪 | boolean |
| `getState()` | 获取当前状态 | String |
| `setGenerationConfig(...)` | 设置生成参数 | void |
| `setSystemPrompt(prompt)` | 设置系统提示词 | void |
| `release()` | 释放资源 | void |

### 回调接口

| 接口 | 方法 | 触发时机 |
|------|------|---------|
| `LlamaCallback<String>` | `onSuccess(String)` | 生成成功 |
| `LlamaCallback<String>` | `onError(Throwable)` | 生成失败 |
| `StateChangedCallback` | `onStateChanged(String)` | 状态变化 |
| `ErrorCallback` | `onError(String)` | 发生错误 |

---

## ❓ 常见问题速查

### 编译问题

| 问题 | 解决方案 |
|------|---------|
| SDK location not found | 创建 `local.properties` 并设置 `sdk.dir` |
| Could not resolve dependencies | 清理并重新构建 `./gradlew clean --refresh-dependencies` |
| OutOfMemoryError | 增加内存 `org.gradle.jvmargs=-Xmx4096m` |
| gradlew 权限错误 | `chmod +x gradlew && sed -i 's/\r$//' gradlew` |
| AAR 文件过小 | 检查是否使用了 UE 模式编译 |

### UE 集成问题

| 问题 | 解决方案 |
|------|---------|
| ClassNotFoundException | 检查 AAR 文件位置和 build.gradle 配置 |
| MethodNotFound | 使用 `javap -s` 查看正确的方法签名 |
| 回调未触发 | 实现 `JNI_OnLoad` 并注册 native 方法 |
| 内存泄漏 | 确保调用 `DeleteGlobalRef` 释放 JNI 对象 |

详细解决方案请参考各文档的 **常见问题** 章节。

---

## 🔄 版本历史

### v1.0 (2026-04-14)

**初始版本**：
- ✅ 支持 UE 集成
- ✅ Fat AAR 打包
- ✅ Java 友好的 API
- ✅ 本地推理支持
- ✅ 多轮对话
- ✅ 流式生成
- ✅ 参考 asr-onnx 项目优化

---

## 🔗 相关资源

### 技术栈

- **[Llama.cpp](https://github.com/ggerganov/llama.cpp)** - LLM 推理引擎
- **[Fat AAR](https://github.com/kezong/fat-aar)** - Android 库打包插件
- **[Unreal Engine](https://docs.unrealengine.com/)** - 游戏引擎
- **[Android JNI](https://developer.android.com/ndk/guides/jni)** - Java Native Interface |

### 参考项目

- `asr-onnx/` - ASR Android 库（参考配置和架构）

---

## 💡 使用建议

1. **首次使用**：阅读 **[docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md)** 了解编译细节
2. **UE 集成**：参考 **[docs/UE_INTEGRATION_GUIDE.md](docs/UE_INTEGRATION_GUIDE.md)** 进行集成
3. **API 调用**：查阅 **[docs/JAVA_API_GUIDE.md](docs/JAVA_API_GUIDE.md)** 了解 API 详情

---

## 📞 技术支持

如需帮助：
1. 查阅本文档和相关指南
2. 检查日志输出
3. 提交 Issue 并附上完整的错误日志

---

## 📄 许可证

本库基于 Llama.cpp 和相关开源项目构建。请参考各组件的许可证。

---

**文档版本**: v1.0
**最后更新**: 2026-04-14
**维护者**: GGUF Chat Team

🎉 祝你使用愉快！
