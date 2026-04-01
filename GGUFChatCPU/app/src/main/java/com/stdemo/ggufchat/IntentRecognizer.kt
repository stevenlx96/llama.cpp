package com.stdemo.ggufchat

import ai.onnxruntime.OnnxTensor
import ai.onnxruntime.OrtEnvironment
import ai.onnxruntime.OrtSession
import android.util.Log
import org.json.JSONObject
import java.io.BufferedReader
import java.io.File
import java.io.FileReader
import java.nio.LongBuffer
import kotlin.math.exp

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
    var text: String = "",
    var hit: Boolean = false,
    var intent: String = "",
    var confidence: Float = 0.0f,
    var slots: List<IntentSlot> = emptyList(),
    var rawIntent: String = ""
)

/**
 * Intent Recognizer using ONNX Runtime Java API (pure Kotlin, no C++ JNI)
 *
 * Usage:
 * ```
 * val recognizer = IntentRecognizer()
 * if (recognizer.initialize(modelDir, threshold = 0.6f)) {
 *     val result = recognizer.predict("今天北京天气怎么样")
 *     if (result.hit) {
 *         handleWeather(result.slots)
 *     } else {
 *         useLLM(result.text)
 *     }
 * }
 * ```
 */
class IntentRecognizer {

    companion object {
        private const val TAG = "IntentRecognizer"
    }

    private var ortEnv: OrtEnvironment? = null
    private var ortSession: OrtSession? = null
    private var initialized = false

    private var intentLabels: List<String> = emptyList()
    private var slotLabels: List<String> = emptyList()
    private var vocab: Map<String, Long> = emptyMap()

    private var padTokenId: Long = 0
    private var clsTokenId: Long = 101
    private var sepTokenId: Long = 102
    private var unkTokenId: Long = 100

    private var maxSeqLen: Int = 64
    private var confidenceThreshold: Float = IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD

    /**
     * Initialize the intent recognizer
     *
     * @param modelDir Path to directory containing ONNX model files
     * @param numThreads Number of CPU threads (default: from IntentConfig)
     * @param confidenceThreshold Minimum confidence for intent "hit" (default: from IntentConfig)
     * @return true if successful, false otherwise
     */
    fun initialize(
        modelDir: String,
        numThreads: Int = IntentConfig.DEFAULT_NUM_THREADS,
        confidenceThreshold: Float = IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD
    ): Boolean {
        this.confidenceThreshold = confidenceThreshold

        return try {
            // Load config
            loadConfig(modelDir)

            // Load labels
            intentLabels = loadLabels(modelDir, "intent_label.txt")
            if (intentLabels.isEmpty()) {
                Log.e(TAG, "Failed to load intent labels")
                return false
            }

            slotLabels = loadLabels(modelDir, "slot_label.txt")
            if (slotLabels.isEmpty()) {
                Log.e(TAG, "Failed to load slot labels")
                return false
            }

            // Load vocabulary
            loadVocab(modelDir)

            // Initialize ONNX Runtime
            ortEnv = OrtEnvironment.getEnvironment()
            val sessionOptions = OrtSession.SessionOptions()
            sessionOptions.setIntraOpNumThreads(numThreads)
            sessionOptions.setOptimizationLevel(OrtSession.SessionOptions.OptLevel.NO_OPT)

            val modelPath = findModelFile(modelDir) ?: run {
                Log.e(TAG, "ONNX model file not found in: $modelDir")
                return false
            }

            ortSession = ortEnv!!.createSession(modelPath, sessionOptions)
            initialized = true

            Log.i(TAG, "Intent recognizer initialized (threshold: ${(confidenceThreshold * 100).toInt()}%)")
            Log.i(TAG, "Intent labels: ${intentLabels.size}, Slot labels: ${slotLabels.size}")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize intent recognizer", e)
            false
        }
    }

    /**
     * Predict intent for user input
     */
    fun predict(text: String): IntentResult {
        if (!initialized || ortSession == null || ortEnv == null) {
            return IntentResult(text = text, hit = false)
        }

        return try {
            val tokens = tokenize(text)

            // Convert to IDs
            val inputIds = LongArray(maxSeqLen) { padTokenId }
            val attentionMask = LongArray(maxSeqLen) { 0L }
            for (i in tokens.indices) {
                inputIds[i] = vocab[tokens[i]] ?: unkTokenId
                attentionMask[i] = 1L
            }

            // Create tensors
            val inputShape = longArrayOf(1, maxSeqLen.toLong())
            val inputIdsTensor = OnnxTensor.createTensor(
                ortEnv, LongBuffer.wrap(inputIds), inputShape
            )
            val attentionMaskTensor = OnnxTensor.createTensor(
                ortEnv, LongBuffer.wrap(attentionMask), inputShape
            )

            // Build inputs map using session's input names
            val inputNames = ortSession!!.inputNames.toList()
            val inputs = mutableMapOf<String, OnnxTensor>()
            if (inputNames.size >= 2) {
                inputs[inputNames[0]] = inputIdsTensor
                inputs[inputNames[1]] = attentionMaskTensor
            } else {
                inputIdsTensor.close()
                attentionMaskTensor.close()
                return IntentResult(text = text, hit = false)
            }

            // Run inference
            val results = ortSession!!.run(inputs)

            // Process intent output (first output)
            val intentLogits = extractIntentLogits(results[0].value)
            if (intentLogits == null) {
                inputIdsTensor.close()
                attentionMaskTensor.close()
                results.close()
                return IntentResult(text = text, hit = false)
            }

            softmax(intentLogits)

            // Find best intent
            var bestIdx = 0
            var bestConf = intentLogits[0]
            for (i in 1 until intentLogits.size.coerceAtMost(intentLabels.size)) {
                if (intentLogits[i] > bestConf) {
                    bestConf = intentLogits[i]
                    bestIdx = i
                }
            }

            val rawIntent = if (bestIdx < intentLabels.size) intentLabels[bestIdx] else ""
            val hit = bestConf >= confidenceThreshold

            // Process slot output (second output)
            val chars = utf8SplitChars(text)
            val slotTags = extractSlotTags(results[1].value, tokens)

            val result = IntentResult(
                text = text,
                hit = hit,
                intent = if (hit) rawIntent else "",
                confidence = bestConf,
                slots = if (hit) extractSlots(chars, slotTags) else emptyList(),
                rawIntent = rawIntent
            )

            // Cleanup
            inputIdsTensor.close()
            attentionMaskTensor.close()
            results.close()

            if (hit) {
                Log.i(TAG, "Prediction HIT: intent=$rawIntent, confidence=${(bestConf * 100).toInt()}%")
            } else {
                Log.i(TAG, "Prediction NO HIT: confidence=${(bestConf * 100).toInt()}% (raw=$rawIntent)")
            }

            result
        } catch (e: Exception) {
            Log.e(TAG, "Failed to predict intent", e)
            IntentResult(text = text, hit = false)
        }
    }

    fun setThreshold(threshold: Float) {
        confidenceThreshold = threshold
        Log.i(TAG, "Threshold updated to: ${(threshold * 100).toInt()}%")
    }

    fun getThreshold(): Float = confidenceThreshold

    fun release() {
        try {
            ortSession?.close()
            ortSession = null
            initialized = false
            Log.i(TAG, "Intent recognizer released")
        } catch (e: Exception) {
            Log.e(TAG, "Error releasing intent recognizer", e)
            ortSession = null
            initialized = false
        }
    }

    fun isInitialized(): Boolean = initialized && ortSession != null

    // ---- Private helpers ----

    private fun loadConfig(modelDir: String) {
        try {
            val configFile = File(modelDir, "android_config.json")
            if (configFile.exists()) {
                val json = JSONObject(configFile.readText())
                if (json.has("max_seq_len")) {
                    maxSeqLen = json.getInt("max_seq_len")
                    Log.i(TAG, "Loaded max_seq_len from config: $maxSeqLen")
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Could not load config, using defaults: ${e.message}")
        }
    }

    private fun loadLabels(modelDir: String, filename: String): List<String> {
        val labels = mutableListOf<String>()
        try {
            BufferedReader(FileReader(File(modelDir, filename))).use { reader ->
                reader.forEachLine { line ->
                    val trimmed = line.trim()
                    if (trimmed.isNotEmpty()) {
                        labels.add(trimmed)
                    }
                }
            }
            Log.i(TAG, "Loaded ${labels.size} labels from $filename")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load labels from $filename", e)
        }
        return labels
    }

    private fun loadVocab(modelDir: String) {
        val vocabMap = mutableMapOf<String, Long>()
        try {
            var id = 0L
            BufferedReader(FileReader(File(modelDir, "vocab.txt"))).use { reader ->
                reader.forEachLine { line ->
                    val trimmed = line.trim()
                    if (trimmed.isNotEmpty()) {
                        vocabMap[trimmed] = id++
                    }
                }
            }
            Log.i(TAG, "Loaded vocabulary with ${vocabMap.size} tokens")
            vocabMap["[PAD]"]?.let { padTokenId = it }
            vocabMap["[CLS]"]?.let { clsTokenId = it }
            vocabMap["[SEP]"]?.let { sepTokenId = it }
            vocabMap["[UNK]"]?.let { unkTokenId = it }
        } catch (e: Exception) {
            Log.w(TAG, "Could not load vocab: ${e.message}")
        }
        vocab = vocabMap
    }

    private fun findModelFile(modelDir: String): String? {
        val quantized = File(modelDir, "joint_model_quantized.onnx")
        if (quantized.exists()) return quantized.absolutePath
        val nonQuantized = File(modelDir, "joint_model.onnx")
        if (nonQuantized.exists()) return nonQuantized.absolutePath
        return null
    }

    private fun tokenize(text: String): List<String> {
        val tokens = mutableListOf("[CLS]")
        tokens.addAll(utf8SplitChars(text))
        tokens.add("[SEP]")
        if (tokens.size > maxSeqLen) {
            val truncated = tokens.subList(0, maxSeqLen - 1).toMutableList()
            truncated.add("[SEP]")
            return truncated
        }
        return tokens
    }

    private fun utf8SplitChars(text: String): List<String> {
        val chars = mutableListOf<String>()
        var i = 0
        while (i < text.length) {
            val cp = text.codePointAt(i)
            val ch = String(Character.toChars(cp))
            if (ch != " " && ch != "\t" && ch != "\n" && ch != "\r") {
                chars.add(ch)
            }
            i += Character.charCount(cp)
        }
        return chars
    }

    private fun softmax(data: FloatArray) {
        var maxVal = Float.NEGATIVE_INFINITY
        for (v in data) if (v > maxVal) maxVal = v
        var sum = 0f
        for (i in data.indices) {
            data[i] = exp((data[i] - maxVal).toDouble()).toFloat()
            sum += data[i]
        }
        for (i in data.indices) data[i] /= sum
    }

    @Suppress("UNCHECKED_CAST")
    private fun extractIntentLogits(output: Any?): FloatArray? {
        return when (output) {
            is Array<*> -> (output as Array<FloatArray>)[0]
            else -> {
                Log.e(TAG, "Unexpected intent output type: ${output?.javaClass}")
                null
            }
        }
    }

    @Suppress("UNCHECKED_CAST")
    private fun extractSlotTags(output: Any?, tokens: List<String>): List<String> {
        val tags = mutableListOf<String>()
        try {
            when (output) {
                is Array<*> -> {
                    val slotLogits3D = output as Array<Array<FloatArray>>
                    val seqLogits = slotLogits3D[0] // batch=0

                    for (i in 1 until (tokens.size - 1).coerceAtMost(seqLogits.size)) {
                        if (tokens[i] == "[SEP]") break
                        val logitsAtI = seqLogits[i]
                        var predIdx = 0
                        var predMax = logitsAtI[0]
                        for (j in 1 until logitsAtI.size) {
                            if (logitsAtI[j] > predMax) {
                                predMax = logitsAtI[j]
                                predIdx = j
                            }
                        }
                        if (predIdx < slotLabels.size) {
                            tags.add(slotLabels[predIdx])
                        }
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to extract slot tags", e)
        }
        return tags
    }

    private fun extractSlots(chars: List<String>, tags: List<String>): List<IntentSlot> {
        val slots = mutableListOf<IntentSlot>()
        var currentSlotType = ""
        var currentValue = ""

        for (i in chars.indices) {
            if (i >= tags.size) break
            val tag = tags[i]
            val ch = chars[i]

            if (tag.startsWith("B-")) {
                if (currentSlotType.isNotEmpty()) {
                    slots.add(IntentSlot(currentSlotType, currentValue))
                }
                currentSlotType = tag.substring(2)
                currentValue = ch
            } else if (tag.startsWith("I-") && currentSlotType.isNotEmpty()) {
                if (tag.substring(2) == currentSlotType) {
                    currentValue += ch
                } else {
                    slots.add(IntentSlot(currentSlotType, currentValue))
                    currentSlotType = ""
                    currentValue = ""
                }
            } else {
                if (currentSlotType.isNotEmpty()) {
                    slots.add(IntentSlot(currentSlotType, currentValue))
                    currentSlotType = ""
                    currentValue = ""
                }
            }
        }

        if (currentSlotType.isNotEmpty()) {
            slots.add(IntentSlot(currentSlotType, currentValue))
        }
        return slots
    }
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
        "HIT: $intent (${(confidence * 100).toInt()}%) slots=${slots.size}"
    } else {
        "NO HIT: ${(confidence * 100).toInt()}% (raw: $rawIntent)"
    }
}
