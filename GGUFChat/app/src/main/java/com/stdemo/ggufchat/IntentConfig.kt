package com.stdemo.ggufchat

/**
 * Intent Recognition Configuration
 *
 * Centralized configuration for intent recognition.
 * Change DEFAULT_CONFIDENCE_THRESHOLD here to apply globally.
 */
object IntentConfig {
    /**
     * Default confidence threshold for intent matching
     *
     * Only predictions with confidence >= this value will be considered as "HIT"
     *
     * Recommended values:
     * - 0.6: Loose matching (more intent hits, more false positives)
     * - 0.75: Balanced
     * - 0.85: Strict matching (fewer intent hits, fewer false positives)
     * - 0.95: Very strict (only very confident predictions)
     */
    var DEFAULT_CONFIDENCE_THRESHOLD = 0.85f
        set(value) {
            require(value in 0f..1f) { "Threshold must be between 0 and 1, got $value" }
            field = value
        }

    /**
     * Number of threads for ONNX Runtime inference
     */
    var DEFAULT_NUM_THREADS = 4
        set(value) {
            require(value > 0) { "Number of threads must be positive, got $value" }
            field = value
        }

    /**
     * Intents that should fallback to LLM even when hit
     *
     * These intents will be recognized but won't trigger intent handlers.
     * Instead, they will be passed to the LLM for natural conversation.
     *
     * Example: "chat-chat" is a general chat intent that should use LLM
     */
    var FALLBACK_TO_LLM_INTENTS = setOf(
        "chat-chat"
    )

    /**
     * Check if an intent should fallback to LLM
     */
    fun shouldFallbackToLLM(intent: String): Boolean {
        return FALLBACK_TO_LLM_INTENTS.any {
            intent.equals(it, ignoreCase = true)
        }
    }
}
