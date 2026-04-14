package com.stdemo.ggufchat

import android.content.Context
import android.util.Log

/**
 * Llama Helper - UE/Unity 集成辅助类
 *
 * 为外部引擎（UE, Unity 等）提供简化的 Llama Chat 初始化和调用接口
 * 自动处理 Context 获取和初始化逻辑
 *
 * 使用示例：
 * ```kotlin
 * // 1. 创建实例（自动获取 Context）
 * val llama = LlamaHelper.create()
 *
 * // 2. 设置回调
 * llama.setOnResponse(object : LlamaHelper.ResponseCallback {
 *     override fun onResponse(text: String) {
 *         Log.i(TAG, "生成结果: $text")
 *     }
 *
 *     override fun onError(error: String) {
 *         Log.e(TAG, "错误: $error")
 *     }
 * })
 *
 * // 或使用简化的回调（lambda）
 * llama.setOnResponse { text ->
 *     Log.i(TAG, "生成结果: $text")
 * }
 *
 * // 3. 初始化
 * val success = llama.initialize()
 *
 * // 4. 发送消息
 * llama.sendMessage("你好，请介绍一下自己")
 *
 * // 5. 释放资源
 * llama.release()
 * ```
 */
class LlamaHelper private constructor(context: Context) {

    private val manager: LlamaManagerJava
    private val context: Context
    private var isInitialized: Boolean = false
    private var callback: ResponseCallback? = null

    init {
        this.context = context.applicationContext
        this.manager = LlamaManagerJava(this.context)
    }

    companion object {
        private const val TAG = "LlamaHelper"
        private var instance: LlamaHelper? = null

        /**
         * 创建 LlamaHelper 实例（自动获取 Context）
         *
         * 使用 Application Context，避免内存泄漏
         *
         * @return LlamaHelper 实例，失败返回 null
         */
        fun create(): LlamaHelper? {
            if (instance != null && instance!!.isInitialized) {
                Log.w(TAG, "LlamaHelper instance already exists, reusing")
                return instance
            }

            // 通过静态方法获取 Context
            val context = getContext()
            if (context == null) {
                Log.e(TAG, "Failed to get Context")
                return null
            }

            instance = LlamaHelper(context)
            return instance
        }

        /**
         * 使用指定的 Context 创建实例
         *
         * @param context Android Context
         * @return LlamaHelper 实例
         */
        fun createWithContext(context: Context): LlamaHelper {
            if (instance != null && instance!!.isInitialized) {
                Log.w(TAG, "LlamaHelper instance already exists, reusing")
                return instance!!
            }

            instance = LlamaHelper(context)
            return instance!!
        }

        /**
         * 获取全局 Context
         *
         * 通过反射获取 Application Context
         * 适用于 UE/Unity 等无法直接访问 Context 的环境
         *
         * @return Application Context，失败返回 null
         */
        private fun getContext(): Context? {
            try {
                // 方式 1: 通过 ActivityThread 获取
                val activityThread = Class.forName("android.app.ActivityThread")
                val currentActivityThread = activityThread.getMethod("currentActivityThread").invoke(null)
                val application = activityThread.getMethod("getApplication").invoke(currentActivityThread)
                if (application is Context) {
                    return application
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to get Context via ActivityThread: ${e.message}")
            }

            try {
                // 方式 2: 通过 AppGlobals 获取（部分 ROM 支持）
                val appGlobals = Class.forName("android.app.AppGlobals")
                val application = appGlobals.getMethod("getInitialApplication").invoke(null)
                if (application is Context) {
                    return application
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to get Context via AppGlobals: ${e.message}")
            }

            Log.e(TAG, "Unable to get Application Context")
            return null
        }

        /**
         * 获取单例实例
         */
        fun getInstance(): LlamaHelper? = instance

        /**
         * 检查是否已创建实例
         */
        fun hasInstance(): Boolean = instance != null

        /**
         * 重置单例（用于测试或重新初始化）
         */
        fun reset() {
            instance?.release()
            instance = null
        }
    }

    /**
     * 初始化 Llama Chat
     *
     * 同步初始化，会阻塞调用线程
     *
     * @return 是否初始化成功
     */
    fun initialize(): Boolean {
        if (isInitialized) {
            Log.w(TAG, "Already initialized")
            return true
        }

        return try {
            Log.d(TAG, "Initializing Llama Chat...")

            // 设置内部回调
            setupInternalCallbacks()

            // 初始化模型
            val success = manager.initializeSync()

            if (success) {
                isInitialized = true
                Log.i(TAG, "Llama Chat initialized successfully")
            } else {
                Log.e(TAG, "Llama Chat initialization failed")
            }

            success
        } catch (e: Exception) {
            Log.e(TAG, "Exception during initialization", e)
            false
        }
    }

    /**
     * 初始化 Llama Chat（指定模型路径）
     *
     * @param modelPath 模型文件路径
     * @return 是否初始化成功
     */
    fun initialize(modelPath: String): Boolean {
        if (isInitialized) {
            Log.w(TAG, "Already initialized")
            return true
        }

        return try {
            Log.d(TAG, "Initializing Llama Chat with model: $modelPath")

            // 设置内部回调
            setupInternalCallbacks()

            // 初始化模型
            val success = manager.initializeSync(modelPath)

            if (success) {
                isInitialized = true
                Log.i(TAG, "Llama Chat initialized successfully")
            } else {
                Log.e(TAG, "Llama Chat initialization failed")
            }

            success
        } catch (e: Exception) {
            Log.e(TAG, "Exception during initialization", e)
            false
        }
    }

    /**
     * 设置响应回调
     *
     * @param callback 回调接口
     */
    fun setOnResponse(callback: ResponseCallback?) {
        this.callback = callback
    }

    /**
     * 设置简化的回调（仅响应，无错误）- Lambda 版本
     *
     * @param onResponse 响应回调 lambda
     */
    fun setOnResponse(onResponse: (String) -> Unit) {
        this.callback = object : ResponseCallback {
            override fun onResponse(text: String) {
                onResponse(text)
            }

            override fun onError(error: String) {
                Log.e(TAG, "Error: $error")
            }
        }
    }

    /**
     * 发送消息
     *
     * @param message 用户消息
     */
    fun sendMessage(message: String) {
        if (!isInitialized) {
            notifyError("Llama Chat not initialized")
            return
        }

        try {
            manager.sendMessage(message)
            Log.d(TAG, "Sent message: $message")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to send message", e)
            notifyError(e.message)
        }
    }

    /**
     * 开始新的对话
     */
    fun startNewChat() {
        if (!isInitialized) {
            return
        }

        try {
            manager.startNewChat()
            Log.d(TAG, "Started new chat")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start new chat", e)
            notifyError(e.message)
        }
    }

    /**
     * 获取对话历史
     *
     * @return 对话历史列表
     */
    fun getChatHistory(): List<String> {
        if (!isInitialized) {
            return emptyList()
        }

        return try {
            manager.getChatHistory()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to get chat history", e)
            emptyList()
        }
    }

    /**
     * 停止生成
     */
    fun stopGeneration() {
        if (!isInitialized) {
            return
        }

        try {
            manager.stopGeneration()
            Log.d(TAG, "Stopped generation")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to stop generation", e)
            notifyError(e.message)
        }
    }

    /**
     * 释放资源
     */
    fun release() {
        if (!isInitialized) {
            return
        }

        try {
            manager.release()
            isInitialized = false
            instance = null
            Log.d(TAG, "Released resources")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to release resources", e)
        }
    }

    /**
     * 检查是否已初始化
     */
    fun isInitialized(): Boolean = isInitialized

    /**
     * 检查是否正在生成
     */
    fun isGenerating(): Boolean {
        if (!isInitialized) {
            return false
        }
        return manager.isGenerating()
    }

    /**
     * 获取引擎信息
     */
    fun getInfo(): String {
        if (!isInitialized) {
            return "Not initialized"
        }
        return manager.getEngineInfo()
    }

    // ==================== 配置方法 ====================

    /**
     * 设置生成配置
     *
     * @param temperature 温度参数 (0.0-1.0)
     * @param maxTokens 最大生成 token 数
     * @param topP Top-p 采样参数
     */
    fun setGenerationConfig(temperature: Float, maxTokens: Int, topP: Float) {
        manager.setGenerationConfig(temperature, maxTokens, topP)
    }

    /**
     * 设置系统提示词
     *
     * @param systemPrompt 系统提示词
     */
    fun setSystemPrompt(systemPrompt: String) {
        manager.setSystemPrompt(systemPrompt)
    }

    // ==================== 内部方法 ====================

    private fun setupInternalCallbacks() {
        // 响应回调
        manager.setOnResponse(object : LlamaCallback<String> {
            override fun onSuccess(result: String) {
                Log.d(TAG, "Response: $result")
                notifyResponse(result)
            }

            override fun onError(error: Throwable) {
                Log.e(TAG, "Error in response callback", error)
                notifyError(error.message)
            }
        })

        // 错误回调
        manager.setOnError(object : ErrorCallback {
            override fun onError(error: String) {
                Log.e(TAG, "Error: $error")
                notifyError(error)
            }
        })

        // 状态变化回调
        manager.setOnStateChanged(object : StateChangedCallback {
            override fun onStateChanged(state: String) {
                Log.d(TAG, "State changed: $state")
            }
        })
    }

    private fun notifyResponse(text: String) {
        callback?.onResponse(text)
    }

    private fun notifyError(error: String?) {
        callback?.onError(error ?: "Unknown error")
    }

    // ==================== 回调接口 ====================

    /**
     * 响应回调接口
     */
    interface ResponseCallback {
        /**
         * 生成响应回调
         * @param text 生成的文本
         */
        fun onResponse(text: String)

        /**
         * 错误回调
         * @param error 错误信息
         */
        fun onError(error: String)
    }
}
