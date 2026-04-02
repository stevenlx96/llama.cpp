# GGUFChat AAR API 文档（当前版本）

**生成时间**: 2025-12-18
**AAR包名**: `llama-android-debug.aar`
**包名**: `com.stdemo.ggufchat`

---

## 📦 AAR包含的所有类

| 类名 | 文件 | 说明 |
|------|------|------|
| `GGUFChatEngine` | LlamaEngine.kt | 核心聊天引擎 |
| `ChatConfig` | LlamaEngine.kt | 配置数据类 |
| `Message` | Message.kt | 消息数据类 |
| `ModelDownloader` | ModelDownloader.kt | 模型下载器 |
| `ModelManager` | ModelManager.kt | 模型管理器（扫描、验证） |
| `ChatPromptBuilder` | ChatPromptBuilder.kt | 内部Prompt构建器（internal） |
| Native库 | llama-android.so | JNI封装的llama.cpp |

---

## 1️⃣ GGUFChatEngine - 核心聊天引擎

### 构造函数
```kotlin
val engine = GGUFChatEngine()
```

### 主要方法

#### 模型加载
```kotlin
suspend fun loadModel(path: String): Result<Unit>
```
- **功能**: 加载GGUF模型文件
- **参数**:
  - `path`: 模型文件的完整路径
- **返回**: `Result<Unit>` - 成功或失败
- **示例**:
```kotlin
lifecycleScope.launch {
    val result = engine.loadModel("/path/to/model.gguf")
    if (result.isSuccess) {
        println("模型加载成功")
    }
}
```

#### 生成回复
```kotlin
suspend fun generate(
    userInput: String,
    onTokenGenerated: ((String) -> Unit)? = null
): Result<String>
```
- **功能**: 生成AI回复
- **参数**:
  - `userInput`: 用户输入的消息
  - `onTokenGenerated`: 可选的token流式回调（仅在streaming模式下有效）
- **返回**: `Result<String>` - 完整的AI回复
- **示例（流式）**:
```kotlin
lifecycleScope.launch {
    engine.generate(
        userInput = "你好",
        onTokenGenerated = { token ->
            print(token)  // 逐个token输出
        }
    )
}
```
- **示例（非流式）**:
```kotlin
engine.setStreamingMode(false)
lifecycleScope.launch {
    val result = engine.generate("你好")
    println(result.getOrNull())
}
```

### 配置管理方法

#### 获取/设置配置
```kotlin
fun getConfig(): ChatConfig
fun setConfig(config: ChatConfig)
```

#### 单项配置设置
```kotlin
fun setSystemPrompt(prompt: String)
fun setTemperature(temperature: Float)     // >= 0
fun setTopP(topP: Float)                   // 0-1之间
fun setTopK(topK: Int)                     // > 0
fun setMaxTokens(maxTokens: Int)           // > 0
fun setMaxHistoryPairs(maxPairs: Int)      // >= 0
```

#### 流式模式控制
```kotlin
fun setStreamingMode(enabled: Boolean)
fun isStreamingModeEnabled(): Boolean
```
- **默认**: 流式模式开启（true）

### 生成控制

```kotlin
fun stopGeneration()           // 停止当前生成
fun isGenerating(): Boolean    // 检查是否正在生成
```

### 历史管理

```kotlin
fun clearHistory()             // 清除对话历史
fun getHistorySize(): Int      // 获取历史对话轮数
```

### 状态查询

```kotlin
fun isModelLoaded(): Boolean   // 模型是否已加载
fun getModelInfo(): String     // 获取模型信息字符串
```

### 资源释放

```kotlin
fun release()                  // 释放模型资源
```
- **重要**: 在Activity/Fragment的`onDestroy()`中调用

---

## 2️⃣ ChatConfig - 配置数据类

### 定义
```kotlin
data class ChatConfig(
    val temperature: Float = 0.7f,
    val topP: Float = 0.9f,
    val topK: Int = 40,
    val maxTokens: Int = 512,
    val maxHistoryPairs: Int = 10,
    val systemPrompt: String = "你叫小达，是一个有帮助的ai机器人助手，请用简体中文回答问题。"
)
```

### 参数说明

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `temperature` | Float | 0.7 | 输出随机性（0=确定性，越高越随机） |
| `topP` | Float | 0.9 | 核采样概率阈值（0-1） |
| `topK` | Int | 40 | 采样时考虑的top-k个token |
| `maxTokens` | Int | 512 | 单次生成的最大token数 |
| `maxHistoryPairs` | Int | 10 | 保留的对话历史轮数 |
| `systemPrompt` | String | 见上 | 系统提示词 |

### 使用示例
```kotlin
val config = ChatConfig(
    temperature = 0.8f,
    maxTokens = 1024,
    systemPrompt = "You are a helpful assistant."
)
engine.setConfig(config)
```

---

## 3️⃣ Message - 消息数据类

### 定义
```kotlin
data class Message(
    val content: String,
    val isUser: Boolean,
    val timestamp: Long = System.currentTimeMillis()
)
```

### 参数说明

| 参数 | 类型 | 说明 |
|------|------|------|
| `content` | String | 消息内容 |
| `isUser` | Boolean | true=用户消息，false=AI消息 |
| `timestamp` | Long | 消息时间戳（自动生成） |

### 使用示例
```kotlin
val userMsg = Message("你好", isUser = true)
val aiMsg = Message("你好！有什么可以帮助你的？", isUser = false)
```

---

## 4️⃣ ModelDownloader - 模型下载器

### 构造函数
```kotlin
val downloader = ModelDownloader()
```

### 主要方法

#### 下载模型
```kotlin
suspend fun downloadModel(
    modelScopeId: String,
    fileName: String,
    downloadDir: String,
    listener: DownloadProgressListener? = null
): Result<String>
```

- **参数**:
  - `modelScopeId`: ModelScope模型ID（如 `"Qwen/Qwen2.5-1.5B-Instruct-GGUF"`）
  - `fileName`: 文件名（如 `"qwen2.5-1.5b-instruct-q4_k_m.gguf"`）
  - `downloadDir`: 下载目录路径
  - `listener`: 可选的进度监听器
- **返回**: `Result<String>` - 成功时返回文件完整路径

#### 构建下载URL
```kotlin
fun buildDownloadUrl(modelScopeId: String, fileName: String): String
```
- **返回**: 完整的ModelScope下载URL

### DownloadProgressListener 接口
```kotlin
interface DownloadProgressListener {
    fun onProgress(percentage: Int, downloadedBytes: Long, totalBytes: Long)
    fun onSuccess(filePath: String)
    fun onError(message: String)
}
```

### 使用示例
```kotlin
val downloader = ModelDownloader()
lifecycleScope.launch {
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
}
```

### 常量
- `CONNECT_TIMEOUT`: 60000ms (60秒)
- `READ_TIMEOUT`: 300000ms (5分钟)
- `BUFFER_SIZE`: 65536字节 (64KB)
- `MODELSCOPE_BASE_URL`: `"https://www.modelscope.cn/models"`

---

## 5️⃣ ModelManager - 模型管理器

### 构造函数
```kotlin
val modelManager = ModelManager(modelsDir: String)
```
- **参数**: `modelsDir` - 模型目录路径（如果不存在会自动创建）

### ModelInfo 数据类
```kotlin
data class ModelInfo(
    val name: String,           // 文件名
    val path: String,           // 完整路径
    val sizeBytes: Long,        // 文件大小（字节）
    val sizeMB: Long,           // 文件大小（MB）
    val lastModified: Long,     // 最后修改时间
    val isValid: Boolean        // 是否有效（>= 50MB）
)
```

### 主要方法

#### 扫描模型
```kotlin
fun scanModels(): List<ModelInfo>
```
- **功能**: 扫描目录中所有.gguf文件
- **返回**: 按最后修改时间倒序排列的模型列表

#### 获取第一个有效模型
```kotlin
fun getFirstValidModel(): ModelInfo?
```
- **功能**: 获取第一个有效模型（用于自动加载）
- **返回**: 第一个有效模型或null

#### 按名称获取模型
```kotlin
fun getModel(modelName: String): ModelInfo?
```

#### 验证模型
```kotlin
fun validateModel(path: String): Boolean
```

#### 删除模型
```kotlin
fun deleteModel(path: String): Boolean
```

#### 统计信息
```kotlin
fun getTotalSize(): Long         // 获取所有模型的总大小（字节）
fun getModelCount(): Int         // 获取模型总数
fun getValidModelCount(): Int    // 获取有效模型数
```

#### 工具方法
```kotlin
fun formatSize(bytes: Long): String              // 格式化文件大小（"1 GB", "500 MB"等）
fun getModelsDescription(): String               // 获取所有模型的描述（用于调试）
fun getDetailedInfo(modelPath: String): String   // 获取单个模型的详细信息
```

### 使用示例
```kotlin
val modelManager = ModelManager("/path/to/models")

// 扫描所有模型
val models = modelManager.scanModels()
models.forEach { model ->
    println("${model.name} - ${model.sizeMB}MB - Valid: ${model.isValid}")
}

// 自动加载第一个有效模型
val firstModel = modelManager.getFirstValidModel()
if (firstModel != null) {
    engine.loadModel(firstModel.path)
}

// 获取模型列表描述
println(modelManager.getModelsDescription())

// 删除模型
modelManager.deleteModel("/path/to/old-model.gguf")
```

### 常量
- `GGUF_EXTENSION`: `".gguf"`
- `MIN_MODEL_SIZE_MB`: 50 (最小有效模型大小为50MB)

---

## 6️⃣ ChatPromptBuilder（内部类）

**注意**: 此类标记为`internal`，不应在AAR外部直接使用。它被`GGUFChatEngine`内部使用。

### 功能
1. 构建ChatML格式的Prompt
2. 清理模型响应中的ChatML标记
3. 管理对话历史

---

## 📝 完整使用示例

### 基本流程
```kotlin
class MainActivity : AppCompatActivity() {
    private val engine = GGUFChatEngine()
    private val modelManager = ModelManager(modelsDir)
    private val downloader = ModelDownloader()
    private val messages = mutableListOf<Message>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val modelsDir = getExternalFilesDir("models")?.absolutePath ?: return
        modelManager = ModelManager(modelsDir)

        // 1. 扫描本地模型
        val models = modelManager.scanModels().filter { it.isValid }

        if (models.isEmpty()) {
            // 2. 下载模型
            downloadModel()
        } else {
            // 3. 加载模型
            loadModel(models[0].path)
        }
    }

    private fun downloadModel() {
        lifecycleScope.launch {
            downloader.downloadModel(
                modelScopeId = "Qwen/Qwen2.5-1.5B-Instruct-GGUF",
                fileName = "qwen2.5-1.5b-instruct-q4_k_m.gguf",
                downloadDir = modelsDir,
                listener = object : ModelDownloader.DownloadProgressListener {
                    override fun onProgress(percentage: Int, downloadedBytes: Long, totalBytes: Long) {
                        runOnUiThread {
                            statusText.text = "Downloading: $percentage%"
                        }
                    }
                    override fun onSuccess(filePath: String) {
                        runOnUiThread {
                            loadModel(filePath)
                        }
                    }
                    override fun onError(message: String) {
                        runOnUiThread {
                            Toast.makeText(this@MainActivity, "Download failed", Toast.LENGTH_SHORT).show()
                        }
                    }
                }
            )
        }
    }

    private fun loadModel(modelPath: String) {
        lifecycleScope.launch {
            val result = engine.loadModel(modelPath)
            if (result.isSuccess) {
                statusText.text = "Model ready"
                addMessage(Message("你好，有什么可以帮助你的吗？", isUser = false))
            }
        }
    }

    private fun sendMessage(text: String) {
        addMessage(Message(text, isUser = true))

        lifecycleScope.launch {
            val assistantMessage = Message("", isUser = false)
            addMessage(assistantMessage)
            val assistantIndex = messages.lastIndex

            engine.generate(
                userInput = text,
                onTokenGenerated = { token ->
                    runOnUiThread {
                        messages[assistantIndex] = messages[assistantIndex].copy(
                            content = messages[assistantIndex].content + token
                        )
                        updateUI()
                    }
                }
            )
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        engine.release()
    }
}
```

---

## 🔧 技术细节

### Native库加载
AAR内部自动加载：
```kotlin
System.loadLibrary("llama-android")
```
包含以下架构的.so文件：
- `arm64-v8a/libllama-android.so`
- `armeabi-v7a/libllama-android.so`

### 线程模型
- `loadModel()`: IO线程（suspend函数）
- `generate()`: IO线程（suspend函数）
- `onTokenGenerated`: Main线程回调（内部使用Handler处理）

### ChatML格式
使用标准ChatML格式：
```
<|im_start|>system
{systemPrompt}
<|im_end|>
<|im_start|>user
{userMessage}
<|im_end|>
<|im_start|>assistant
{aiResponse}
<|im_end|>
```

---

## 📋 系统要求

- **最低SDK**: 26 (Android 8.0)
- **目标SDK**: 34 (Android 14)
- **支持架构**: arm64-v8a, armeabi-v7a
- **依赖**: Kotlin Coroutines

---

## ✅ 功能完整性检查清单

- ✅ 模型加载（本地文件）
- ✅ 模型下载（ModelScope）
- ✅ 模型扫描和管理
- ✅ 流式生成（token by token）
- ✅ 非流式生成（完整响应）
- ✅ 停止生成
- ✅ 对话历史管理
- ✅ 配置参数调整（temperature, topP, topK, maxTokens等）
- ✅ ChatML格式支持
- ✅ 多轮对话支持
- ✅ 资源释放管理

---

**文档版本**: 1.0
**最后更新**: 2025-12-18
