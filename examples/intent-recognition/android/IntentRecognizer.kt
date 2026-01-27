package com.llama.cpp

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
 * Intent Recognizer class for joint intent classification and slot filling
 *
 * This class provides a JNI wrapper around the C++ ONNX-based intent recognizer.
 *
 * Usage example with threshold:
 * ```
 * val recognizer = IntentRecognizer()
 * if (recognizer.initialize(
 *     modelDir = "/path/to/model",
 *     numThreads = 4,
 *     confidenceThreshold = 0.6f
 * )) {
 *     val result = recognizer.predict("今天北京天气怎么样")
 *
 *     if (result.hit) {
 *         // Intent recognized with high confidence
 *         println("✅ HIT! Intent: ${result.intent}")
 *         handleIntent(result.intent, result.slots)
 *     } else {
 *         // Confidence too low, fallback to LLM
 *         println("❌ NO HIT, fallback to LLM")
 *         useLLM(result.text)
 *     }
 *
 *     recognizer.release()
 * }
 * ```
 */
class IntentRecognizer {

    companion object {
        init {
            try {
                System.loadLibrary("intent_jni")
            } catch (e: UnsatisfiedLinkError) {
                throw RuntimeException("Failed to load intent_jni library: ${e.message}", e)
            }
        }
    }

    /**
     * Initialize the recognizer with model directory
     *
     * @param modelDir Path to directory containing ONNX model and label files
     * @param numThreads Number of CPU threads for inference (default: 4)
     * @param confidenceThreshold Minimum confidence [0, 1] for intent "hit" (default: 0.6)
     * @return true if initialization succeeds, false otherwise
     */
    fun initialize(
        modelDir: String,
        numThreads: Int = 4,
        confidenceThreshold: Float = 0.6f
    ): Boolean {
        return nativeInit(modelDir, numThreads, confidenceThreshold)
    }

    /**
     * Predict intent and slots for input text
     *
     * @param text Input text string (UTF-8)
     * @return IntentResult object with 'hit' field indicating if intent was matched
     */
    fun predict(text: String): IntentResult {
        if (!isInitialized()) {
            throw IllegalStateException("IntentRecognizer not initialized. Call initialize() first.")
        }

        return nativePredict(text) ?: IntentResult()
    }

    /**
     * Set confidence threshold for intent matching
     *
     * @param threshold Minimum confidence [0, 1] to consider intent as "hit"
     */
    fun setThreshold(threshold: Float) {
        nativeSetThreshold(threshold)
    }

    /**
     * Get current confidence threshold
     *
     * @return Current threshold value
     */
    fun getThreshold(): Float {
        return nativeGetThreshold()
    }

    /**
     * Release resources
     *
     * Should be called when the recognizer is no longer needed to free memory.
     */
    fun release() {
        nativeRelease()
    }

    /**
     * Check if recognizer is initialized
     *
     * @return true if initialized, false otherwise
     */
    fun isInitialized(): Boolean {
        return nativeIsInitialized()
    }

    // Native methods
    private external fun nativeInit(
        modelDir: String,
        numThreads: Int,
        confidenceThreshold: Float
    ): Boolean
    private external fun nativePredict(text: String): IntentResult?
    private external fun nativeSetThreshold(threshold: Float)
    private external fun nativeGetThreshold(): Float
    private external fun nativeRelease()
    private external fun nativeIsInitialized(): Boolean
}

/**
 * Extension function to format IntentResult as a readable string
 *
 * @param showDebug If true, show raw intent even when !hit
 */
fun IntentResult.format(showDebug: Boolean = false): String {
    val sb = StringBuilder()
    sb.appendLine("=".repeat(60))
    sb.appendLine("Input: $text")
    sb.appendLine("-".repeat(60))

    if (hit) {
        // HIT: Intent recognized with sufficient confidence
        sb.appendLine("✅ HIT! Intent: $intent")
        sb.appendLine("   Confidence: ${"%.2f".format(confidence * 100)}%")

        if (slots.isNotEmpty()) {
            sb.appendLine("   Slots:")
            slots.forEach { slot ->
                sb.appendLine("     - ${slot.slotType}: ${slot.slotValue}")
            }
        }

        sb.appendLine("-".repeat(60))
        sb.appendLine("   → Execute intent handler")
    } else {
        // NO HIT: Confidence below threshold
        sb.appendLine("❌ NO HIT (confidence: ${"%.2f".format(confidence * 100)}% < threshold)")

        if (showDebug && rawIntent.isNotEmpty()) {
            sb.appendLine("   [Debug] Best guess was: $rawIntent")
        }

        sb.appendLine("-".repeat(60))
        sb.appendLine("   → Fallback to LLaMA inference")
    }

    sb.appendLine("=".repeat(60))
    return sb.toString()
}

/**
 * Integration example: Route to handler or fallback to LLM
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
