package com.stdemo.ggufchat

import android.util.Log

/**
 * Data class representing a single slot extracted from input text
 */
data class IntentSlot(
    val slotType: String,   // e.g., "location", "time", "artist"
    val slotValue: String   // e.g., "北京", "明天", "周杰伦"
)

/**
 * Data class representing the prediction result
 */
data class IntentResult(
    var text: String = "",                      // Original input text
    var hit: Boolean = false,                   // True if confidence >= threshold
    var intent: String = "",                    // Predicted intent label (empty if !hit)
    var confidence: Float = 0.0f,               // Confidence score [0, 1]
    var slots: List<IntentSlot> = emptyList(),  // Extracted slots (empty if !hit)
    var rawIntent: String = ""                  // Raw predicted intent (for debugging)
)

/**
 * Intent Recognizer for routing user queries to specific handlers or LLM fallback
 *
 * Usage:
 * ```
 * val recognizer = IntentRecognizer()
 * if (recognizer.initialize(modelDir, threshold = 0.6f)) {
 *     val result = recognizer.predict("今天北京天气怎么样")
 *
 *     if (result.hit) {
 *         // ✅ HIT: Execute intent handler
 *         handleWeather(result.slots)
 *     } else {
 *         // ❌ NO HIT: Fallback to LLM
 *         useLLM(result.text)
 *     }
 * }
 * ```
 */
class IntentRecognizer {

    companion object {
        private const val TAG = "IntentRecognizer"

        init {
            try {
                System.loadLibrary("llama-android")  // 使用现有的JNI库
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "Failed to load llama-android library", e)
            }
        }
    }

    private var initialized = false
    private var contextPtr: Long = 0

    /**
     * Initialize the intent recognizer
     *
     * @param modelDir Path to directory containing ONNX model files
     * @param numThreads Number of CPU threads (default: 4)
     * @param confidenceThreshold Minimum confidence for intent "hit" (default: 0.6)
     * @return true if successful, false otherwise
     */
    fun initialize(
        modelDir: String,
        numThreads: Int = 4,
        confidenceThreshold: Float = 0.85f
    ): Boolean {
        return try {
            contextPtr = nativeIntentInit(modelDir, numThreads, confidenceThreshold)
            initialized = (contextPtr != 0L)
            Log.i(TAG, "Intent recognizer initialized: $initialized (threshold: ${confidenceThreshold * 100}%)")
            initialized
        } catch (e: UnsatisfiedLinkError) {
            Log.w(TAG, "Intent recognition not available - ONNX Runtime not included in build")
            Log.w(TAG, "To enable: add libonnxruntime.so to jniLibs/${System.getProperty("os.arch")}")
            false
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize intent recognizer", e)
            false
        }
    }

    /**
     * Predict intent for user input
     *
     * @param text User input text
     * @return IntentResult with hit status and intent info
     */
    fun predict(text: String): IntentResult {
        if (!initialized || contextPtr == 0L) {
            return IntentResult(text = text, hit = false)
        }

        return try {
            nativeIntentPredict(contextPtr, text) ?: IntentResult(text = text, hit = false)
        } catch (e: UnsatisfiedLinkError) {
            Log.w(TAG, "Intent recognition not available")
            IntentResult(text = text, hit = false)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to predict intent", e)
            IntentResult(text = text, hit = false)
        }
    }

    /**
     * Set confidence threshold
     *
     * @param threshold New threshold value [0, 1]
     */
    fun setThreshold(threshold: Float) {
        if (initialized && contextPtr != 0L) {
            try {
                nativeIntentSetThreshold(contextPtr, threshold)
                Log.i(TAG, "Threshold updated to: ${threshold * 100}%")
            } catch (e: UnsatisfiedLinkError) {
                Log.w(TAG, "Intent recognition not available")
            }
        }
    }

    /**
     * Get current threshold
     *
     * @return Current threshold value
     */
    fun getThreshold(): Float {
        return if (initialized && contextPtr != 0L) {
            try {
                nativeIntentGetThreshold(contextPtr)
            } catch (e: UnsatisfiedLinkError) {
                Log.w(TAG, "Intent recognition not available")
                0.0f
            }
        } else {
            0.0f
        }
    }

    /**
     * Release resources
     */
    fun release() {
        if (initialized && contextPtr != 0L) {
            try {
                nativeIntentFree(contextPtr)
                contextPtr = 0
                initialized = false
                Log.i(TAG, "Intent recognizer released")
            } catch (e: UnsatisfiedLinkError) {
                Log.w(TAG, "Intent recognition not available")
                contextPtr = 0
                initialized = false
            }
        }
    }

    /**
     * Check if initialized
     */
    fun isInitialized(): Boolean = initialized && contextPtr != 0L

    // Native methods
    private external fun nativeIntentInit(
        modelDir: String,
        numThreads: Int,
        confidenceThreshold: Float
    ): Long

    private external fun nativeIntentPredict(
        contextPtr: Long,
        text: String
    ): IntentResult?

    private external fun nativeIntentSetThreshold(contextPtr: Long, threshold: Float)
    private external fun nativeIntentGetThreshold(contextPtr: Long): Float
    private external fun nativeIntentFree(contextPtr: Long)
}

/**
 * Helper extension for routing logic
 */
fun IntentResult.routeOrFallback(
    onIntent: (intent: String, slots: List<IntentSlot>) -> Unit,
    onFallback: (text: String) -> Unit
) {
    if (hit) {
        onIntent(intent, slots)
    } else {
        onFallback(text)
    }
}

/**
 * Format result for logging
 */
fun IntentResult.formatLog(): String {
    return if (hit) {
        "✅ HIT: $intent (${(confidence * 100).toInt()}%) slots=${slots.size}"
    } else {
        "❌ NO HIT: ${(confidence * 100).toInt()}% (raw: $rawIntent)"
    }
}
