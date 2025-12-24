# GGUF Chat Demo Project

这是一个**超级精简的**Android示例项目，展示如何使用`llama-android-debug.aar`库来创建一个功能完整的GGUF模型聊天应用。

## 🎯 项目特点

- ✅ **超级精简**：仅2个Kotlin文件（417行代码）
- ✅ **100%依赖AAR**：所有核心功能全部来自aar包
- ✅ **功能完整**：与主项目功能完全相同
- ✅ **易于理解**：最佳AAR库集成示例

## 📁 项目结构

```
demo/
└── app/src/main/java/com/stdemo/ggufchat/demo/
    ├── MainActivity.kt      362行  - 直接调用AAR API
    └── MessageAdapter.kt     55行  - UI适配器
    ────────────────────────────
    总计：417行代码，仅2个文件！
```

## 📦 AAR包内容（来自llama-android模块）

AAR包已包含**所有**核心功能，demo项目直接调用：

| 类名 | 说明 | 用途 |
|------|------|------|
| `GGUFChatEngine` | 聊天引擎 | 加载模型、生成回复、参数配置 |
| `Message` | 消息数据类 | 存储聊天消息 |
| `ModelDownloader` | 模型下载器 | 从ModelScope下载模型 |
| `ModelManager` | 模型扫描器 | 扫描本地.gguf模型文件 |
| `ChatPromptBuilder` | Prompt构建器 | 内部使用，构建ChatML格式 |
| `ChatConfig` | 配置类 | 存储temperature等参数 |
| Native库(.so) | JNI封装 | llama.cpp核心引擎 |

## 💡 Demo项目的2个文件

### 1. MainActivity.kt（362行）

主Activity，**只负责UI逻辑**，所有核心功能都调用AAR：

```kotlin
class MainActivity : AppCompatActivity() {
    private val engine = GGUFChatEngine()  // 来自AAR
    private val downloader = ModelDownloader()  // 来自AAR
    private val modelManager = ModelManager(modelsDir)  // 来自AAR
    private val messages = mutableListOf<Message>()  // 来自AAR

    // 发送消息
    private fun sendMessage(text: String) {
        lifecycleScope.launch {
            engine.generate(userInput = text, onTokenGenerated = { token ->
                // 流式更新UI
            })
        }
    }

    // 下载模型
    private fun downloadModel(modelScopeId: String, fileName: String) {
        downloader.downloadModel(modelScopeId, fileName, modelDir,
            listener = object : ModelDownloader.DownloadProgressListener {
                override fun onProgress(...) { }
                override fun onSuccess(filePath: String) { loadModel(filePath) }
                override fun onError(message: String) { }
            }
        )
    }

    // 扫描本地模型
    private fun tryLoadModel() {
        val models = modelManager.scanModels().filter { it.isValid }
        // 显示模型列表或自动加载
    }
}
```

### 2. MessageAdapter.kt（55行）

RecyclerView适配器，使用AAR中的`Message`类：

```kotlin
class MessageAdapter : ListAdapter<Message, MessageViewHolder>(MessageDiffCallback()) {
    // 使用com.stdemo.ggufchat.Message（来自AAR）
}
```

## ✨ 主要功能

所有功能都来自AAR包，demo只负责UI：

### 1. 模型管理（ModelManager）
- 自动扫描`models`目录中的GGUF模型文件
- 支持多个模型时提供选择界面
- 验证模型文件有效性（最小50MB）

### 2. 模型下载（ModelDownloader）
- 从ModelScope在线下载GGUF模型
- 实时显示下载进度
- 用户可自定义ModelScope ID和文件名

### 3. 聊天功能（GGUFChatEngine）
- 流式和非流式两种输出模式
- 自动管理对话历史
- 支持中途停止模型生成
- 一键清除所有对话历史

### 4. 参数调整（ChatConfig）
- Temperature：控制输出随机性
- Max Tokens：设置最大生成Token数
- Max History Pairs：对话历史轮数
- 其他LLM参数

## 🚀 使用方法

### 1. 构建项目

```bash
cd demo
./gradlew build
```

### 2. 安装到设备

```bash
./gradlew installDebug
```

### 3. 使用应用

1. **首次启动**：授予存储权限
2. **下载模型**：点击"Download"按钮
   - ModelScope ID: `Qwen/Qwen2.5-1.5B-Instruct-GGUF`
   - 文件名: `qwen2.5-1.5b-instruct-q4_k_m.gguf`
3. **开始聊天**：模型加载后即可使用

## 📝 AAR库使用示例

### 添加AAR依赖

```kotlin
// app/build.gradle.kts
dependencies {
    implementation(fileTree(mapOf("dir" to "libs", "include" to listOf("*.aar"))))
}
```

### 导入AAR中的类

```kotlin
import com.stdemo.ggufchat.GGUFChatEngine
import com.stdemo.ggufchat.ChatConfig
import com.stdemo.ggufchat.Message
import com.stdemo.ggufchat.ModelDownloader
import com.stdemo.ggufchat.ModelManager
```

### 使用示例

```kotlin
// 1. 扫描本地模型
val modelManager = ModelManager("/path/to/models")
val models = modelManager.scanModels()
models.forEach { model ->
    println("${model.name} - ${model.sizeMB}MB")
}

// 2. 加载模型
val engine = GGUFChatEngine()
lifecycleScope.launch {
    val result = engine.loadModel("/path/to/model.gguf")
    if (result.isSuccess) {
        println("模型加载成功")
    }
}

// 3. 生成回复（流式）
lifecycleScope.launch {
    engine.generate(
        userInput = "你好",
        onTokenGenerated = { token ->
            // 每个token生成时调用
            print(token)
        }
    )
}

// 4. 生成回复（非流式）
engine.setStreamingMode(false)
lifecycleScope.launch {
    val result = engine.generate("你好")
    println(result.getOrNull())
}

// 5. 调整参数
engine.setTemperature(0.8f)
engine.setMaxTokens(512)
engine.setMaxHistoryPairs(10)

// 6. 下载模型
val downloader = ModelDownloader()
downloader.downloadModel(
    modelScopeId = "Qwen/Qwen2.5-1.5B-Instruct-GGUF",
    fileName = "qwen2.5-1.5b-instruct-q4_k_m.gguf",
    downloadDir = "/path/to/models",
    listener = object : ModelDownloader.DownloadProgressListener {
        override fun onProgress(percentage: Int, downloadedBytes: Long, totalBytes: Long) {
            println("下载进度: $percentage%")
        }
        override fun onSuccess(filePath: String) {
            println("下载成功: $filePath")
        }
        override fun onError(message: String) {
            println("下载失败: $message")
        }
    }
)

// 7. 释放资源
engine.release()
```

## 🔧 技术栈

- **Kotlin**：主要编程语言
- **Coroutines**：异步编程
- **ViewBinding**：视图绑定
- **RecyclerView**：消息列表
- **Material Design 3**：UI设计

## 📋 系统要求

- **最低SDK版本**：26 (Android 8.0)
- **目标SDK版本**：34 (Android 14)
- **支持架构**：arm64-v8a, armeabi-v7a

## 🎓 推荐模型

### 新手推荐
- **Qwen2.5-1.5B-Instruct-GGUF**
  - ModelScope ID: `Qwen/Qwen2.5-1.5B-Instruct-GGUF`
  - 文件名: `qwen2.5-1.5b-instruct-q4_k_m.gguf`
  - 大小: 约1GB
  - 速度: 快

### 平衡性能
- **Qwen2.5-3B-Instruct-GGUF**
  - ModelScope ID: `Qwen/Qwen2.5-3B-Instruct-GGUF`
  - 文件名: `qwen2.5-3b-instruct-q4_k_m.gguf`
  - 大小: 约2GB

## 🆚 与主项目的区别

| 特性 | 主项目 | Demo项目 |
|------|--------|----------|
| 依赖方式 | 源代码模块 | AAR包 |
| 包名 | `com.stdemo.ggufchat` | `com.stdemo.ggufchat.demo` |
| Kotlin文件数 | 10+ | **仅2个** ⭐ |
| 代码行数 | 2000+ | **仅417行** ⭐ |
| 功能 | 完整 | 完整 |
| 性能 | 完全相同 | 完全相同 |
| ModelManager | 在app模块 | **在AAR中** ⭐ |

## ❓ 常见问题

### Q: 为什么demo只有2个Kotlin文件？
A: 因为**所有**核心功能（GGUFChatEngine、Message、ModelDownloader、ModelManager等）都已经打包在`llama-android-debug.aar`中了！demo项目只需要：
- MainActivity：调用AAR中的API，处理UI逻辑
- MessageAdapter：RecyclerView适配器

### Q: AAR包里都有什么？
A: AAR包含：
- ✅ GGUFChatEngine（聊天引擎）
- ✅ Message（消息类）
- ✅ ModelDownloader（下载器）
- ✅ ModelManager（模型扫描器）
- ✅ ChatPromptBuilder（Prompt构建）
- ✅ ChatConfig（配置类）
- ✅ Native库（.so文件）

### Q: 模型加载失败？
A: 确保：
1. 模型文件完整下载
2. 文件大小至少50MB
3. 文件扩展名为`.gguf`

### Q: 如何修改AAR包中的功能？
A: 需要修改`llama-android`模块的源代码，然后重新编译AAR包。

## 📄 许可证

本demo项目遵循与主项目相同的许可证。
