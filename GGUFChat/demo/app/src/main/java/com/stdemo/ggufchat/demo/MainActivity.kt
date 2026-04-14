package com.stdemo.ggufchat.demo

import android.os.Bundle
import android.util.Log
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.stdemo.ggufchat.GGUFChatEngine
import com.stdemo.ggufchat.demo.databinding.ActivityMainBinding
import kotlinx.coroutines.launch
import java.io.File

/**
 * Demo MainActivity - 展示如何使用llama-android.aar中的GGUF聊天功能
 */
class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "DemoMainActivity"
    }

    private lateinit var binding: ActivityMainBinding
    private val engine = GGUFChatEngine()
    private val messageAdapter = MessageAdapter()
    private val messages = mutableListOf<Message>()
    private var isGenerating = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupRecyclerView()
        setupClickListeners()

        // 初始化引擎
        initializeEngine()
    }

    private fun setupRecyclerView() {
        binding.recyclerView.apply {
            layoutManager = LinearLayoutManager(this@MainActivity)
            adapter = messageAdapter
        }
    }

    private fun setupClickListeners() {
        binding.sendButton.setOnClickListener {
            val userInput = binding.inputEditText.text.toString()
            if (userInput.isNotBlank() && !isGenerating) {
                binding.inputEditText.text.clear()
                sendMessage(userInput)
            }
        }

        binding.clearButton.setOnClickListener {
            clearChat()
        }
    }

    private fun initializeEngine() {
        binding.statusText.text = "正在初始化..."

        lifecycleScope.launch {
            try {
                // 自动查找模型文件
                val modelPath = findModelFile()
                if (modelPath != null) {
                    loadModel(modelPath)
                } else {
                    showModelSelectionDialog()
                }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to initialize engine", e)
                binding.statusText.text = "初始化失败: ${e.message}"
            }
        }
    }

    private suspend fun loadModel(modelPath: String) {
        try {
            binding.statusText.text = "正在加载模型..."
            engine.loadModel(modelPath)
            binding.statusText.text = "模型就绪 - ${engine.getModelInfo()}"
            binding.sendButton.isEnabled = true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load model", e)
            binding.statusText.text = "模型加载失败: ${e.message}"
            showModelSelectionDialog()
        }
    }

    private fun sendMessage(userInput: String) {
        addMessage(Message(userInput, isUser = true))

        lifecycleScope.launch {
            try {
                isGenerating = true
                binding.sendButton.isEnabled = false
                binding.statusText.text = "正在生成回复..."

                // 生成回复（使用流式输出）
                val response = engine.generate(userInput) { token ->
                    // 流式 token 回调
                    runOnUiThread {
                        // 可以在这里实现实时显示 token
                    }
                }

                // 处理 Result 类型
                response.getOrNull()?.let { generatedText ->
                    addMessage(Message(generatedText, isUser = false))
                } ?: run {
                    addMessage(Message("生成失败", isUser = false))
                }
                binding.statusText.text = "模型就绪"

            } catch (e: Exception) {
                Log.e(TAG, "Failed to generate response", e)
                addMessage(Message("错误: ${e.message}", isUser = false))
                binding.statusText.text = "生成失败"
            } finally {
                isGenerating = false
                binding.sendButton.isEnabled = true
            }
        }
    }

    private fun addMessage(message: Message) {
        messages.add(message)
        messageAdapter.submitList(messages.toList())
        binding.recyclerView.scrollToPosition(messages.size - 1)
    }

    private fun clearChat() {
        messages.clear()
        messageAdapter.submitList(emptyList())
        engine.clearHistory()
        Toast.makeText(this, "对话已清空", Toast.LENGTH_SHORT).show()
    }

    private fun findModelFile(): String? {
        // 搜索可能的模型位置
        val possiblePaths = listOf(
            File(filesDir, "models/llm/"),
            File(filesDir, "models/"),
            File("/sdcard/Download/")
        )

        for (dir in possiblePaths) {
            if (dir.exists()) {
                val modelFile = dir.listFiles()?.firstOrNull { it.extension == "gguf" }
                if (modelFile != null) {
                    return modelFile.absolutePath
                }
            }
        }
        return null
    }

    private fun showModelSelectionDialog() {
        val modelsDir = File(filesDir, "models/llm")
        if (!modelsDir.exists()) {
            modelsDir.mkdirs()
        }

        val modelFiles = mutableListOf<File>()

        // 搜索所有可能的目录
        listOf(modelsDir, File("/sdcard/Download/")).forEach { dir ->
            if (dir.exists()) {
                modelFiles.addAll(dir.listFiles()?.filter { it.extension == "gguf" } ?: emptyList())
            }
        }

        if (modelFiles.isEmpty()) {
            AlertDialog.Builder(this)
                .setTitle("需要模型文件")
                .setMessage("请在以下位置放置 .gguf 模型文件：\n\n" +
                          "1. ${modelsDir.absolutePath}\n" +
                          "2. /sdcard/Download/\n\n" +
                          "然后重启应用。")
                .setPositiveButton("确定", null)
                .show()
        } else {
            val modelNames = modelFiles.map { "${it.name} (${it.parent})" }.toTypedArray()
            AlertDialog.Builder(this)
                .setTitle("选择模型文件")
                .setItems(modelNames) { _, which ->
                    lifecycleScope.launch {
                        loadModel(modelFiles[which].absolutePath)
                    }
                }
                .show()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        engine.release()
    }
}
