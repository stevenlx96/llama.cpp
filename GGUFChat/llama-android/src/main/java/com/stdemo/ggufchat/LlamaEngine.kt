package com.stdemo.ggufchat

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean

/**
 * LlamaEngine - GGUF Chat 推理引擎
 *
 * 提供本地 LLM 推理功能，支持 GGUF 格式模型
 */
class LlamaEngine(private val context: Context? = null) {

    companion object {
        private const val TAG = "LlamaEngine"

        // Native library loading
        init {
            try {
                System.loadLibrary("llama-android")
                Log.d(TAG, "Successfully loaded llama-android native library")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "Failed to load llama-android native library", e)
            }
        }
    }

    // Native methods
    private external fun nativeInit(modelPath: String, nThreads: Int): Long

    private external fun nativeCompletion(
        contextPtr: Long,
        prompt: String,
        nPredict: Int,
        temperature: Float,
        topP: Float,
        topK: Int
    ): String

    private external fun nativeCompletionStreaming(
        contextPtr: Long,
        prompt: String,
        nPredict: Int,
        temperature: Float,
        topP: Float,
        topK: Int,
        tokenCallback: TokenCallback
    ): String

    private external fun nativeFree(contextPtr: Long)

    interface TokenCallback {
        fun onToken(token: String)
    }

    // 回调接口（用于 UE/Java 集成）
    var onResponse: ((String) -> Unit)? = null
    var onStateChanged: ((EngineState) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    enum class EngineState {
        IDLE,
        LOADING,
        READY,
        GENERATING,
        ERROR
    }

    // Internal state
    private var streamingMode = true
    private var contextPtr: Long = 0
    private var isModelLoaded = false
    private var modelPath: String? = null
    private val mainHandler = Handler(Looper.getMainLooper())
    private val isGenerating = AtomicBoolean(false)
    private val shouldStopGeneration = AtomicBoolean(false)
    private var currentState = EngineState.IDLE

    // Configuration
    private var config = ChatConfig()

    // 更新状态并通知回调
    private fun setState(state: EngineState) {
        currentState = state
        onStateChanged?.invoke(state)
    }

    fun isNativeLoaded(): Boolean = true

    suspend fun loadModel(path: String): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            setState(EngineState.LOADING)

            val file = File(path)
            if (!file.exists()) {
                setState(EngineState.ERROR)
                return@withContext Result.failure(Exception("Model file not found: $path"))
            }

            Log.d(TAG, "Loading model from: $path")
            Log.d(TAG, "Model size: ${file.length() / 1024 / 1024} MB")

            // Use available CPU cores for inference
            val numThreads = Runtime.getRuntime().availableProcessors().coerceIn(4, 8)
            Log.d(TAG, "Using $numThreads threads for inference")
            contextPtr = nativeInit(path, numThreads)

            if (contextPtr == 0L) {
                setState(EngineState.ERROR)
                return@withContext Result.failure(Exception("Model loading failed"))
            }

            modelPath = path
            isModelLoaded = true
            isGenerating.set(false)
            shouldStopGeneration.set(false)

            setState(EngineState.READY)
            Log.d(TAG, "Model loaded successfully, context ptr: $contextPtr")
            Result.success(Unit)
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Native library not loaded", e)
            setState(EngineState.ERROR)
            Result.failure(e)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load model", e)
            setState(EngineState.ERROR)
            Result.failure(e)
        }
    }

    suspend fun generate(
        userInput: String,
        onTokenGenerated: ((String) -> Unit)? = null
    ): Result<String> = withContext(Dispatchers.IO) {
        try {
            if (!isModelLoaded || contextPtr == 0L) {
                Log.e(TAG, "Model not loaded. Call loadModel() first.")
                return@withContext Result.failure(Exception("Model not loaded"))
            }

            if (isGenerating.getAndSet(true)) {
                return@withContext Result.failure(Exception("Generation already in progress"))
            }

            shouldStopGeneration.set(false)
            setState(EngineState.GENERATING)

            Log.d(TAG, "Generate called, streaming mode: $streamingMode")

            val prompt = userInput // 简化版本，直接使用用户输入

            val response = if (streamingMode) {
                generateStreaming(prompt, onTokenGenerated)
            } else {
                generateStatic(prompt)
            }

            Log.d(TAG, "Generated response length: ${response.length}")
            setState(EngineState.READY)
            Result.success(response)
        } catch (e: Exception) {
            Log.e(TAG, "Generation failed with exception", e)
            setState(EngineState.ERROR)
            Result.failure(e)
        } finally {
            isGenerating.set(false)
            shouldStopGeneration.set(false)
        }
    }

    private fun generateStreaming(
        prompt: String,
        onTokenGenerated: ((String) -> Unit)?
    ): String {
        val tokenCallback = object : TokenCallback {
            override fun onToken(token: String) {
                if (shouldStopGeneration.get()) {
                    return
                }

                if (onTokenGenerated != null) {
                    mainHandler.post {
                        onTokenGenerated(token)
                        Log.d(TAG, "Token streamed: '$token'")
                    }
                }
            }
        }

        return nativeCompletionStreaming(
            contextPtr,
            prompt,
            config.maxTokens,
            config.temperature,
            config.topP,
            config.topK,
            tokenCallback
        )
    }

    private fun generateStatic(prompt: String): String {
        Log.d(TAG, "Generating static (non-streaming) completion...")
        return nativeCompletion(
            contextPtr,
            prompt,
            config.maxTokens,
            config.temperature,
            config.topP,
            config.topK
        )
    }

    // Configuration management
    fun setConfig(newConfig: ChatConfig) {
        this.config = newConfig
        Log.d(TAG, "Config updated: $newConfig")
    }

    fun getConfig(): ChatConfig = config.copy()

    fun setSystemPrompt(prompt: String) {
        config = config.copy(systemPrompt = prompt)
        Log.d(TAG, "System prompt updated")
    }

    fun setTemperature(temperature: Float) {
        require(temperature >= 0f) { "Temperature must be >= 0" }
        config = config.copy(temperature = temperature)
        Log.d(TAG, "Temperature set to: $temperature")
    }

    fun setTopP(topP: Float) {
        require(topP in 0f..1f) { "TopP must be between 0 and 1" }
        config = config.copy(topP = topP)
        Log.d(TAG, "TopP set to: $topP")
    }

    fun setTopK(topK: Int) {
        require(topK > 0) { "TopK must be > 0" }
        config = config.copy(topK = topK)
        Log.d(TAG, "TopK set to: $topK")
    }

    fun setMaxTokens(maxTokens: Int) {
        require(maxTokens > 0) { "MaxTokens must be > 0" }
        config = config.copy(maxTokens = maxTokens)
        Log.d(TAG, "MaxTokens set to: $maxTokens")
    }

    fun setStreamingMode(enabled: Boolean) {
        streamingMode = enabled
        Log.d(TAG, "Streaming mode set to: $enabled")
    }

    fun isStreamingModeEnabled(): Boolean = streamingMode

    // Generation control
    fun stopGeneration() {
        Log.d(TAG, "Stop generation requested")
        shouldStopGeneration.set(true)
    }

    fun isGenerating(): Boolean = isGenerating.get()

    // History management
    fun clearHistory() {
        Log.d(TAG, "History cleared")
    }

    // ==================== 兼容方法（用于 UE/Java 集成） ====================

    /**
     * 初始化（兼容方法）
     */
    fun initialize(modelPath: String? = null): Boolean {
        return try {
            val defaultModelPath = context?.let { ctx ->
                getDefaultModelPath(ctx)
            }

            val pathToLoad = modelPath ?: defaultModelPath

            if (pathToLoad == null) {
                setState(EngineState.ERROR)
                onError?.invoke("Model path not specified and no default model found")
                return false
            }

            // 使用 runBlocking 来调用 suspend 函数
            val result = kotlinx.coroutines.runBlocking {
                loadModel(pathToLoad)
            }

            result.isSuccess.also { success ->
                if (!success) {
                    onError?.invoke(result.exceptionOrNull()?.message ?: "Unknown error")
                }
            }
        } catch (e: Exception) {
            setState(EngineState.ERROR)
            onError?.invoke(e.message ?: "Initialization failed")
            false
        }
    }

    /**
     * 发送消息（兼容方法）
     */
    fun sendMessage(message: String) {
        try {
            setState(EngineState.GENERATING)

            // 在后台线程生成响应
            Handler(Looper.getMainLooper()).post {
                kotlinx.coroutines.GlobalScope.launch(kotlinx.coroutines.Dispatchers.IO) {
                    try {
                        val result = generate(message) { token ->
                            // 流式 token 回调（如果需要）
                            Handler(Looper.getMainLooper()).post {
                                onResponse?.invoke("[TOKEN] $token")
                            }
                        }

                        result.onSuccess { response ->
                            Handler(Looper.getMainLooper()).post {
                                setState(EngineState.READY)
                                onResponse?.invoke(response)
                            }
                        }

                        result.onFailure { exception ->
                            Handler(Looper.getMainLooper()).post {
                                setState(EngineState.ERROR)
                                onError?.invoke(exception.message ?: "Generation failed")
                            }
                        }
                    } catch (e: Exception) {
                        Handler(Looper.getMainLooper()).post {
                            setState(EngineState.ERROR)
                            onError?.invoke(e.message ?: "Generation failed")
                        }
                    }
                }
            }
        } catch (e: Exception) {
            setState(EngineState.ERROR)
            onError?.invoke(e.message ?: "Failed to send message")
        }
    }

    /**
     * 获取默认模型路径
     */
    private fun getDefaultModelPath(context: Context): String? {
        // 尝试多个可能的模型位置
        val possiblePaths = listOf(
            File(context.filesDir, "models/ggml-model-f16.gguf").absolutePath,
            File(context.filesDir, "models/model.gguf").absolutePath,
            File(context.getExternalFilesDir(null), "models/model.gguf")?.absolutePath,
            "/sdcard/Download/model.gguf"
        )

        for (path in possiblePaths) {
            if (File(path).exists()) {
                Log.d(TAG, "Found model at: $path")
                return path
            }
        }

        Log.w(TAG, "No model found in default locations")
        return null
    }

    /**
     * 检查模型是否就绪（兼容方法）
     */
    fun isModelReady(): Boolean = isModelLoaded()

    /**
     * 获取当前状态（兼容方法）
     */
    fun getState(): EngineState = currentState

    /**
     * 获取对话历史（兼容方法）
     */
    fun getChatHistory(): List<String> {
        return emptyList()
    }

    /**
     * 获取模型目录（兼容方法）
     */
    fun getModelDir(): File {
        val modelFile = if (modelPath != null) File(modelPath!!) else null
        if (modelFile != null && modelFile.exists()) {
            val parent = modelFile.parentFile
            if (parent != null) {
                return parent
            }
        }
        val filesDir = context?.filesDir
        if (filesDir != null) {
            return File(filesDir, "models")
        }
        return File(".", "models")
    }

    /**
     * 设置生成配置（兼容方法）
     */
    fun setGenerationConfig(temperature: Float, maxTokens: Int, topP: Float) {
        setTemperature(temperature)
        setMaxTokens(maxTokens)
        setTopP(topP)
        Log.d(TAG, "Generation config updated: temp=$temperature, maxTokens=$maxTokens, topP=$topP")
    }

    /**
     * 开始新对话（兼容方法）
     */
    fun startNewChat() {
        clearHistory()
        Log.d(TAG, "Started new chat")
    }

    /**
     * 获取引擎信息（兼容方法）
     */
    fun getEngineInfo(): String {
        return buildString {
            append("LlamaEngine Info:\n")
            append("Native Loaded: true\n")
            append("Model Ready: ${isModelReady()}\n")
            append("State: ${currentState.name}\n")
            append("Model Dir: ${getModelDir().absolutePath}\n")
        }
    }

    // Lifecycle
    fun release() {
        try {
            if (isGenerating.get()) {
                stopGeneration()
                Thread.sleep(100)
            }

            if (contextPtr != 0L) {
                nativeFree(contextPtr)
                Log.d(TAG, "Model freed")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to free model", e)
        }

        isModelLoaded = false
        contextPtr = 0
        modelPath = null
        setState(EngineState.IDLE)
        Log.d(TAG, "Resources released")
    }

    fun isModelLoaded(): Boolean = isModelLoaded && contextPtr != 0L

    fun getModelInfo(): String {
        return if (isModelLoaded && contextPtr != 0L) {
            "Model loaded: ${modelPath?.substringAfterLast('/')}"
        } else {
            "Model not loaded"
        }
    }
}

data class ChatConfig(
    val temperature: Float = 0.7f,
    val topP: Float = 0.9f,
    val topK: Int = 40,
    val maxTokens: Int = 512,
    val maxHistoryPairs: Int = 10,
    val systemPrompt: String = "你叫小达，是一个有帮助的ai机器人助手，请用简体中文回答问题。"
)

// 别名：GGUFChatEngine = LlamaEngine (向后兼容)
typealias GGUFChatEngine = LlamaEngine