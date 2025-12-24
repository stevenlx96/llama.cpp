package com.stdemo.ggufchat

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * Chat ViewModel - UI层与引擎的桥梁
 *
 * 职责：
 * 1. 管理UI状态
 * 2. 协调用户输入和引擎生成
 * 3. 处理消息列表
 */
class ChatViewModel : ViewModel() {

    private val llamaEngine = GGUFChatEngine()

    private val _messages = MutableStateFlow<List<Message>>(emptyList())
    val messages: StateFlow<List<Message>> = _messages.asStateFlow()

    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading.asStateFlow()

    private val _modelStatus = MutableStateFlow("Model not loaded")
    val modelStatus: StateFlow<String> = _modelStatus.asStateFlow()

    private val _isGenerating = MutableStateFlow(false)
    val isGenerating: StateFlow<Boolean> = _isGenerating.asStateFlow()

    /**
     * 加载模型
     *
     * @param modelPath 模型文件路径
     */
    fun loadModel(modelPath: String) {
        viewModelScope.launch {
            _modelStatus.value = "Loading model..."
            val result = llamaEngine.loadModel(modelPath)

            if (result.isSuccess) {
                _modelStatus.value = "Model ready"
                addMessage(Message("你好，请问有什么可以帮助你的吗？", isUser = false))
            } else {
                _modelStatus.value = "Model load failed: ${result.exceptionOrNull()?.message}"
                addMessage(Message(
                    "Failed to load model: ${result.exceptionOrNull()?.message}",
                    isUser = false
                ))
            }
        }
    }

    /**
     * 发送消息并生成回复
     *
     * @param text 用户消息
     */
    fun sendMessage(text: String) {
        if (text.isBlank()) return
        if (!llamaEngine.isModelLoaded()) {
            addMessage(Message("Model not loaded. Please load a model first.", isUser = false))
            return
        }

        viewModelScope.launch {
            addMessage(Message(text, isUser = true))

            _isLoading.value = true
            _isGenerating.value = true

            val assistantMessage = Message("", isUser = false)
            addMessage(assistantMessage)
            val assistantMessageIndex = _messages.value.lastIndex

            val result = llamaEngine.generate(
                userInput = text,
                onTokenGenerated = { token ->
                    val currentMessages = _messages.value.toMutableList()
                    if (currentMessages.isNotEmpty() && assistantMessageIndex < currentMessages.size) {
                        val lastMessage = currentMessages[assistantMessageIndex]
                        val updatedMessage = lastMessage.copy(content = lastMessage.content + token)
                        currentMessages[assistantMessageIndex] = updatedMessage
                        _messages.value = currentMessages
                    }
                }
            )

            if (result.isSuccess) {
                val generatedText = result.getOrNull() ?: ""
                val currentMessages = _messages.value.toMutableList()
                if (currentMessages.isNotEmpty() && assistantMessageIndex < currentMessages.size) {
                    val lastMessage = currentMessages[assistantMessageIndex]
                    // 如果消息为空，说明是静态模式，直接设置文本
                    if (lastMessage.content.isEmpty()) {
                        currentMessages[assistantMessageIndex] = Message(generatedText, isUser = false)
                        _messages.value = currentMessages
                    }
                }
            } else if (result.isFailure) {
                val errorMsg = result.exceptionOrNull()?.message ?: "Unknown error"
                val currentMessages = _messages.value.toMutableList()
                if (currentMessages.isNotEmpty() && assistantMessageIndex < currentMessages.size) {
                    currentMessages[assistantMessageIndex] =
                        Message("Sorry, generation failed: $errorMsg", isUser = false)
                    _messages.value = currentMessages
                }
            }

            _isLoading.value = false
            _isGenerating.value = false
        }
    }

    /**
     * 停止当前的生成
     */
    fun stopGeneration() {
        llamaEngine.stopGeneration()
        _isGenerating.value = false
        _isLoading.value = false
    }

    /**
     * 清除聊天记录
     */
    fun clearChat() {
        llamaEngine.clearHistory()
        _messages.value = emptyList()
        addMessage(Message("Chat cleared. How can I help you?", isUser = false))
    }

    /**
     * 切换输出模式
     */
    fun toggleStreamingMode() {
        val currentMode = llamaEngine.isStreamingModeEnabled()
        llamaEngine.setStreamingMode(!currentMode)
    }

    /**
     * 获取当前输出模式
     */
    fun isStreamingMode(): Boolean {
        return llamaEngine.isStreamingModeEnabled()
    }

    /**
     * 设置系统提示词
     *
     * @param prompt 系统提示词
     */
    fun setSystemPrompt(prompt: String) {
        llamaEngine.setSystemPrompt(prompt)
    }

    /**
     * 设置温度参数
     *
     * @param temperature 温度值 (0.0-2.0)
     */
    fun setTemperature(temperature: Float) {
        llamaEngine.setTemperature(temperature)
    }

    /**
     * 设置TopP参数
     *
     * @param topP TopP值 (0.0-1.0)
     */
    fun setTopP(topP: Float) {
        llamaEngine.setTopP(topP)
    }

    /**
     * 设置TopK参数
     *
     * @param topK TopK值
     */
    fun setTopK(topK: Int) {
        llamaEngine.setTopK(topK)
    }

    /**
     * 设置最大生成Token数
     *
     * @param maxTokens 最大Token数
     */
    fun setMaxTokens(maxTokens: Int) {
        llamaEngine.setMaxTokens(maxTokens)
    }

    /**
     * 设置保留的最大对话轮数
     *
     * @param maxPairs 最多保留的对话对数
     */
    fun setMaxHistoryPairs(maxPairs: Int) {
        llamaEngine.setMaxHistoryPairs(maxPairs)
    }

    /**
     * 获取当前配置
     */
    fun getConfig(): ChatConfig {
        return llamaEngine.getConfig()
    }

    /**
     * 设置完整的配置
     *
     * @param config ChatConfig对象
     */
    fun setConfig(config: ChatConfig) {
        llamaEngine.setConfig(config)
    }

    /**
     * 获取当前对话历史轮数
     */
    fun getHistorySize(): Int {
        return llamaEngine.getHistorySize()
    }

    /**
     * 检查模型是否已加载
     */
    fun isModelLoaded(): Boolean {
        return llamaEngine.isModelLoaded()
    }

    /**
     * 获取模型信息
     */
    fun getModelInfo(): String {
        return llamaEngine.getModelInfo()
    }

    /**
     * 添加消息到列表
     */
    private fun addMessage(message: Message) {
        _messages.value = _messages.value + message
    }

    override fun onCleared() {
        super.onCleared()
        llamaEngine.release()
    }
}