package com.stdemo.ggufchat

import android.content.Context
import android.util.Log
import java.io.File

/**
 * Diagnostic tool for intent recognition feature
 *
 * Usage in MainActivity or Application:
 * ```
 * IntentDiagnostic.checkStatus(this)
 * ```
 */
object IntentDiagnostic {
    private const val TAG = "IntentDiagnostic"

    /**
     * Check and log complete status of intent recognition feature
     */
    fun checkStatus(context: Context) {
        Log.i(TAG, "========================================")
        Log.i(TAG, "Intent Recognition Diagnostic")
        Log.i(TAG, "========================================")

        // 1. Check if ONNX Runtime was compiled in
        val isRuntimeAvailable = checkRuntimeAvailable()
        Log.i(TAG, "1. ONNX Runtime compiled: ${if (isRuntimeAvailable) "✅ YES" else "❌ NO"}")

        // 2. Check model directory
        val modelDir = IntentModelManager.getModelDir(context)
        val modelDirFile = File(modelDir)
        Log.i(TAG, "2. Model directory: $modelDir")
        Log.i(TAG, "   Directory exists: ${if (modelDirFile.exists()) "✅ YES" else "❌ NO"}")

        // 3. Check model files
        val modelInfo = IntentModelManager.getModelInfo(context)
        Log.i(TAG, "3. Model files (${modelInfo.size} files, ${String.format("%.2f", IntentModelManager.getModelSizeMB(context))} MB):")

        val requiredFiles = listOf(
            "joint_model_quantized.onnx",
            "intent_label.txt",
            "slot_label.txt",
            "vocab.txt",
            "android_config.json"
        )

        requiredFiles.forEach { filename ->
            val size = modelInfo[filename]
            if (size != null) {
                Log.i(TAG, "   ✅ $filename (${formatBytes(size)})")
            } else {
                Log.w(TAG, "   ❌ $filename (MISSING)")
            }
        }

        // 4. Overall status
        val isReady = isRuntimeAvailable && IntentModelManager.isModelInstalled(context)
        Log.i(TAG, "========================================")
        Log.i(TAG, "Status: ${if (isReady) "✅ READY" else "❌ NOT READY"}")

        if (!isRuntimeAvailable) {
            Log.w(TAG, "Action: ONNX Runtime not compiled in build")
            Log.w(TAG, "  - Check CMake log for 'ONNX Runtime found'")
            Log.w(TAG, "  - Ensure libonnxruntime.so is in jniLibs/arm64-v8a/")
        }

        if (!IntentModelManager.isModelInstalled(context)) {
            Log.w(TAG, "Action: Model files missing")
            Log.w(TAG, "  - Place model files in assets/models/intend/")
            Log.w(TAG, "  - Call IntentModelManager.copyModelsFromAssets()")
        }

        Log.i(TAG, "========================================")
    }

    /**
     * Check if ONNX Runtime is available by trying to create IntentRecognizer
     */
    private fun checkRuntimeAvailable(): Boolean {
        return try {
            // Try to load the library
            System.loadLibrary("llama-android")
            true
        } catch (e: UnsatisfiedLinkError) {
            false
        }
    }

    /**
     * Format bytes to human-readable size
     */
    private fun formatBytes(bytes: Long): String {
        return when {
            bytes < 1024 -> "$bytes B"
            bytes < 1024 * 1024 -> "${bytes / 1024} KB"
            else -> String.format("%.2f MB", bytes / (1024.0 * 1024.0))
        }
    }

    /**
     * Test intent recognition with a sample text
     */
    fun testPredict(context: Context, testText: String = "今天北京天气怎么样"): Boolean {
        Log.i(TAG, "========================================")
        Log.i(TAG, "Testing Intent Recognition")
        Log.i(TAG, "========================================")
        Log.i(TAG, "Test text: \"$testText\"")

        val recognizer = IntentRecognizer()
        val modelDir = IntentModelManager.getModelDir(context)

        val initialized = recognizer.initialize(modelDir, confidenceThreshold = 0.6f)
        Log.i(TAG, "Initialize: ${if (initialized) "✅ SUCCESS" else "❌ FAILED"}")

        if (!initialized) {
            Log.e(TAG, "Cannot test - initialization failed")
            return false
        }

        try {
            val result = recognizer.predict(testText)
            Log.i(TAG, "Result: ${result.formatLog()}")
            Log.i(TAG, "  - Text: ${result.text}")
            Log.i(TAG, "  - Hit: ${result.hit}")
            Log.i(TAG, "  - Intent: ${result.intent}")
            Log.i(TAG, "  - Confidence: ${(result.confidence * 100).toInt()}%")
            Log.i(TAG, "  - Raw Intent: ${result.rawIntent}")
            Log.i(TAG, "  - Slots: ${result.slots}")

            recognizer.release()
            Log.i(TAG, "========================================")
            return true

        } catch (e: Exception) {
            Log.e(TAG, "Test failed", e)
            recognizer.release()
            return false
        }
    }
}
