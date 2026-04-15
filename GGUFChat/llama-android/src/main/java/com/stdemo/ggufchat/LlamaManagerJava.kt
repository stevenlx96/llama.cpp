package com.stdemo.ggufchat

import android.content.Context
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.util.concurrent.CompletableFuture
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

/**
 * Java 友好的 Llama Chat Manager 包装类
 *
 * 为 Java 和 JNI (UE, Unity 等) 提供简单易用的 API
 * 解决 Kotlin suspend 函数和协程无法直接从 Java 调用的问题
 *
 * 使用示例：
 * ```java
 * LlamaManagerJava llama = new LlamaManagerJava(context);
 *
 * // 设置回调
 * llama.setOnResponse(new LlamaCallback() {
 *     @Override
 *     public void onSuccess(String text) {
 *         // 处理生成结果
 *         Log.i(TAG, "生成结果: " + text);
 *     }
 *
 *     @Override
 *     public void onError(Throwable error) {
 *         // 处理错误
 *         Log.e(TAG, "错误: " + error.getMessage());
 *     }
 * });
 *
 * // 同步初始化（阻塞）
 * boolean success = llama.initializeSync();
 *
 * // 发送消息
 * llama.sendMessage("你好，请介绍一下自己");
 *
 * // 开始新对话
 * llama.startNewChat();
 *
 * // 释放资源
 * llama.release();
 * ```
 *
 * 回调接口说明：
 * - OnResponse: 生成响应回调
 * - OnStateChanged: 状态变化回调
 * - OnError: 错误回调
 */
class LlamaManagerJava(private val context: Context) {

    companion object {
        private const val TAG = "LlamaManagerJava"
        private const val DEFAULT_TIMEOUT_MS = 30000L // 30 秒超时
    }

    // 核心管理器
    private val engine: LlamaEngine = LlamaEngine(context)

    // 主线程 Handler，用于回调
    private val mainHandler = android.os.Handler(android.os.Looper.getMainLooper())

    // 状态标志
    @Volatile
    private var isInitializedState = false

    // 回调接口
    private var onResponseCallback: LlamaCallback<String>? = null
    private var onStateChangedCallback: StateChangedCallback? = null
    private var onErrorCallback: ErrorCallback? = null
    private var onTokenCallback: TokenStreamCallback? = null

    // ==================== 初始化方法 ====================

    /**
     * 同步初始化（阻塞调用）
     *
     * ⚠️ 警告：此方法会阻塞调用线程，建议在后台线程调用
     *
     * @return 是否初始化成功
     */
    fun initializeSync(): Boolean {
        return try {
            Log.d(TAG, "Initializing synchronously...")

            // 设置内部回调
            setupInternalCallbacks()

            // 初始化引擎
            val success = engine.initialize()
            isInitializedState = success

            if (success) {
                Log.d(TAG, "Initialized successfully")
            } else {
                Log.e(TAG, "Initialization failed")
            }

            success
        } catch (e: Exception) {
            Log.e(TAG, "Exception in initializeSync", e)
            false
        }
    }

    /**
     * 同步初始化（指定模型路径）
     *
     * @param modelPath 模型文件路径
     * @return 是否初始化成功
     */
    fun initializeSync(modelPath: String): Boolean {
        return try {
            Log.d(TAG, "Initializing synchronously with model: $modelPath")

            // 设置内部回调
            setupInternalCallbacks()

            // 初始化引擎
            val success = engine.initialize(modelPath)
            isInitializedState = success

            if (success) {
                Log.d(TAG, "Initialized successfully")
            } else {
                Log.e(TAG, "Initialization failed")
            }

            success
        } catch (e: Exception) {
            Log.e(TAG, "Exception in initializeSync", e)
            false
        }
    }

    // ==================== 回调设置方法 ====================

    /**
     * 设置响应回调
     */
    fun setOnResponse(callback: LlamaCallback<String>?) {
        this.onResponseCallback = callback
    }

    /**
     * 设置状态变化回调
     */
    fun setOnStateChanged(callback: StateChangedCallback?) {
        this.onStateChangedCallback = callback
    }

    /**
     * 设置错误回调
     */
    fun setOnError(callback: ErrorCallback?) {
        this.onErrorCallback = callback
    }

    /**
     * 设置流式 token 回调（每个 token 一次）。
     *
     * 注意：[setOnResponse] 是「最终完整响应」回调，本回调是「流式逐 token」回调。
     * 同时设置时，token 通过本回调投递；最终响应通过 [setOnResponse] 投递。
     */
    fun setOnToken(callback: TokenStreamCallback?) {
        this.onTokenCallback = callback
    }

    // ==================== 消息发送方法 ====================

    /**
     * 发送消息
     *
     * @param message 用户消息
     */
    fun sendMessage(message: String) {
        if (!isInitializedState) {
            onErrorCallback?.onError("Not initialized")
            return
        }

        try {
            Log.d(TAG, "Sending message: $message")
            engine.sendMessage(message)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to send message", e)
            onErrorCallback?.onError(e.message ?: "Unknown error")
        }
    }

    /**
     * 开始新的对话
     */
    fun startNewChat() {
        if (!isInitializedState) {
            onErrorCallback?.onError("Not initialized")
            return
        }

        try {
            Log.d(TAG, "Starting new chat")
            engine.startNewChat()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start new chat", e)
            onErrorCallback?.onError(e.message ?: "Unknown error")
        }
    }

    /**
     * 停止生成
     */
    fun stopGeneration() {
        if (!isInitializedState) {
            onErrorCallback?.onError("Not initialized")
            return
        }

        try {
            Log.d(TAG, "Stopping generation")
            engine.stopGeneration()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to stop generation", e)
            onErrorCallback?.onError(e.message ?: "Unknown error")
        }
    }

    // ==================== 配置方法 ====================

    /**
     * 设置生成配置（一次设置温度、最大 token、TopP 三个参数）
     *
     * @param temperature 温度参数 (>= 0)
     * @param maxTokens 最大生成 token 数 (> 0)
     * @param topP Top-p 采样参数 (0.0 - 1.0)
     */
    fun setGenerationConfig(
        temperature: Float,
        maxTokens: Int,
        topP: Float
    ) {
        engine.setGenerationConfig(temperature, maxTokens, topP)
        Log.d(TAG, "Generation config updated")
    }

    /** 单独设置温度 */
    fun setTemperature(temperature: Float) {
        engine.setTemperature(temperature)
    }

    /** 单独设置 TopP */
    fun setTopP(topP: Float) {
        engine.setTopP(topP)
    }

    /** 单独设置 TopK */
    fun setTopK(topK: Int) {
        engine.setTopK(topK)
    }

    /** 单独设置最大生成 token 数 */
    fun setMaxTokens(maxTokens: Int) {
        engine.setMaxTokens(maxTokens)
    }

    /** 设置最大保留对话历史轮数（0 表示不保留） */
    fun setMaxHistoryPairs(maxPairs: Int) {
        engine.setMaxHistoryPairs(maxPairs)
    }

    /** 启用/禁用流式生成模式 */
    fun setStreamingMode(enabled: Boolean) {
        engine.setStreamingMode(enabled)
    }

    /** 是否启用了流式生成模式 */
    fun isStreamingModeEnabled(): Boolean = engine.isStreamingModeEnabled()

    /**
     * 设置系统提示词
     *
     * @param systemPrompt 系统提示词
     */
    fun setSystemPrompt(systemPrompt: String) {
        engine.setSystemPrompt(systemPrompt)
        Log.d(TAG, "System prompt updated")
    }

    /** 获取当前完整配置（temperature, topP, topK, maxTokens, maxHistoryPairs, systemPrompt） */
    fun getConfig(): ChatConfig = engine.getConfig()

    /** 整体替换配置 */
    fun setConfig(newConfig: ChatConfig) {
        engine.setConfig(newConfig)
    }

    /** 清除对话历史（不释放模型） */
    fun clearHistory() {
        engine.clearHistory()
    }

    /** 当前对话历史的轮数 */
    fun getHistorySize(): Int = engine.getHistorySize()

    /** 获取模型加载信息（已加载模型的文件名 / 未加载提示） */
    fun getModelInfo(): String = engine.getModelInfo()

    // ==================== 状态查询方法 ====================

    /**
     * 检查模型是否就绪
     */
    fun isModelReady(): Boolean {
        return engine.isModelReady()
    }

    /**
     * 获取当前状态
     * @return 状态字符串
     */
    fun getState(): String {
        return engine.getState().name
    }

    /**
     * 检查是否正在生成
     */
    fun isGenerating(): Boolean {
        return getState() == "GENERATING"
    }

    /**
     * 获取对话历史
     *
     * @return 对话历史列表
     */
    fun getChatHistory(): List<String> {
        return engine.getChatHistory()
    }

    /**
     * 获取模型目录路径
     */
    fun getModelDir(): String {
        return engine.getModelDir().absolutePath
    }

    // ==================== 生命周期管理 ====================

    /**
     * 释放所有资源
     */
    fun release() {
        try {
            Log.d(TAG, "Releasing resources...")
            engine.release()
            isInitializedState = false
            Log.d(TAG, "Resources released")
        } catch (e: Exception) {
            Log.e(TAG, "Error releasing", e)
        }
    }

    /**
     * 获取引擎信息
     */
    fun getEngineInfo(): String {
        return engine.getEngineInfo()
    }

    // ==================== 内部回调设置 ====================

    private fun setupInternalCallbacks() {
        // 响应回调 — 兼容 LlamaEngine.sendMessage() 现有契约：流式 token
        // 以 "[TOKEN] " 前缀通过 onResponse 回调投递，最终响应不带前缀。
        // 在 Java 层将两者拆开，token 走 TokenStreamCallback，最终响应走 LlamaCallback。
        engine.onResponse = object : (String) -> Unit {
            override fun invoke(text: String) {
                mainHandler.post(object : Runnable {
                    override fun run() {
                        try {
                            if (text.startsWith("[TOKEN] ")) {
                                val token = text.removePrefix("[TOKEN] ")
                                onTokenCallback?.onToken(token)
                            } else {
                                onResponseCallback?.onSuccess(text)
                            }
                        } catch (e: Exception) {
                            Log.e(TAG, "Error in response callback", e)
                        }
                    }
                })
            }
        }

        // 状态变化回调
        engine.onStateChanged = object : (LlamaEngine.EngineState) -> Unit {
            override fun invoke(state: LlamaEngine.EngineState) {
                mainHandler.post(object : Runnable {
                    override fun run() {
                        try {
                            onStateChangedCallback?.onStateChanged(state.name)
                        } catch (e: Exception) {
                            Log.e(TAG, "Error in state changed callback", e)
                        }
                    }
                })
            }
        }

        // 错误回调
        engine.onError = object : (String) -> Unit {
            override fun invoke(error: String) {
                mainHandler.post(object : Runnable {
                    override fun run() {
                        try {
                            onErrorCallback?.onError(error)
                        } catch (e: Exception) {
                            Log.e(TAG, "Error in error callback", e)
                        }
                    }
                })
            }
        }
    }
}

// ==================== 回调接口定义 ====================

/**
 * 通用回调接口
 */
interface LlamaCallback<T> {
    fun onSuccess(result: T)
    fun onError(error: Throwable)
}

/**
 * 状态变化回调接口
 */
interface StateChangedCallback {
    fun onStateChanged(state: String)
}

/**
 * 错误回调接口
 */
interface ErrorCallback {
    fun onError(error: String)
}

/**
 * 流式 token 回调接口（每个 token 一次）
 */
interface TokenStreamCallback {
    fun onToken(token: String)
}
