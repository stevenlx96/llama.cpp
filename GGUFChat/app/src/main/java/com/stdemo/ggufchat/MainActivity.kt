package com.stdemo.ggufchat

import android.os.Build
import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.stdemo.ggufchat.databinding.ActivityMainBinding
import kotlinx.coroutines.launch
import java.io.File

/**
 * MainActivity - 示例应用程序
 *
 * 演示如何使用改进后的GGUFChatEngine API：
 * 1. 扫描本地模型
 * 2. 从ModelScope下载模型
 * 3. 加载和使用模型
 * 4. 调整各种参数
 * 5. 控制流式/静态输出
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val viewModel: ChatViewModel by viewModels()
    private val messageAdapter = MessageAdapter()
    private val downloader = ModelDownloader()
    private lateinit var modelManager: ModelManager
    private var hasAttemptedModelLoad = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val modelsDir = getExternalFilesDir("models")?.absolutePath ?: return
        modelManager = ModelManager(modelsDir)

        setupRecyclerView()
        setupObservers()
        setupClickListeners()
        updateStreamingModeStatus()
        requestStoragePermission()
    }

    private fun setupRecyclerView() {
        binding.recyclerView.apply {
            layoutManager = LinearLayoutManager(this@MainActivity)
            adapter = messageAdapter
        }
    }

    private fun setupObservers() {
        lifecycleScope.launch {
            viewModel.messages.collect { messages ->
                messageAdapter.submitList(messages) {
                    binding.recyclerView.scrollToPosition(messages.size - 1)
                }
            }
        }

        lifecycleScope.launch {
            viewModel.isLoading.collect { isLoading ->
                binding.sendButton.isEnabled = !isLoading
                binding.inputEditText.isEnabled = !isLoading
            }
        }

        lifecycleScope.launch {
            viewModel.modelStatus.collect { status ->
                binding.statusText.text = status
            }
        }

        // 观察生成状态，更新停止按钮
        lifecycleScope.launch {
            viewModel.isGenerating.collect { isGenerating ->
                binding.stopButton.isEnabled = isGenerating
                binding.stopButton.text = if (isGenerating) "Stop" else "Stop"
            }
        }
    }

    private fun setupClickListeners() {
        // 诊断按钮 - 长按状态栏显示存储信息
        binding.statusText.setOnLongClickListener {
            showStorageDiagnostics()
            true
        }

        // 发送按钮
        binding.sendButton.setOnClickListener {
            val text = binding.inputEditText.text.toString()
            if (text.isNotBlank()) {
                viewModel.sendMessage(text)
                binding.inputEditText.text?.clear()
            }
        }

        // 清除对话按钮
        binding.clearButton.setOnClickListener {
            viewModel.clearChat()
        }

        // 下载模型按钮
        binding.downloadButton.setOnClickListener {
            showModelInputDialog()
        }

        // 流式/静态切换按钮
        binding.streamingToggleButton.setOnClickListener {
            viewModel.toggleStreamingMode()
            updateStreamingModeStatus()
        }

        // 停止生成按钮
        binding.stopButton.setOnClickListener {
            viewModel.stopGeneration()
            Toast.makeText(this, "Generation stopped", Toast.LENGTH_SHORT).show()
        }

        // 设置按钮（示例：打开设置对话框）
        binding.settingsButton?.setOnClickListener {
            showSettingsDialog()
        }
    }

    private fun updateStreamingModeStatus() {
        val isStreaming = viewModel.isStreamingMode()
        val statusText = if (isStreaming) "Streaming: ON" else "Streaming: OFF"
        binding.streamingToggleButton.text = statusText
    }

    private fun requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            // Android 13+ (API 33+)
            // 注意：getExternalFilesDir() 不需要权限，但为了兼容性还是请求
            // 无论权限如何，都应该尝试加载模型
            if (checkSelfPermission(Manifest.permission.READ_MEDIA_IMAGES) == PackageManager.PERMISSION_GRANTED) {
                // 权限已授予，直接加载
                tryLoadModelFromStorage()
            } else {
                // 请求权限
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
                tryLoadModelFromStorage()
            }
        } else {
            tryLoadModelFromStorage()
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 1) {
            if ((grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED)) {
                tryLoadModelFromStorage()
            } else {
                Toast.makeText(this, "Storage permission required to load models", Toast.LENGTH_SHORT).show()
            }
        }
    }

    /**
     * 尝试从存储中加载模型
     */
    private fun tryLoadModelFromStorage() {
        if (hasAttemptedModelLoad) {
            return
        }
        hasAttemptedModelLoad = true

        updateStatusText("Scanning for models...")

        val availableModels = modelManager.scanModels().filter { it.isValid }

        when {
            availableModels.isEmpty() -> {
                updateStatusText("No models found. Click 'Download' to download a model.")
                showNoModelDialog()
            }
            availableModels.size == 1 -> {
                val model = availableModels[0]
                updateStatusText("Found model: ${model.name}, loading...")
                loadModelAndStart(model.path)
            }
            else -> {
                updateStatusText("Found ${availableModels.size} models. Please select one.")
                showModelSelectionDialog()
            }
        }
    }

    /**
     * 显示未找到模型的对话框
     */
    private fun showNoModelDialog() {
        AlertDialog.Builder(this)
            .setTitle("No Models Found")
            .setMessage("No GGUF model files found in the app directory.\n\nClick 'Download Model' to download one from ModelScope.")
            .setPositiveButton("Download Model") { _, _ ->
                showModelInputDialog()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    /**
     * 显示模型选择对话框
     */
    private fun showModelSelectionDialog() {
        val models = modelManager.scanModels().filter { it.isValid }

        if (models.isEmpty()) {
            showNoModelDialog()
            return
        }

        val items: Array<String> = models.map { model ->
            "${model.name} (${model.sizeMB}MB)"
        }.toTypedArray()

        AlertDialog.Builder(this)
            .setTitle("Select model (${models.size} found)")
            .setItems(items) { _, which ->
                val selectedModel = models[which]
                loadModelAndStart(selectedModel.path)
            }
            .setNeutralButton("Download New Model") { _, _ ->
                showModelInputDialog()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    /**
     * 显示模型输入对话框
     */
    private fun showModelInputDialog() {
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(50, 30, 50, 30)
        }

        val modelScopeIdLabel = TextView(this).apply {
            text = "ModelScope ID:"
            textSize = 14f
        }
        layout.addView(modelScopeIdLabel)

        val modelScopeIdInput = EditText(this).apply {
            hint = "e.g., Qwen/Qwen2.5-1.5B-Instruct-GGUF"
            setText("Qwen/Qwen2.5-1.5B-Instruct-GGUF")
        }
        layout.addView(modelScopeIdInput)

        val fileNameLabel = TextView(this).apply {
            text = "File Name:"
            textSize = 14f
            setPadding(0, 20, 0, 0)
        }
        layout.addView(fileNameLabel)

        val fileNameInput = EditText(this).apply {
            hint = "e.g., qwen2.5-1.5b-instruct-q4_k_m.gguf"
            setText("qwen2.5-1.5b-instruct-q4_k_m.gguf")
        }
        layout.addView(fileNameInput)

        val infoLabel = TextView(this).apply {
            text = "Find these on ModelScope.cn"
            textSize = 12f
            setPadding(0, 20, 0, 0)
            setTextColor(android.graphics.Color.GRAY)
        }
        layout.addView(infoLabel)

        AlertDialog.Builder(this)
            .setTitle("Download Model from ModelScope")
            .setView(layout)
            .setPositiveButton("Download") { _, _ ->
                val modelScopeId = modelScopeIdInput.text.toString().trim()
                val fileName = fileNameInput.text.toString().trim()

                if (modelScopeId.isEmpty() || fileName.isEmpty()) {
                    Toast.makeText(this, "Please fill in both fields", Toast.LENGTH_SHORT).show()
                    return@setPositiveButton
                }

                startModelDownload(modelScopeId, fileName)
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    /**
     * 开始下载模型
     */
    private fun startModelDownload(modelScopeId: String, fileName: String) {
        val modelDir = getExternalFilesDir("models")?.absolutePath
        if (modelDir == null) {
            Toast.makeText(this, "Unable to access storage directory", Toast.LENGTH_SHORT).show()
            return
        }

        binding.downloadButton.isEnabled = false
        updateStatusText("Downloading: $fileName...")

        lifecycleScope.launch {
            downloader.downloadModel(
                modelScopeId = modelScopeId,
                fileName = fileName,
                downloadDir = modelDir,
                listener = object : ModelDownloader.DownloadProgressListener {
                    override fun onProgress(percentage: Int, downloadedBytes: Long, totalBytes: Long) {
                        val safePercentage = minOf(percentage, 100)
                        runOnUiThread {
                            val totalMB = totalBytes / (1024L * 1024)
                            val downloadedMB = downloadedBytes / (1024L * 1024)
                            updateStatusText("Downloading: $safePercentage% ($downloadedMB MB / $totalMB MB)")
                        }
                    }

                    override fun onSuccess(filePath: String) {
                        runOnUiThread {
                            updateStatusText("Download complete, loading model...")
                            binding.downloadButton.isEnabled = true
                            loadModelAndStart(filePath)
                            Toast.makeText(this@MainActivity, "Model downloaded successfully!", Toast.LENGTH_SHORT).show()
                        }
                    }

                    override fun onError(message: String) {
                        runOnUiThread {
                            updateStatusText("Download failed: $message")
                            Toast.makeText(this@MainActivity, "Download failed: $message", Toast.LENGTH_LONG).show()
                            binding.downloadButton.isEnabled = true
                        }
                    }
                }
            )
        }
    }

    /**
     * 加载模型并启动
     */
    private fun loadModelAndStart(modelPath: String) {
        updateStatusText("Loading model...")
        viewModel.loadModel(modelPath)
    }

    /**
     * 显示设置对话框
     *
     * 演示如何调整各种参数
     */
    private fun showSettingsDialog() {
        val config = viewModel.getConfig()

        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(50, 30, 50, 30)
        }

        // Temperature
        val tempLabel = TextView(this).apply {
            text = "Temperature: ${String.format("%.1f", config.temperature)}"
            textSize = 14f
        }
        layout.addView(tempLabel)

        val tempInput = EditText(this).apply {
            hint = "0.0 - 2.0"
            setText(config.temperature.toString())
            inputType = android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL
        }
        layout.addView(tempInput)

        // Max History Pairs
        val historyLabel = TextView(this).apply {
            text = "Max History Pairs: ${config.maxHistoryPairs}"
            textSize = 14f
            setPadding(0, 20, 0, 0)
        }
        layout.addView(historyLabel)

        val historyInput = EditText(this).apply {
            hint = "e.g., 10"
            setText(config.maxHistoryPairs.toString())
            inputType = android.text.InputType.TYPE_CLASS_NUMBER
        }
        layout.addView(historyInput)

        // Max Tokens
        val tokensLabel = TextView(this).apply {
            text = "Max Tokens: ${config.maxTokens}"
            textSize = 14f
            setPadding(0, 20, 0, 0)
        }
        layout.addView(tokensLabel)

        val tokensInput = EditText(this).apply {
            hint = "e.g., 512"
            setText(config.maxTokens.toString())
            inputType = android.text.InputType.TYPE_CLASS_NUMBER
        }
        layout.addView(tokensInput)

        AlertDialog.Builder(this)
            .setTitle("Chat Settings")
            .setView(layout)
            .setPositiveButton("Save") { _, _ ->
                try {
                    val newTemp = tempInput.text.toString().toFloatOrNull() ?: config.temperature
                    val newHistory = historyInput.text.toString().toIntOrNull() ?: config.maxHistoryPairs
                    val newTokens = tokensInput.text.toString().toIntOrNull() ?: config.maxTokens

                    viewModel.setTemperature(newTemp)
                    viewModel.setMaxHistoryPairs(newHistory)
                    viewModel.setMaxTokens(newTokens)

                    Toast.makeText(this, "Settings saved", Toast.LENGTH_SHORT).show()
                } catch (e: Exception) {
                    Toast.makeText(this, "Invalid input", Toast.LENGTH_SHORT).show()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun updateStatusText(text: String) {
        binding.statusText.text = text
    }

    /**
     * 存储诊断工具 - 长按状态栏调用
     */
    private fun showStorageDiagnostics() {
        val info = buildString {
            appendLine("=== 存储诊断信息 ===")
            appendLine()

            // 1. 应用存储路径
            val externalFilesDir = getExternalFilesDir("models")
            appendLine("【当前使用的路径】")
            appendLine("路径: ${externalFilesDir?.absolutePath ?: "NULL"}")
            appendLine("存在: ${externalFilesDir?.exists() ?: false}")
            appendLine()

            // 2. 扫描的模型文件
            val models = modelManager.scanModels()
            appendLine("【扫描到的模型】")
            if (models.isEmpty()) {
                appendLine("❌ 未找到任何模型文件")
            } else {
                models.forEachIndexed { index, model ->
                    appendLine("${index + 1}. ${model.name}")
                    appendLine("   大小: ${model.sizeMB} MB")
                    appendLine("   有效: ${if (model.isValid) "✓" else "✗"}")
                    appendLine("   路径: ${model.path}")

                    // 检查文件是否真实存在
                    val file = File(model.path)
                    appendLine("   文件存在: ${file.exists()}")
                    if (file.exists()) {
                        appendLine("   可读: ${file.canRead()}")
                    }
                }
            }
            appendLine()

            // 3. 目录内容
            val modelsDir = File(externalFilesDir?.absolutePath ?: "")
            appendLine("【目录完整内容】")
            if (modelsDir.exists() && modelsDir.isDirectory) {
                val allFiles = modelsDir.listFiles()
                if (allFiles.isNullOrEmpty()) {
                    appendLine("❌ 目录为空！")
                } else {
                    appendLine("共 ${allFiles.size} 个文件:")
                    allFiles.forEach { file ->
                        appendLine("- ${file.name} (${file.length() / (1024 * 1024)} MB)")
                    }
                }
            } else {
                appendLine("❌ 目录不存在或不可访问")
            }
            appendLine()

            // 4. 存储空间信息
            appendLine("【存储空间】")
            val externalDir = getExternalFilesDir(null)
            if (externalDir != null) {
                val usableSpace = externalDir.usableSpace / (1024 * 1024)
                val totalSpace = externalDir.totalSpace / (1024 * 1024)
                appendLine("可用空间: $usableSpace MB")
                appendLine("总空间: $totalSpace MB")
            }
            appendLine()

            // 5. 重要提示
            appendLine("【重要说明】")
            appendLine("⚠️ getExternalFilesDir() 的数据会在以下情况消失:")
            appendLine("1. 卸载应用")
            appendLine("2. 清除应用数据")
            appendLine("3. 系统自动清理（某些情况下）")
            appendLine()
            appendLine("💡 如果模型总是消失，检查:")
            appendLine("1. 是否频繁重装应用?")
            appendLine("2. 是否在设置中清除了数据?")
            appendLine("3. 下载是否真的完成?")
        }

        // 显示诊断信息
        AlertDialog.Builder(this)
            .setTitle("存储诊断")
            .setMessage(info)
            .setPositiveButton("复制") { _, _ ->
                val clipboard = getSystemService(CLIPBOARD_SERVICE) as android.content.ClipboardManager
                val clip = android.content.ClipData.newPlainText("Storage Diagnostics", info)
                clipboard.setPrimaryClip(clip)
                Toast.makeText(this, "已复制到剪贴板", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("关闭", null)
            .setNeutralButton("刷新") { _, _ ->
                showStorageDiagnostics()
            }
            .show()
    }
}