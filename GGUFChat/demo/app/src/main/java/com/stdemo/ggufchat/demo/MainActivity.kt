package com.stdemo.ggufchat.demo

import android.os.Build
import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.stdemo.ggufchat.GGUFChatEngine  // 来自AAR包
import com.stdemo.ggufchat.IntentConfig  // 来自AAR包
import com.stdemo.ggufchat.IntentDiagnostic  // 来自AAR包
import com.stdemo.ggufchat.IntentRecognizerManager  // 来自AAR包
import com.stdemo.ggufchat.Message  // 来自AAR包
import com.stdemo.ggufchat.ModelConfig  // 来自AAR包
import com.stdemo.ggufchat.ModelDownloader  // 来自AAR包
import com.stdemo.ggufchat.ModelManager  // 来自AAR包
import com.stdemo.ggufchat.ModelRegistry  // 来自AAR包
import com.stdemo.ggufchat.PersistentStorageHelper  // 来自AAR包
import com.stdemo.ggufchat.demo.databinding.ActivityMainBinding
import kotlinx.coroutines.launch

/**
 * Demo MainActivity - 展示如何使用AAR包中的所有功能
 *
 * 所有核心功能都来自llama-android.aar：
 * - GGUFChatEngine: 聊天引擎（流式/非流式输出）
 * - Message: 消息数据类
 * - ModelDownloader: 模型下载器（ModelScope）
 * - ModelManager: 模型扫描器
 * - ModelConfig: 模型配置持久化（SharedPreferences）
 * - ModelRegistry: 预设模型列表
 * - IntentRecognizerManager: 意图识别管理器
 * - IntentConfig: 意图识别配置
 * - IntentDiagnostic: 意图识别诊断工具
 * - PersistentStorageHelper: 持久化存储助手
 */
class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "DemoMainActivity"
    }

    private lateinit var binding: ActivityMainBinding
    private val engine = GGUFChatEngine()  // 来自AAR包
    private val messageAdapter = MessageAdapter()
    private val downloader = ModelDownloader()  // 来自AAR包
    private lateinit var modelManager: ModelManager
    private lateinit var modelConfig: ModelConfig
    private val messages = mutableListOf<Message>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val modelsDir = getExternalFilesDir("models")?.absolutePath ?: return
        modelManager = ModelManager(modelsDir)
        modelConfig = ModelConfig(this)

        setupRecyclerView()
        setupClickListeners()
        updateStreamingStatus()
        requestStoragePermission()
        initIntentRecognition()
    }

    private fun setupRecyclerView() {
        binding.recyclerView.apply {
            layoutManager = LinearLayoutManager(this@MainActivity)
            adapter = messageAdapter
        }
    }

    /**
     * 初始化意图识别（可选功能）
     */
    private fun initIntentRecognition() {
        val initialized = IntentRecognizerManager.initialize(this)
        if (initialized) {
            Log.i(TAG, "Intent recognition initialized successfully")
            addMessage(Message("[Intent recognition: READY]", isUser = false))
        } else {
            Log.i(TAG, "Intent recognition not available (ONNX Runtime or models not present)")
        }
    }

    private fun setupClickListeners() {
        binding.sendButton.setOnClickListener {
            val text = binding.inputEditText.text.toString()
            if (text.isNotBlank()) {
                sendMessage(text)
                binding.inputEditText.text?.clear()
            }
        }

        binding.clearButton.setOnClickListener {
            engine.clearHistory()
            messages.clear()
            updateMessages()
            addMessage(Message("Chat cleared. How can I help you?", isUser = false))
        }

        binding.downloadButton.setOnClickListener {
            showModelInputDialog()
        }

        binding.streamingToggleButton.setOnClickListener {
            engine.setStreamingMode(!engine.isStreamingModeEnabled())
            modelConfig.saveStreamingMode(engine.isStreamingModeEnabled())
            updateStreamingStatus()
        }

        binding.stopButton.setOnClickListener {
            engine.stopGeneration()
            Toast.makeText(this, "Generation stopped", Toast.LENGTH_SHORT).show()
        }

        binding.settingsButton?.setOnClickListener {
            showSettingsDialog()
        }

        // Feature test buttons
        binding.intentTestButton.setOnClickListener {
            testIntentRecognition()
        }

        binding.diagnosticButton.setOnClickListener {
            runDiagnostics()
        }

        binding.storageInfoButton.setOnClickListener {
            showStorageInfo()
        }

        binding.modelRegistryButton.setOnClickListener {
            showModelRegistry()
        }
    }

    /**
     * 发送消息 - 支持意图识别 + LLM 回退
     */
    private fun sendMessage(text: String) {
        if (!engine.isModelLoaded()) {
            Toast.makeText(this, "Please load a model first", Toast.LENGTH_SHORT).show()
            return
        }

        addMessage(Message(text, isUser = true))
        binding.sendButton.isEnabled = false
        binding.stopButton.isEnabled = true

        lifecycleScope.launch {
            // 尝试意图识别
            val intentResult = IntentRecognizerManager.predict(text)
            if (intentResult != null && intentResult.hit) {
                if (IntentConfig.shouldFallbackToLLM(intentResult.intent)) {
                    Log.i(TAG, "Intent HIT: ${intentResult.intent} - fallback to LLM")
                    generateWithLLM(text)
                } else {
                    Log.i(TAG, "Intent HIT: ${intentResult.intent} (${(intentResult.confidence * 100).toInt()}%)")
                    val response = buildString {
                        appendLine("Intent: ${intentResult.intent}")
                        appendLine("Confidence: ${(intentResult.confidence * 100).toInt()}%")
                        if (intentResult.slots.isNotEmpty()) {
                            appendLine("Slots:")
                            intentResult.slots.forEach { slot ->
                                appendLine("  ${slot.slotType}: ${slot.slotValue}")
                            }
                        }
                    }
                    addMessage(Message(response, isUser = false))
                    runOnUiThread {
                        binding.sendButton.isEnabled = true
                        binding.stopButton.isEnabled = false
                    }
                }
            } else {
                generateWithLLM(text)
            }
        }
    }

    /**
     * 使用 LLM 生成回复（流式/非流式）
     */
    private suspend fun generateWithLLM(text: String) {
        val assistantMessage = Message("", isUser = false)
        addMessage(assistantMessage)
        val assistantIndex = messages.lastIndex

        val result = engine.generate(
            userInput = text,
            onTokenGenerated = { token ->
                runOnUiThread {
                    messages[assistantIndex] = messages[assistantIndex].copy(
                        content = messages[assistantIndex].content + token
                    )
                    updateMessages()
                }
            }
        )

        runOnUiThread {
            if (result.isSuccess) {
                val response = result.getOrNull() ?: ""
                if (messages[assistantIndex].content.isEmpty()) {
                    messages[assistantIndex] = Message(response, isUser = false)
                    updateMessages()
                }
            } else {
                messages[assistantIndex] = Message(
                    "Error: ${result.exceptionOrNull()?.message}",
                    isUser = false
                )
                updateMessages()
            }
            binding.sendButton.isEnabled = true
            binding.stopButton.isEnabled = false
        }
    }

    /**
     * 测试意图识别功能
     */
    private fun testIntentRecognition() {
        val info = buildString {
            appendLine("=== Intent Recognition Test ===")
            appendLine("Initialized: ${IntentRecognizerManager.isReady()}")
            appendLine("Threshold: ${IntentRecognizerManager.getThreshold()}")
            appendLine("Default threshold: ${IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD}")
            appendLine("Fallback intents: ${IntentConfig.FALLBACK_TO_LLM_INTENTS}")
            appendLine()

            if (IntentRecognizerManager.isReady()) {
                val testTexts = listOf(
                    "今天北京天气怎么样",
                    "播放周杰伦的歌",
                    "你好啊"
                )
                testTexts.forEach { text ->
                    val result = IntentRecognizerManager.predict(text)
                    if (result != null) {
                        appendLine("\"$text\"")
                        appendLine("  -> ${result.intent} (${(result.confidence * 100).toInt()}%) hit=${result.hit}")
                    } else {
                        appendLine("\"$text\" -> null")
                    }
                }
            } else {
                appendLine("Intent recognition not available.")
                appendLine("(ONNX Runtime or model files not present)")
            }
        }
        addMessage(Message(info, isUser = false))

        // Also run IntentDiagnostic (output goes to logcat)
        IntentDiagnostic.checkStatus(this)
    }

    /**
     * 运行诊断
     */
    private fun runDiagnostics() {
        val info = buildString {
            appendLine("=== AAR Feature Diagnostics ===")
            appendLine()

            // Engine status
            appendLine("[GGUFChatEngine]")
            appendLine("  Model loaded: ${engine.isModelLoaded()}")
            appendLine("  Streaming: ${engine.isStreamingModeEnabled()}")
            appendLine("  History size: ${engine.getHistorySize()}")
            if (engine.isModelLoaded()) {
                appendLine("  Model info: ${engine.getModelInfo()}")
            }
            val config = engine.getConfig()
            appendLine("  Temperature: ${config.temperature}")
            appendLine("  TopP: ${config.topP}")
            appendLine("  TopK: ${config.topK}")
            appendLine("  Max tokens: ${config.maxTokens}")
            appendLine()

            // ModelConfig (SharedPreferences)
            appendLine("[ModelConfig]")
            appendLine("  Saved model path: ${modelConfig.getModelPath() ?: "none"}")
            appendLine("  Saved streaming mode: ${modelConfig.isStreamingModeEnabled()}")
            appendLine("  Saved model dir: ${modelConfig.getModelDirectory() ?: "none"}")
            appendLine()

            // ModelManager
            appendLine("[ModelManager]")
            val models = modelManager.scanModels()
            appendLine("  Found ${models.size} model(s):")
            models.forEach { model ->
                appendLine("    - ${model.name} (${model.sizeMB}MB) valid=${model.isValid}")
            }
            appendLine()

            // Intent recognition
            appendLine("[IntentRecognition]")
            appendLine("  Ready: ${IntentRecognizerManager.isReady()}")
            appendLine("  Threshold: ${IntentRecognizerManager.getThreshold()}")
            appendLine()

            // Storage
            appendLine("[Storage]")
            appendLine("  Public available: ${PersistentStorageHelper.isPublicStorageAvailable()}")
            val recommended = PersistentStorageHelper.getRecommendedModelsDir(this@MainActivity)
            appendLine("  Recommended dir: ${recommended.absolutePath}")
        }
        addMessage(Message(info, isUser = false))

        // Also run IntentDiagnostic to logcat
        IntentDiagnostic.checkStatus(this)
    }

    /**
     * 显示存储信息
     */
    private fun showStorageInfo() {
        val info = PersistentStorageHelper.getStorageInfo(this)
        addMessage(Message(info, isUser = false))
    }

    /**
     * 显示预设模型列表
     */
    private fun showModelRegistry() {
        val info = buildString {
            appendLine("=== Model Registry ===")
            ModelRegistry.availableModels.forEach { model ->
                appendLine()
                appendLine("Name: ${model.name}")
                appendLine("  ID: ${model.modelId}")
                appendLine("  File: ${model.fileName}")
                appendLine("  Desc: ${model.description}")
                appendLine("  Size: ~${model.size / (1024 * 1024)}MB")
            }
        }

        // Show as dialog with download option
        val items = ModelRegistry.availableModels.map { "${it.name} (~${it.size / (1024 * 1024)}MB)" }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Model Registry (from AAR)")
            .setItems(items) { _, which ->
                val model = ModelRegistry.availableModels[which]
                downloadModel(model.modelId, model.fileName)
            }
            .setNegativeButton("Cancel", null)
            .show()

        addMessage(Message(info, isUser = false))
    }

    private fun addMessage(message: Message) {
        messages.add(message)
        updateMessages()
    }

    private fun updateMessages() {
        messageAdapter.submitList(messages.toList()) {
            if (messages.isNotEmpty()) {
                binding.recyclerView.scrollToPosition(messages.size - 1)
            }
        }
    }

    private fun updateStreamingStatus() {
        val isStreaming = engine.isStreamingModeEnabled()
        binding.streamingToggleButton.text = if (isStreaming) "Streaming: ON" else "Streaming: OFF"
    }

    private fun requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
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

    private fun tryLoadModel() {
        // Try loading saved model path from ModelConfig first
        val savedPath = modelConfig.getModelPath()
        if (savedPath != null && java.io.File(savedPath).exists()) {
            Log.i(TAG, "Loading saved model path from ModelConfig: $savedPath")
            loadModel(savedPath)
            return
        }

        binding.statusText.text = "Scanning for models..."
        val availableModels = modelManager.scanModels().filter { it.isValid }

        when {
            availableModels.isEmpty() -> {
                binding.statusText.text = "No models found. Click 'Download' to download a model."
                showNoModelDialog()
            }
            availableModels.size == 1 -> {
                loadModel(availableModels[0].path)
            }
            else -> {
                showModelSelectionDialog(availableModels)
            }
        }
    }

    private fun loadModel(modelPath: String) {
        binding.statusText.text = "Loading model..."
        lifecycleScope.launch {
            val result = engine.loadModel(modelPath)
            runOnUiThread {
                if (result.isSuccess) {
                    binding.statusText.text = "Model ready"
                    modelConfig.saveModelPath(modelPath)
                    addMessage(Message("你好，请问有什么可以帮助你的吗？", isUser = false))
                    updateStreamingStatus()
                } else {
                    binding.statusText.text = "Failed to load model: ${result.exceptionOrNull()?.message}"
                }
            }
        }
    }

    private fun showNoModelDialog() {
        AlertDialog.Builder(this)
            .setTitle("No Models Found")
            .setMessage("Click 'Download Model' to download one from ModelScope, or choose from the Model Registry.")
            .setPositiveButton("Download Model") { _, _ -> showModelInputDialog() }
            .setNeutralButton("Model Registry") { _, _ -> showModelRegistry() }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showModelSelectionDialog(models: List<ModelManager.ModelInfo>) {
        val items = models.map { "${it.name} (${it.sizeMB}MB)" }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Select model (${models.size} found)")
            .setItems(items) { _, which -> loadModel(models[which].path) }
            .setNeutralButton("Download New Model") { _, _ -> showModelInputDialog() }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showModelInputDialog() {
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(50, 30, 50, 30)
        }

        layout.addView(TextView(this).apply {
            text = "ModelScope ID:"
            textSize = 14f
        })
        val modelScopeIdInput = EditText(this).apply {
            hint = "e.g., Qwen/Qwen2.5-1.5B-Instruct-GGUF"
            setText("Qwen/Qwen2.5-1.5B-Instruct-GGUF")
        }
        layout.addView(modelScopeIdInput)

        layout.addView(TextView(this).apply {
            text = "File Name:"
            textSize = 14f
            setPadding(0, 20, 0, 0)
        })
        val fileNameInput = EditText(this).apply {
            hint = "e.g., qwen2.5-1.5b-instruct-q4_k_m.gguf"
            setText("qwen2.5-1.5b-instruct-q4_k_m.gguf")
        }
        layout.addView(fileNameInput)

        AlertDialog.Builder(this)
            .setTitle("Download Model from ModelScope")
            .setView(layout)
            .setPositiveButton("Download") { _, _ ->
                val modelScopeId = modelScopeIdInput.text.toString().trim()
                val fileName = fileNameInput.text.toString().trim()
                if (modelScopeId.isNotEmpty() && fileName.isNotEmpty()) {
                    downloadModel(modelScopeId, fileName)
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun downloadModel(modelScopeId: String, fileName: String) {
        val modelDir = getExternalFilesDir("models")?.absolutePath ?: return

        binding.downloadButton.isEnabled = false
        binding.statusText.text = "Downloading: $fileName..."

        lifecycleScope.launch {
            downloader.downloadModel(
                modelScopeId = modelScopeId,
                fileName = fileName,
                downloadDir = modelDir,
                listener = object : ModelDownloader.DownloadProgressListener {
                    override fun onProgress(percentage: Int, downloadedBytes: Long, totalBytes: Long) {
                        runOnUiThread {
                            val downloadedMB = downloadedBytes / (1024L * 1024)
                            val totalMB = totalBytes / (1024L * 1024)
                            binding.statusText.text = "Downloading: $percentage% ($downloadedMB MB / $totalMB MB)"
                        }
                    }

                    override fun onSuccess(filePath: String) {
                        runOnUiThread {
                            binding.downloadButton.isEnabled = true
                            loadModel(filePath)
                            Toast.makeText(this@MainActivity, "Model downloaded!", Toast.LENGTH_SHORT).show()
                        }
                    }

                    override fun onError(message: String) {
                        runOnUiThread {
                            binding.statusText.text = "Download failed: $message"
                            binding.downloadButton.isEnabled = true
                            Toast.makeText(this@MainActivity, "Download failed", Toast.LENGTH_LONG).show()
                        }
                    }
                }
            )
        }
    }

    private fun showSettingsDialog() {
        val config = engine.getConfig()
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(50, 30, 50, 30)
        }

        layout.addView(TextView(this).apply {
            text = "Temperature: ${String.format("%.1f", config.temperature)}"
            textSize = 14f
        })
        val tempInput = EditText(this).apply {
            setText(config.temperature.toString())
            inputType = android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL
        }
        layout.addView(tempInput)

        layout.addView(TextView(this).apply {
            text = "Max Tokens: ${config.maxTokens}"
            textSize = 14f
            setPadding(0, 20, 0, 0)
        })
        val tokensInput = EditText(this).apply {
            setText(config.maxTokens.toString())
            inputType = android.text.InputType.TYPE_CLASS_NUMBER
        }
        layout.addView(tokensInput)

        layout.addView(TextView(this).apply {
            text = "Intent Threshold: ${IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD}"
            textSize = 14f
            setPadding(0, 20, 0, 0)
        })
        val thresholdInput = EditText(this).apply {
            setText(IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD.toString())
            inputType = android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL
        }
        layout.addView(thresholdInput)

        AlertDialog.Builder(this)
            .setTitle("Settings")
            .setView(layout)
            .setPositiveButton("Save") { _, _ ->
                tempInput.text.toString().toFloatOrNull()?.let { engine.setTemperature(it) }
                tokensInput.text.toString().toIntOrNull()?.let { engine.setMaxTokens(it) }
                thresholdInput.text.toString().toFloatOrNull()?.let { threshold ->
                    if (threshold in 0f..1f) {
                        IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD = threshold
                        IntentRecognizerManager.setThreshold(threshold)
                    }
                }
                Toast.makeText(this, "Settings saved", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    override fun onDestroy() {
        super.onDestroy()
        engine.release()
        IntentRecognizerManager.release()
    }
}
