package com.stdemo.ggufchat

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

class ChatViewModel(application: Application) : AndroidViewModel(application) {

    private val llamaEngine = GGUFChatEngine()

    private val _messages = MutableStateFlow<List<Message>>(emptyList())
    val messages: StateFlow<List<Message>> = _messages.asStateFlow()

    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading.asStateFlow()

    private val _modelStatus = MutableStateFlow("Model not loaded")
    val modelStatus: StateFlow<String> = _modelStatus.asStateFlow()

    private val _isGenerating = MutableStateFlow(false)
    val isGenerating: StateFlow<Boolean> = _isGenerating.asStateFlow()

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

    fun stopGeneration() {
        llamaEngine.stopGeneration()
        _isGenerating.value = false
        _isLoading.value = false
    }

    fun clearChat() {
        llamaEngine.clearHistory()
        _messages.value = emptyList()
        addMessage(Message("Chat cleared. How can I help you?", isUser = false))
    }

    fun toggleStreamingMode() {
        val currentMode = llamaEngine.isStreamingModeEnabled()
        llamaEngine.setStreamingMode(!currentMode)
    }

    fun isStreamingMode(): Boolean {
        return llamaEngine.isStreamingModeEnabled()
    }

    fun setSystemPrompt(prompt: String) {
        llamaEngine.setSystemPrompt(prompt)
    }

    fun setTemperature(temperature: Float) {
        llamaEngine.setTemperature(temperature)
    }

    fun setTopP(topP: Float) {
        llamaEngine.setTopP(topP)
    }

    fun setTopK(topK: Int) {
        llamaEngine.setTopK(topK)
    }

    fun setMaxTokens(maxTokens: Int) {
        llamaEngine.setMaxTokens(maxTokens)
    }

    fun setMaxHistoryPairs(maxPairs: Int) {
        llamaEngine.setMaxHistoryPairs(maxPairs)
    }

    fun getConfig(): ChatConfig {
        return llamaEngine.getConfig()
    }

    fun setConfig(config: ChatConfig) {
        llamaEngine.setConfig(config)
    }

    fun getHistorySize(): Int {
        return llamaEngine.getHistorySize()
    }

    fun isModelLoaded(): Boolean {
        return llamaEngine.isModelLoaded()
    }

    fun getModelInfo(): String {
        return llamaEngine.getModelInfo()
    }

    private fun addMessage(message: Message) {
        _messages.value = _messages.value + message
    }

    override fun onCleared() {
        super.onCleared()
        llamaEngine.release()
    }
}
