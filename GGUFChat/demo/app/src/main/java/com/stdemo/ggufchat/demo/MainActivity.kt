package com.stdemo.ggufchat.demo

import android.os.Build
import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.stdemo.ggufchat.GGUFChatEngine  // 来自AAR包
import com.stdemo.ggufchat.Message  // 来自AAR包
import com.stdemo.ggufchat.ModelDownloader  // 来自AAR包
import com.stdemo.ggufchat.ModelManager  // 来自AAR包
import com.stdemo.ggufchat.demo.databinding.ActivityMainBinding
import kotlinx.coroutines.launch

/**
 * Demo MainActivity - 展示如何使用AAR包中的GGUFChatEngine
 *
 * 所有核心功能都来自llama-android.aar：
 * - GGUFChatEngine: 聊天引擎
 * - Message: 消息数据类
 * - ModelDownloader: 模型下载器
 * - ModelManager: 模型扫描器
 * - ChatConfig: 配置类
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val engine = GGUFChatEngine()  // 来自AAR包
    private val messageAdapter = MessageAdapter()
    private val downloader = ModelDownloader()  // 来自AAR包
    private lateinit var modelManager: ModelManager
    private val messages = mutableListOf<Message>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val modelsDir = getExternalFilesDir("models")?.absolutePath ?: return
        modelManager = ModelManager(modelsDir)

        setupRecyclerView()
        setupClickListeners()
        updateStreamingStatus()
        requestStoragePermission()
    }

    private fun setupRecyclerView() {
        binding.recyclerView.apply {
            layoutManager = LinearLayoutManager(this@MainActivity)
            adapter = messageAdapter
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
            updateStreamingStatus()
        }

        binding.stopButton.setOnClickListener {
            engine.stopGeneration()
            Toast.makeText(this, "Generation stopped", Toast.LENGTH_SHORT).show()
        }

        binding.settingsButton?.setOnClickListener {
            showSettingsDialog()
        }
    }

    private fun sendMessage(text: String) {
        if (!engine.isModelLoaded()) {
            Toast.makeText(this, "Please load a model first", Toast.LENGTH_SHORT).show()
            return
        }

        addMessage(Message(text, isUser = true))
        binding.sendButton.isEnabled = false
        binding.stopButton.isEnabled = true

        lifecycleScope.launch {
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
            // Android 13+ (API 33+)
            // 注意：getExternalFilesDir() 不需要权限，但为了兼容性还是请求
            if (checkSelfPermission(Manifest.permission.READ_MEDIA_IMAGES) == PackageManager.PERMISSION_GRANTED) {
                // 权限已授予，直接加载
                tryLoadModel()
            } else {
                // 请求Android 13+的新权限
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
            // Android 5及以下
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
            .setMessage("Click 'Download Model' to download one from ModelScope.")
            .setPositiveButton("Download Model") { _, _ -> showModelInputDialog() }
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

        AlertDialog.Builder(this)
            .setTitle("Settings")
            .setView(layout)
            .setPositiveButton("Save") { _, _ ->
                tempInput.text.toString().toFloatOrNull()?.let { engine.setTemperature(it) }
                tokensInput.text.toString().toIntOrNull()?.let { engine.setMaxTokens(it) }
                Toast.makeText(this, "Settings saved", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    override fun onDestroy() {
        super.onDestroy()
        engine.release()
    }
}
