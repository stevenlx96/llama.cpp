package com.stdemo.ggufchat

import android.os.Handler
import android.os.Looper
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.concurrent.atomic.AtomicBoolean

class GGUFChatEngine {

    companion object {
        private const val TAG = "GGUFChatEngine"
    }

    // Native methods
    private external fun nativeInit(modelPath: String, nThreads: Int): Long

    // Static (non-streaming) completion - returns complete response at once
    private external fun nativeCompletion(
        contextPtr: Long,
        prompt: String,
        nPredict: Int,
        temperature: Float,
        topP: Float,
        topK: Int
    ): String

    // Streaming completion - calls callback for each token
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

    // Internal state
    private var streamingMode = true
    private var contextPtr: Long = 0
    private var isModelLoaded = false
    private var modelPath: String? = null
    private val mainHandler = Handler(Looper.getMainLooper())
    private val isGenerating = AtomicBoolean(false)
    private val shouldStopGeneration = AtomicBoolean(false)

    // Configuration and history
    private val promptBuilder = ChatPromptBuilder()
    private var config = ChatConfig()

    init {
        Log.d(TAG, "Initializing GGUFChatEngine, loading native libraries...")
        try {
            // --- 核心修复：注入 NPU 搜索路径 ---
            // 尝试通过多种方式获取 nativeLibraryDir
            val nativeLibDir = try {
                // 1. 如果能拿到 context 就用 context (最标准)
                // 这里我们尝试通过类加载器找到路径，或者在后面 loadModel 时再设置
                // 但最简单的办法是在 System.loadLibrary 之前拿到当前应用的路径

                // 这种方式不需要依赖隐藏 API
                val info = java.io.File("/proc/self/maps").takeIf { it.exists() }
                // 实际上，更通用的做法是在 Engine 初始化时由外部传入 Context
                // 或者直接通过反射获取当前的 Application (比 AppGlobals 安全一点点)
                val clazz = Class.forName("android.app.ActivityThread")
                val method = clazz.getDeclaredMethod("currentApplication")
                val app = method.invoke(null) as? android.app.Application
                app?.applicationContext?.applicationInfo?.nativeLibraryDir
            } catch (e: Exception) {
                Log.w(TAG, "Could not resolve nativeLibDir via reflection: ${e.message}")
                null
            }

            if (nativeLibDir != null) {
                try {
                    val adspPath = "$nativeLibDir;/vendor/lib/rfsa/adsp;/vendor/dsp/cdsp"
                    android.system.Os.setenv("ADSP_LIBRARY_PATH", adspPath, true)
                    android.system.Os.setenv("CDSP_LIBRARY_PATH", adspPath, true)

                    // 启用详细的 NPU 日志和性能分析
                    android.system.Os.setenv("GGML_HEXAGON_VERBOSE", "1", true)
                    android.system.Os.setenv("GGML_HEXAGON_PROFILE", "1", true)

                    Log.d(TAG, "NPU search path successfully injected: $nativeLibDir")
                    Log.d(TAG, "NPU verbose logging and profiling enabled")
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to set environment variables", e)
                }
            }
            // --- 修复结束 ---

            System.loadLibrary("llama-android")
            Log.d(TAG, "Successfully loaded llama-android (JNI wrapper)")

        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "FATAL: Failed to load llama-android JNI wrapper", e)
            throw RuntimeException("Cannot load JNI wrapper library", e)
        } catch (e: Exception) {
            Log.e(TAG, "FATAL: Failed to initialize native libraries", e)
            throw RuntimeException("Native library initialization failed", e)
        }
    }

    suspend fun loadModel(path: String): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val file = java.io.File(path)
            if (!file.exists()) {
                return@withContext Result.failure(Exception("Model file not found: $path"))
            }

            Log.d(TAG, "Loading model from: $path")
            Log.d(TAG, "Model size: ${file.length() / 1024 / 1024} MB")

            val numThreads = Runtime.getRuntime().availableProcessors()
            contextPtr = nativeInit(path, numThreads)

            if (contextPtr == 0L) {
                return@withContext Result.failure(Exception("Model loading failed"))
            }

            modelPath = path
            isModelLoaded = true
            promptBuilder.clearHistory()
            isGenerating.set(false)
            shouldStopGeneration.set(false)

            Log.d(TAG, "Model loaded successfully, context ptr: $contextPtr")
            Result.success(Unit)
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Native library not loaded", e)
            Result.failure(e)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load model", e)
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

            Log.d(TAG, "Generate called, streaming mode: $streamingMode")

            val prompt = promptBuilder.buildChatPrompt(config.systemPrompt, userInput)

            val response = if (streamingMode) {
                generateStreaming(prompt, onTokenGenerated)
            } else {
                generateStatic(prompt)
            }

            Log.d(TAG, "Generated response length: ${response.length}")

            val cleanedResponse = promptBuilder.cleanResponse(response)

            promptBuilder.addToHistory(userInput, cleanedResponse, config.maxHistoryPairs)

            Log.d(TAG, "Generation completed, response length: ${cleanedResponse.length}")
            Result.success(cleanedResponse)
        } catch (e: Exception) {
            Log.e(TAG, "Generation failed with exception", e)
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
    fun setConfig(config: ChatConfig) {
        this.config = config
        Log.d(TAG, "Config updated: $config")
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

    fun setMaxHistoryPairs(maxPairs: Int) {
        require(maxPairs >= 0) { "MaxHistoryPairs must be >= 0" }
        config = config.copy(maxHistoryPairs = maxPairs)
        promptBuilder.setMaxHistoryPairs(maxPairs)
        Log.d(TAG, "MaxHistoryPairs set to: $maxPairs")
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
        promptBuilder.clearHistory()
        Log.d(TAG, "History cleared")
    }

    fun getHistorySize(): Int = promptBuilder.getHistorySize()

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
        promptBuilder.clearHistory()
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
