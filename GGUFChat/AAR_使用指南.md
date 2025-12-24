# GGUF Chat AAR 完整使用指南

**版本**: 1.0
**AAR包**: `llama-android-debug.aar`
**包名**: `com.stdemo.ggufchat`
**更新日期**: 2025-12-18

---

## 📑 目录

1. [快速开始](#快速开始)
2. [集成AAR到项目](#集成aar到项目)
3. [核心类使用指南](#核心类使用指南)
4. [完整示例](#完整示例)
5. [常见场景](#常见场景)
6. [错误处理](#错误处理)
7. [性能优化](#性能优化)
8. [常见问题](#常见问题)

---

## 快速开始

### 30秒快速体验

```kotlin
import com.stdemo.ggufchat.*

class MainActivity : AppCompatActivity() {
    private val engine = GGUFChatEngine()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        lifecycleScope.launch {
            // 1. 加载模型
            engine.loadModel("/path/to/model.gguf")

            // 2. 生成回复
            engine.generate("你好") { token ->
                print(token)  // 流式输出每个token
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        engine.release()  // 释放资源
    }
}
```

---

## 集成AAR到项目

### 步骤1: 添加AAR文件

将 `llama-android-debug.aar` 复制到你的项目：

```
your-project/
└── app/
    └── libs/
        └── llama-android-debug.aar  ← 放这里
```

### 步骤2: 配置 build.gradle.kts

```kotlin
// app/build.gradle.kts

android {
    compileSdk = 34

    defaultConfig {
        minSdk = 26  // 最低要求 Android 8.0
        targetSdk = 34

        ndk {
            // 指定支持的架构
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }

    buildFeatures {
        viewBinding = true  // 推荐使用ViewBinding
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true  // 必需，用于.so文件
        }
    }
}

dependencies {
    // 添加AAR依赖
    implementation(fileTree(mapOf("dir" to "libs", "include" to listOf("*.aar"))))

    // 必需的依赖
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")
}
```

### 步骤3: 配置权限

在 `AndroidManifest.xml` 中添加：

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <!-- 必需的权限 -->
    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />

    <!-- Android 13+ 需要 -->
    <uses-permission android:name="android.permission.READ_MEDIA_IMAGES" />
    <uses-permission android:name="android.permission.READ_MEDIA_VIDEO" />
    <uses-permission android:name="android.permission.READ_MEDIA_AUDIO" />

    <application>
        ...
    </application>
</manifest>
```

### 步骤4: 请求运行时权限

**重要**：正确处理Android不同版本的权限，特别是Android 13+

```kotlin
class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // 请求存储权限
        requestStoragePermission()
    }

    private fun requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            // Android 13+ (API 33+)
            if (checkSelfPermission(Manifest.permission.READ_MEDIA_IMAGES) == PackageManager.PERMISSION_GRANTED) {
                // 权限已授予，可以使用AAR功能
                onPermissionGranted()
            } else {
                // 请求Android 13+的新权限
                requestPermissions(arrayOf(
                    Manifest.permission.READ_MEDIA_IMAGES,
                    Manifest.permission.READ_MEDIA_VIDEO,
                    Manifest.permission.READ_MEDIA_AUDIO
                ), 100)
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Android 6-12
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(arrayOf(
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE
                ), 100)
            } else {
                onPermissionGranted()
            }
        } else {
            // Android 5及以下，不需要运行时权限
            onPermissionGranted()
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 100 && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            onPermissionGranted()
        } else {
            Toast.makeText(this, "需要存储权限才能使用模型功能", Toast.LENGTH_LONG).show()
        }
    }

    private fun onPermissionGranted() {
        // 权限已授予，现在可以：
        // 1. 扫描本地模型
        // 2. 下载模型
        // 3. 加载模型
    }
}
```

### 步骤5: 导入类

```kotlin
import com.stdemo.ggufchat.GGUFChatEngine
import com.stdemo.ggufchat.ChatConfig
import com.stdemo.ggufchat.Message
import com.stdemo.ggufchat.ModelDownloader
import com.stdemo.ggufchat.ModelManager
```

✅ 完成！现在可以开始使用AAR了。

---

## 核心类使用指南

### 1️⃣ GGUFChatEngine - 聊天引擎

#### 创建引擎实例

```kotlin
class MainActivity : AppCompatActivity() {
    // 创建引擎实例（只需创建一次）
    private val engine = GGUFChatEngine()
}
```

#### 加载模型

```kotlin
lifecycleScope.launch {
    val modelPath = "/storage/emulated/0/models/model.gguf"

    val result = engine.loadModel(modelPath)

    if (result.isSuccess) {
        Toast.makeText(this@MainActivity, "模型加载成功", Toast.LENGTH_SHORT).show()
    } else {
        val error = result.exceptionOrNull()?.message
        Toast.makeText(this@MainActivity, "加载失败: $error", Toast.LENGTH_LONG).show()
    }
}
```

**重要提示**:
- ✅ `loadModel()` 是suspend函数，必须在协程中调用
- ✅ 加载前会自动清空历史记录
- ✅ 加载成功后才能调用 `generate()`

#### 生成回复（流式模式）

```kotlin
// 默认就是流式模式
lifecycleScope.launch {
    val result = engine.generate(
        userInput = "你好，请介绍一下你自己",
        onTokenGenerated = { token ->
            // 每生成一个token就会调用
            runOnUiThread {
                textView.append(token)
            }
        }
    )

    if (result.isSuccess) {
        val fullText = result.getOrNull()
        println("完整回复: $fullText")
    }
}
```

#### 生成回复（非流式模式）

```kotlin
// 切换为非流式模式
engine.setStreamingMode(false)

lifecycleScope.launch {
    val result = engine.generate(userInput = "你好")

    if (result.isSuccess) {
        val response = result.getOrNull() ?: ""
        textView.text = response  // 一次性显示完整回复
    }
}
```

#### 配置参数

```kotlin
// 方式1: 单独设置
engine.setSystemPrompt("你是一个专业的翻译助手")
engine.setTemperature(0.7f)        // 随机性: 0.0-2.0
engine.setTopP(0.9f)               // 核采样: 0.0-1.0
engine.setTopK(40)                 // Top-K采样
engine.setMaxTokens(512)           // 最大生成token数
engine.setMaxHistoryPairs(10)      // 保留10轮对话历史

// 方式2: 批量设置
val config = ChatConfig(
    temperature = 0.8f,
    topP = 0.95f,
    topK = 50,
    maxTokens = 1024,
    maxHistoryPairs = 15,
    systemPrompt = "你是一个有帮助的AI助手"
)
engine.setConfig(config)
```

#### 停止生成

```kotlin
// 在按钮点击事件中
stopButton.setOnClickListener {
    engine.stopGeneration()  // 立即停止
}

// 检查是否正在生成
if (engine.isGenerating()) {
    println("正在生成中...")
}
```

#### 历史管理

```kotlin
// 清空历史记录
engine.clearHistory()

// 获取当前历史轮数
val rounds = engine.getHistorySize()
println("已保存 $rounds 轮对话")
```

#### 释放资源

```kotlin
override fun onDestroy() {
    super.onDestroy()
    engine.release()  // 必须调用！释放模型和内存
}
```

---

### 2️⃣ ModelManager - 模型管理器

#### 创建管理器

```kotlin
val modelsDir = getExternalFilesDir("models")?.absolutePath ?: ""
val modelManager = ModelManager(modelsDir)
```

#### 扫描本地模型

```kotlin
// 扫描所有GGUF模型
val models = modelManager.scanModels()

models.forEach { model ->
    println("模型: ${model.name}")
    println("大小: ${model.sizeMB} MB")
    println("路径: ${model.path}")
    println("有效: ${model.isValid}")  // >= 50MB才算有效
    println("---")
}
```

#### 自动加载第一个有效模型

```kotlin
val firstModel = modelManager.getFirstValidModel()

if (firstModel != null) {
    lifecycleScope.launch {
        engine.loadModel(firstModel.path)
        Toast.makeText(this@MainActivity,
            "已加载: ${firstModel.name} (${firstModel.sizeMB}MB)",
            Toast.LENGTH_SHORT).show()
    }
} else {
    Toast.makeText(this@MainActivity,
        "未找到模型，请下载",
        Toast.LENGTH_LONG).show()
}
```

#### 显示模型列表供用户选择

```kotlin
val models = modelManager.scanModels().filter { it.isValid }

if (models.isEmpty()) {
    Toast.makeText(this, "未找到模型", Toast.LENGTH_SHORT).show()
    return
}

// 创建模型名称列表
val modelNames = models.map { "${it.name} (${it.sizeMB}MB)" }.toTypedArray()

AlertDialog.Builder(this)
    .setTitle("选择模型")
    .setItems(modelNames) { _, which ->
        val selectedModel = models[which]
        lifecycleScope.launch {
            engine.loadModel(selectedModel.path)
        }
    }
    .show()
```

#### 删除模型

```kotlin
val deleted = modelManager.deleteModel("/path/to/model.gguf")

if (deleted) {
    println("删除成功")
} else {
    println("删除失败")
}
```

#### 获取统计信息

```kotlin
val totalSize = modelManager.getTotalSize()
val count = modelManager.getModelCount()
val validCount = modelManager.getValidModelCount()

println("共有 $count 个模型，其中 $validCount 个有效")
println("总大小: ${modelManager.formatSize(totalSize)}")

// 获取详细描述
val description = modelManager.getModelsDescription()
println(description)
```

---

### 3️⃣ ModelDownloader - 模型下载器

#### 创建下载器

```kotlin
val downloader = ModelDownloader()
```

#### 下载模型（带进度）

```kotlin
val modelsDir = getExternalFilesDir("models")?.absolutePath ?: return

lifecycleScope.launch {
    val result = downloader.downloadModel(
        modelScopeId = "Qwen/Qwen2.5-1.5B-Instruct-GGUF",
        fileName = "qwen2.5-1.5b-instruct-q4_k_m.gguf",
        downloadDir = modelsDir,
        listener = object : ModelDownloader.DownloadProgressListener {

            override fun onProgress(percentage: Int, downloadedBytes: Long, totalBytes: Long) {
                // 更新进度条
                runOnUiThread {
                    progressBar.progress = percentage
                    val downloadedMB = downloadedBytes / (1024 * 1024)
                    val totalMB = totalBytes / (1024 * 1024)
                    statusText.text = "下载中: $percentage% ($downloadedMB MB / $totalMB MB)"
                }
            }

            override fun onSuccess(filePath: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "下载完成", Toast.LENGTH_SHORT).show()
                    // 自动加载模型
                    lifecycleScope.launch {
                        engine.loadModel(filePath)
                    }
                }
            }

            override fun onError(message: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "下载失败: $message", Toast.LENGTH_LONG).show()
                }
            }
        }
    )
}
```

#### 用户自定义下载

```kotlin
// 显示对话框让用户输入
fun showDownloadDialog() {
    val dialogView = layoutInflater.inflate(R.layout.dialog_download, null)
    val idInput = dialogView.findViewById<EditText>(R.id.modelScopeIdInput)
    val fileInput = dialogView.findViewById<EditText>(R.id.fileNameInput)

    AlertDialog.Builder(this)
        .setTitle("下载模型")
        .setView(dialogView)
        .setPositiveButton("下载") { _, _ ->
            val modelScopeId = idInput.text.toString()
            val fileName = fileInput.text.toString()

            if (modelScopeId.isNotBlank() && fileName.isNotBlank()) {
                downloadModel(modelScopeId, fileName)
            }
        }
        .setNegativeButton("取消", null)
        .show()
}
```

#### 构建下载URL

```kotlin
val url = downloader.buildDownloadUrl(
    "Qwen/Qwen2.5-1.5B-Instruct-GGUF",
    "qwen2.5-1.5b-instruct-q4_k_m.gguf"
)

println("下载URL: $url")
// 输出: https://www.modelscope.cn/models/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/master/qwen2.5-1.5b-instruct-q4_k_m.gguf
```

---

### 4️⃣ Message - 消息数据类

#### 创建消息

```kotlin
// 用户消息
val userMsg = Message(
    content = "你好",
    isUser = true
)

// AI消息
val aiMsg = Message(
    content = "你好！有什么可以帮助你的吗？",
    isUser = false
)

// 带自定义时间戳
val msg = Message(
    content = "测试消息",
    isUser = true,
    timestamp = System.currentTimeMillis()
)
```

#### 在RecyclerView中使用

```kotlin
class MessageAdapter : ListAdapter<Message, MessageViewHolder>(MessageDiffCallback()) {

    override fun onBindViewHolder(holder: MessageViewHolder, position: Int) {
        val message = getItem(position)

        holder.contentText.text = message.content

        // 根据isUser设置不同样式
        if (message.isUser) {
            holder.bubble.setBackgroundResource(R.drawable.bg_user_message)
            holder.contentText.gravity = Gravity.END
        } else {
            holder.bubble.setBackgroundResource(R.drawable.bg_ai_message)
            holder.contentText.gravity = Gravity.START
        }

        // 格式化时间戳
        val time = SimpleDateFormat("HH:mm", Locale.getDefault())
            .format(Date(message.timestamp))
        holder.timeText.text = time
    }
}

class MessageDiffCallback : DiffUtil.ItemCallback<Message>() {
    override fun areItemsTheSame(oldItem: Message, newItem: Message): Boolean {
        return oldItem.timestamp == newItem.timestamp
    }

    override fun areContentsTheSame(oldItem: Message, newItem: Message): Boolean {
        return oldItem == newItem
    }
}
```

---

### 5️⃣ ChatConfig - 配置数据类

#### 默认配置

```kotlin
val defaultConfig = ChatConfig()
// temperature = 0.7f
// topP = 0.9f
// topK = 40
// maxTokens = 512
// maxHistoryPairs = 10
// systemPrompt = "你叫小达，是一个有帮助的ai机器人助手，请用简体中文回答问题。"
```

#### 自定义配置

```kotlin
// 翻译助手配置
val translatorConfig = ChatConfig(
    temperature = 0.3f,  // 更确定性
    maxTokens = 1024,
    systemPrompt = "你是一个专业的中英翻译助手，请准确翻译用户给出的内容。"
)

// 创意写作配置
val creativeConfig = ChatConfig(
    temperature = 1.2f,  // 更随机
    topP = 0.95f,
    maxTokens = 2048,
    systemPrompt = "你是一个富有创造力的写作助手，请帮助用户创作有趣的内容。"
)

// 应用配置
engine.setConfig(translatorConfig)
```

---

## 完整示例

### 示例1: 最简单的聊天应用

**⚠️ 注意**: 此示例为了简洁省略了权限处理。在实际使用中，请先参考[步骤4](#步骤4-请求运行时权限)处理权限，或直接参考下面的[示例2](#示例2-完整功能的聊天应用)。

```kotlin
class SimpleChatActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySimpleChatBinding
    private val engine = GGUFChatEngine()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySimpleChatBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // 加载模型
        lifecycleScope.launch {
            val result = engine.loadModel("/path/to/model.gguf")
            if (result.isSuccess) {
                binding.statusText.text = "模型已就绪"
            }
        }

        // 发送按钮
        binding.sendButton.setOnClickListener {
            val input = binding.inputText.text.toString()
            if (input.isNotBlank()) {
                sendMessage(input)
                binding.inputText.text.clear()
            }
        }
    }

    private fun sendMessage(text: String) {
        // 显示用户消息
        binding.chatText.append("用户: $text\n")

        lifecycleScope.launch {
            binding.chatText.append("AI: ")

            engine.generate(
                userInput = text,
                onTokenGenerated = { token ->
                    binding.chatText.append(token)
                }
            )

            binding.chatText.append("\n\n")
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        engine.release()
    }
}
```

---

### 示例2: 完整功能的聊天应用

```kotlin
class FullChatActivity : AppCompatActivity() {

    private lateinit var binding: ActivityFullChatBinding
    private val engine = GGUFChatEngine()
    private val modelManager by lazy {
        ModelManager(getExternalFilesDir("models")?.absolutePath ?: "")
    }
    private val downloader = ModelDownloader()
    private val messageAdapter = MessageAdapter()
    private val messages = mutableListOf<Message>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityFullChatBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupRecyclerView()
        setupClickListeners()
        requestStoragePermission()  // 先请求权限
    }

    private fun requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            // Android 13+
            if (checkSelfPermission(Manifest.permission.READ_MEDIA_IMAGES) == PackageManager.PERMISSION_GRANTED) {
                tryLoadModel()
            } else {
                requestPermissions(arrayOf(
                    Manifest.permission.READ_MEDIA_IMAGES,
                    Manifest.permission.READ_MEDIA_VIDEO,
                    Manifest.permission.READ_MEDIA_AUDIO
                ), 1)
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Android 6-12
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(arrayOf(
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE
                ), 1)
            } else {
                tryLoadModel()
            }
        } else {
            tryLoadModel()
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 1 && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            tryLoadModel()
        }
    }

    private fun setupRecyclerView() {
        binding.recyclerView.apply {
            adapter = messageAdapter
            layoutManager = LinearLayoutManager(this@FullChatActivity)
        }
    }

    private fun setupClickListeners() {
        // 发送消息
        binding.sendButton.setOnClickListener {
            val text = binding.inputText.text.toString()
            if (text.isNotBlank() && engine.isModelLoaded()) {
                sendMessage(text)
                binding.inputText.text.clear()
            }
        }

        // 停止生成
        binding.stopButton.setOnClickListener {
            engine.stopGeneration()
        }

        // 清空历史
        binding.clearButton.setOnClickListener {
            engine.clearHistory()
            messages.clear()
            updateMessages()
        }

        // 下载模型
        binding.downloadButton.setOnClickListener {
            showDownloadDialog()
        }

        // 选择模型
        binding.selectModelButton.setOnClickListener {
            showModelSelector()
        }
    }

    private fun tryLoadModel() {
        val firstModel = modelManager.getFirstValidModel()

        if (firstModel != null) {
            loadModel(firstModel.path)
        } else {
            binding.statusText.text = "未找到模型，请下载"
        }
    }

    private fun loadModel(path: String) {
        binding.statusText.text = "加载模型中..."

        lifecycleScope.launch {
            val result = engine.loadModel(path)

            if (result.isSuccess) {
                binding.statusText.text = "模型已就绪"
                addMessage(Message("你好，有什么可以帮助你的吗？", isUser = false))
            } else {
                binding.statusText.text = "加载失败: ${result.exceptionOrNull()?.message}"
            }
        }
    }

    private fun sendMessage(text: String) {
        // 添加用户消息
        addMessage(Message(text, isUser = true))

        // 创建空的AI消息
        val aiMessage = Message("", isUser = false)
        addMessage(aiMessage)
        val aiIndex = messages.lastIndex

        binding.statusText.text = "生成中..."

        lifecycleScope.launch {
            engine.generate(
                userInput = text,
                onTokenGenerated = { token ->
                    runOnUiThread {
                        messages[aiIndex] = messages[aiIndex].copy(
                            content = messages[aiIndex].content + token
                        )
                        updateMessages()
                    }
                }
            )

            binding.statusText.text = "就绪"
        }
    }

    private fun showModelSelector() {
        val models = modelManager.scanModels().filter { it.isValid }

        if (models.isEmpty()) {
            Toast.makeText(this, "未找到模型", Toast.LENGTH_SHORT).show()
            return
        }

        val modelNames = models.map { "${it.name} (${it.sizeMB}MB)" }.toTypedArray()

        AlertDialog.Builder(this)
            .setTitle("选择模型")
            .setItems(modelNames) { _, which ->
                loadModel(models[which].path)
            }
            .show()
    }

    private fun showDownloadDialog() {
        val modelsDir = getExternalFilesDir("models")?.absolutePath ?: return

        binding.statusText.text = "下载中..."

        lifecycleScope.launch {
            downloader.downloadModel(
                modelScopeId = "Qwen/Qwen2.5-1.5B-Instruct-GGUF",
                fileName = "qwen2.5-1.5b-instruct-q4_k_m.gguf",
                downloadDir = modelsDir,
                listener = object : ModelDownloader.DownloadProgressListener {
                    override fun onProgress(percentage: Int, downloadedBytes: Long, totalBytes: Long) {
                        runOnUiThread {
                            binding.statusText.text = "下载: $percentage%"
                        }
                    }

                    override fun onSuccess(filePath: String) {
                        runOnUiThread {
                            Toast.makeText(this@FullChatActivity, "下载完成", Toast.LENGTH_SHORT).show()
                            loadModel(filePath)
                        }
                    }

                    override fun onError(message: String) {
                        runOnUiThread {
                            binding.statusText.text = "下载失败"
                            Toast.makeText(this@FullChatActivity, "错误: $message", Toast.LENGTH_LONG).show()
                        }
                    }
                }
            )
        }
    }

    private fun addMessage(message: Message) {
        messages.add(message)
        updateMessages()
    }

    private fun updateMessages() {
        messageAdapter.submitList(messages.toList())
        binding.recyclerView.scrollToPosition(messages.lastIndex)
    }

    override fun onDestroy() {
        super.onDestroy()
        engine.release()
    }
}
```

---

## 常见场景

### 场景1: 首次启动检查模型

**注意**：这个逻辑应该在权限授予后调用（参考完整示例中的 `requestStoragePermission()`）

```kotlin
// 在权限授予后调用此方法
private fun checkAndLoadModel() {
    val modelsDir = getExternalFilesDir("models")?.absolutePath ?: return
    val modelManager = ModelManager(modelsDir)

    // 检查是否有可用模型
    val models = modelManager.scanModels().filter { it.isValid }

    when {
        models.isEmpty() -> {
            // 没有模型，引导用户下载
            showDownloadGuide()
        }
        models.size == 1 -> {
            // 只有一个模型，直接加载
            lifecycleScope.launch {
                engine.loadModel(models[0].path)
            }
        }
        else -> {
            // 多个模型，让用户选择
            showModelSelector(models)
        }
    }
}
```

### 场景2: 切换模型

```kotlin
fun switchModel(newModelPath: String) {
    lifecycleScope.launch {
        // 1. 先释放当前模型
        if (engine.isModelLoaded()) {
            engine.release()
        }

        // 2. 重新创建引擎（或者等待release完成）
        delay(100)

        // 3. 加载新模型
        val result = engine.loadModel(newModelPath)

        if (result.isSuccess) {
            Toast.makeText(this@MainActivity, "模型切换成功", Toast.LENGTH_SHORT).show()
        }
    }
}
```

### 场景3: 保存和恢复配置

```kotlin
// 保存配置
fun saveConfig() {
    val config = engine.getConfig()
    val prefs = getSharedPreferences("chat_config", MODE_PRIVATE)

    prefs.edit {
        putFloat("temperature", config.temperature)
        putFloat("topP", config.topP)
        putInt("topK", config.topK)
        putInt("maxTokens", config.maxTokens)
        putInt("maxHistoryPairs", config.maxHistoryPairs)
        putString("systemPrompt", config.systemPrompt)
    }
}

// 恢复配置
fun loadConfig() {
    val prefs = getSharedPreferences("chat_config", MODE_PRIVATE)

    val config = ChatConfig(
        temperature = prefs.getFloat("temperature", 0.7f),
        topP = prefs.getFloat("topP", 0.9f),
        topK = prefs.getInt("topK", 40),
        maxTokens = prefs.getInt("maxTokens", 512),
        maxHistoryPairs = prefs.getInt("maxHistoryPairs", 10),
        systemPrompt = prefs.getString("systemPrompt", "默认提示词") ?: "默认提示词"
    )

    engine.setConfig(config)
}
```

### 场景4: 显示生成速度

```kotlin
private fun sendMessage(text: String) {
    val startTime = System.currentTimeMillis()
    var tokenCount = 0

    lifecycleScope.launch {
        engine.generate(
            userInput = text,
            onTokenGenerated = { token ->
                tokenCount++

                // 计算速度（tokens/秒）
                val elapsed = (System.currentTimeMillis() - startTime) / 1000.0
                val speed = if (elapsed > 0) tokenCount / elapsed else 0.0

                runOnUiThread {
                    statusText.text = "生成速度: %.2f tokens/s".format(speed)
                }
            }
        )
    }
}
```

---

## 错误处理

### 常见错误和解决方案

#### 错误1: "Model file not found"

```kotlin
val result = engine.loadModel(path)
if (result.isFailure) {
    val error = result.exceptionOrNull()?.message
    if (error?.contains("not found") == true) {
        // 文件不存在
        AlertDialog.Builder(this)
            .setTitle("模型文件不存在")
            .setMessage("请检查路径或重新下载模型")
            .setPositiveButton("下载模型") { _, _ ->
                showDownloadDialog()
            }
            .show()
    }
}
```

#### 错误2: "Model not loaded"

```kotlin
fun sendMessage(text: String) {
    if (!engine.isModelLoaded()) {
        Toast.makeText(this, "请先加载模型", Toast.LENGTH_SHORT).show()
        return
    }

    // 继续发送消息
    lifecycleScope.launch {
        engine.generate(text)
    }
}
```

#### 错误3: "Generation already in progress"

```kotlin
fun sendMessage(text: String) {
    if (engine.isGenerating()) {
        Toast.makeText(this, "请等待当前回复完成", Toast.LENGTH_SHORT).show()
        return
    }

    // 继续发送消息
}
```

#### 错误4: 下载失败

```kotlin
override fun onError(message: String) {
    when {
        message.contains("timeout") -> {
            // 网络超时
            showRetryDialog("网络超时，是否重试？")
        }
        message.contains("Unable to get file size") -> {
            // 无法获取文件大小
            showError("服务器连接失败，请检查网络")
        }
        else -> {
            showError("下载失败: $message")
        }
    }
}
```

---

## 性能优化

### 优化1: 减少生成时间

```kotlin
// 降低maxTokens
engine.setMaxTokens(256)  // 从512降到256

// 减少历史轮数
engine.setMaxHistoryPairs(5)  // 从10降到5
```

### 优化2: 内存优化

```kotlin
// 不用时释放模型
override fun onPause() {
    super.onPause()
    if (isFinishing) {
        engine.release()
    }
}

// 限制消息列表大小
private fun addMessage(message: Message) {
    messages.add(message)

    // 只保留最近100条消息
    if (messages.size > 100) {
        messages.removeAt(0)
    }

    updateMessages()
}
```

### 优化3: UI流畅性

```kotlin
// 降低UI更新频率
private var lastUpdateTime = 0L

engine.generate(
    userInput = text,
    onTokenGenerated = { token ->
        val now = System.currentTimeMillis()

        // 每100ms更新一次UI
        if (now - lastUpdateTime > 100) {
            runOnUiThread {
                updateUI(token)
            }
            lastUpdateTime = now
        }
    }
)
```

---

## 常见问题

### Q1: 应该使用什么模型？

**A**: 推荐使用GGUF格式的量化模型：

| 设备类型 | 推荐模型 | 大小 |
|---------|---------|------|
| 入门设备 | Qwen2.5-1.5B-Q4 | ~1GB |
| 中端设备 | Qwen2.5-3B-Q4 | ~2GB |
| 高端设备 | Qwen2.5-7B-Q4 | ~4GB |

### Q2: 生成速度慢怎么办？

**A**: 尝试以下方法：
1. 使用更小的模型（1.5B参数）
2. 降低 `maxTokens`
3. 减少 `maxHistoryPairs`
4. 关闭后台应用释放内存

### Q3: 如何支持多轮对话？

**A**: 引擎自动管理历史，只需连续调用 `generate()`：

```kotlin
// 第一轮
engine.generate("你好")

// 第二轮（会自动包含第一轮的上下文）
engine.generate("我刚才问了什么？")
```

### Q4: 可以同时发送多个请求吗？

**A**: 不可以。引擎同一时间只能处理一个请求。需要等待当前生成完成。

### Q5: 如何修改系统提示词？

**A**:
```kotlin
engine.setSystemPrompt("你是一个专业的编程助手")
```

### Q6: 流式模式和非流式模式有什么区别？

**A**:
- **流式**: 逐个token返回，用户体验好，实时性强
- **非流式**: 等待完整生成，简单但响应慢

### Q7: 下载的模型保存在哪里？

**A**:
```kotlin
val modelsDir = getExternalFilesDir("models")?.absolutePath
// 通常在: /storage/emulated/0/Android/data/your.package/files/models/
```

### Q8: 如何清空对话重新开始？

**A**:
```kotlin
engine.clearHistory()
```

### Q9: 模型加载失败怎么办？

**A**: 检查：
1. 文件是否存在
2. 文件大小是否正常（至少50MB）
3. 文件扩展名是否为`.gguf`
4. 存储权限是否已授予

### Q10: 可以自定义模型路径吗？

**A**: 可以，只要路径有效：
```kotlin
engine.loadModel("/your/custom/path/model.gguf")
```

### Q11: 为什么关闭app重开后模型不自动加载？

**A**: 这是Android 13+权限处理的常见问题。确保：

1. **正确处理Android 13+权限**（参考步骤4）
2. **在权限授予后调用加载逻辑**：
```kotlin
private fun requestStoragePermission() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        if (checkSelfPermission(...) == PackageManager.PERMISSION_GRANTED) {
            tryLoadModel()  // ✅ 关键：已有权限时要调用
        } else {
            requestPermissions(...)
        }
    }
    // ... 其他版本处理
}
```

3. **避免使用 `hasAttemptedModelLoad` 等标志**防止重复加载，除非重置策略正确

---

## 推荐资源

### 模型下载源

1. **ModelScope** (官方推荐)
   - Qwen系列: `Qwen/Qwen2.5-XXX-Instruct-GGUF`

2. **Hugging Face**
   - 搜索关键词: "GGUF", "Qwen", "Instruct"

### 参数调优指南

| 用途 | Temperature | TopP | MaxTokens |
|------|-------------|------|-----------|
| 翻译 | 0.3 | 0.9 | 1024 |
| 问答 | 0.7 | 0.9 | 512 |
| 创作 | 1.2 | 0.95 | 2048 |
| 代码 | 0.2 | 0.9 | 1024 |

---

## 总结

### 核心流程

1. ✅ 添加AAR到项目
2. ✅ 配置依赖和权限
3. ✅ 创建 `GGUFChatEngine` 实例
4. ✅ 使用 `ModelManager` 扫描或 `ModelDownloader` 下载模型
5. ✅ 调用 `loadModel()` 加载模型
6. ✅ 调用 `generate()` 生成回复
7. ✅ 在 `onDestroy()` 中调用 `release()`

### 最佳实践

- ✅ 始终在协程中调用suspend函数
- ✅ 使用 `Result` 检查操作是否成功
- ✅ 流式模式提供更好的用户体验
- ✅ 及时释放资源避免内存泄漏
- ✅ 根据设备性能选择合适的模型

---

**文档完成！如有问题，请参考demo项目或提issue。**
