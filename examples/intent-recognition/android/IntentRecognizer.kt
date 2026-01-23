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
    var intent: String = "",                    // Predicted intent label
    var confidence: Float = 0.0f,               // Confidence score [0, 1]
    var slots: List<IntentSlot> = emptyList()   // Extracted slots
)

/**
 * Intent Recognizer class for joint intent classification and slot filling
 *
 * This class provides a JNI wrapper around the C++ ONNX-based intent recognizer.
 *
 * Usage:
 * ```
 * val recognizer = IntentRecognizer()
 * if (recognizer.initialize(modelDir = "/path/to/model", numThreads = 4)) {
 *     val result = recognizer.predict("今天北京天气怎么样")
 *     println("Intent: ${result.intent} (${result.confidence * 100}%)")
 *     result.slots.forEach { slot ->
 *         println("  ${slot.slotType}: ${slot.slotValue}")
 *     }
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
     * @return true if initialization succeeds, false otherwise
     */
    fun initialize(modelDir: String, numThreads: Int = 4): Boolean {
        return nativeInit(modelDir, numThreads)
    }

    /**
     * Predict intent and slots for input text
     *
     * @param text Input text string (UTF-8)
     * @return IntentResult object containing intent and slots (or empty result on error)
     */
    fun predict(text: String): IntentResult {
        if (!isInitialized()) {
            throw IllegalStateException("IntentRecognizer not initialized. Call initialize() first.")
        }

        return nativePredict(text) ?: IntentResult()
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
    private external fun nativeInit(modelDir: String, numThreads: Int): Boolean
    private external fun nativePredict(text: String): IntentResult?
    private external fun nativeRelease()
    private external fun nativeIsInitialized(): Boolean
}

/**
 * Extension function to format IntentResult as a readable string
 */
fun IntentResult.format(): String {
    val sb = StringBuilder()
    sb.appendLine("=" .repeat(50))
    sb.appendLine("Input: $text")
    sb.appendLine("-".repeat(50))
    sb.appendLine("Intent: $intent (confidence: ${"%.2f".format(confidence * 100)}%)")
    sb.appendLine("-".repeat(50))

    if (slots.isNotEmpty()) {
        sb.appendLine("Slots:")
        slots.forEach { slot ->
            sb.appendLine("  - ${slot.slotType}: ${slot.slotValue}")
        }
    } else {
        sb.appendLine("Slots: (none)")
    }

    sb.appendLine("=" .repeat(50))
    return sb.toString()
}
