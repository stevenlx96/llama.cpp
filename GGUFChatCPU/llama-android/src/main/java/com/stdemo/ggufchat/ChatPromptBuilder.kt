package com.stdemo.ggufchat

import android.util.Log

/**
 * Chat Prompt Builder - Prompt构建和响应处理
 *
 * 职责：
 * 1. ChatML格式Prompt构建
 * 2. 响应清理和后处理
 * 3. 对话历史管理（支持轮数限制）
 */
internal class ChatPromptBuilder {
    companion object {
        private const val TAG = "ChatPromptBuilder"
    }

    private val conversationHistory = mutableListOf<String>()
    private var maxHistoryPairs = 10  // 默认保留最近10轮对话

    /**
     * 设置最大保留历史轮数
     *
     * @param maxPairs 最多保留的对话对数（0表示不保留历史）
     */
    fun setMaxHistoryPairs(maxPairs: Int) {
        require(maxPairs >= 0) { "maxPairs must be >= 0" }
        this.maxHistoryPairs = maxPairs
        Log.d(TAG, "Max history pairs set to: $maxPairs")
        trimHistory()  // 立即修剪历史
    }

    /**
     * 修剪历史记录以符合最大轮数限制
     */
    private fun trimHistory() {
        if (maxHistoryPairs == 0) {
            return  // 不限制
        }

        val maxItems = maxHistoryPairs * 2  // 每对包含2个项（用户+助手）
        if (conversationHistory.size > maxItems) {
            val itemsToRemove = conversationHistory.size - maxItems
            repeat(itemsToRemove) {
                conversationHistory.removeAt(0)
            }
            Log.d(TAG, "History trimmed, removed $itemsToRemove items, remaining: ${conversationHistory.size}")
        }
    }

    /**
     * 构建标准ChatML格式的Prompt
     *
     * 格式：
     * <|im_start|>system
     * {systemPrompt}
     * <|im_end|>
     * <|im_start|>user
     * {previousUserMessage}
     * <|im_end|>
     * <|im_start|>assistant
     * {previousAssistantMessage}
     * <|im_end|>
     * <|im_start|>user
     * {currentUserMessage}
     * <|im_end|>
     * <|im_start|>assistant
     */
    fun buildChatPrompt(systemPrompt: String, userInput: String): String {
        val builder = StringBuilder()

        // System message
        builder.append("<|im_start|>system\n")
        builder.append(systemPrompt)
        builder.append("<|im_end|>\n")

        // Add conversation history
        for (i in conversationHistory.indices step 2) {
            if (i + 1 < conversationHistory.size) {
                builder.append("<|im_start|>user\n")
                builder.append(conversationHistory[i])
                builder.append("<|im_end|>\n")

                builder.append("<|im_start|>assistant\n")
                builder.append(conversationHistory[i + 1])
                builder.append("<|im_end|>\n")
            }
        }

        // Current user message
        builder.append("<|im_start|>user\n")
        builder.append(userInput)
        builder.append("<|im_end|>\n")

        // Assistant start marker
        builder.append("<|im_start|>assistant\n")

        Log.d(TAG, "Built prompt with history length: ${conversationHistory.size}")
        return builder.toString()
    }

    /**
     * 清理响应文本 - 移除ChatML标记
     *
     * 处理的标记：
     * - <|im_end|> - 消息结束标记
     * - <|im_start|>system - 系统消息起始
     * - <|im_start|>user - 用户消息起始
     * - <|im_start|>assistant - 助手消息起始
     */
    fun cleanResponse(response: String): String {
        var cleaned = response.trim()

        Log.d(TAG, "Cleaning response, input length: ${cleaned.length}")

        // Remove all ChatML markers
        cleaned = cleaned.replace("<|im_end|>", "")
        cleaned = cleaned.replace("<|im_start|>system", "")
        cleaned = cleaned.replace("<|im_start|>user", "")
        cleaned = cleaned.replace("<|im_start|>assistant", "")

        cleaned = cleaned.trim()
        Log.d(TAG, "Cleaned response, output length: ${cleaned.length}")

        return cleaned
    }

    /**
     * 添加到对话历史
     *
     * @param userMessage 用户消息
     * @param assistantMessage 助手回复
     * @param maxPairs 最大保留轮数（用于及时修剪）
     */
    fun addToHistory(userMessage: String, assistantMessage: String, maxPairs: Int = maxHistoryPairs) {
        conversationHistory.add(userMessage)
        conversationHistory.add(assistantMessage)

        // 更新maxHistoryPairs并修剪
        if (maxPairs != maxHistoryPairs) {
            this.maxHistoryPairs = maxPairs
        }
        trimHistory()

        Log.d(TAG, "Added to history, total pairs: ${conversationHistory.size / 2}")
    }

    /**
     * 清除对话历史
     */
    fun clearHistory() {
        conversationHistory.clear()
        Log.d(TAG, "Conversation history cleared")
    }

    /**
     * 获取历史记录大小
     *
     * @return 对话对数
     */
    fun getHistorySize(): Int = conversationHistory.size / 2

    /**
     * 获取完整的历史记录（用于调试或导出）
     *
     * @return List<Pair<String, String>> - (用户消息, 助手回复)对列表
     */
    fun getHistoryList(): List<Pair<String, String>> {
        val result = mutableListOf<Pair<String, String>>()
        for (i in conversationHistory.indices step 2) {
            if (i + 1 < conversationHistory.size) {
                result.add(conversationHistory[i] to conversationHistory[i + 1])
            }
        }
        return result
    }
}