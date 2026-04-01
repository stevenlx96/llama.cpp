package com.stdemo.ggufchat

import android.content.Context
import android.util.Log
import java.io.File

/**
 * Diagnostic tool for intent recognition feature
 */
object IntentDiagnostic {
    private const val TAG = "IntentDiagnostic"

    fun checkStatus(context: Context) {
        Log.i(TAG, "========================================")
        Log.i(TAG, "Intent Recognition Diagnostic")
        Log.i(TAG, "========================================")

        // 1. Check if ONNX Runtime Java API is available
        val isRuntimeAvailable = checkRuntimeAvailable()
        Log.i(TAG, "1. ONNX Runtime (Java API): ${if (isRuntimeAvailable) "YES" else "NO"}")

        // 2. Check model directory
        val modelDir = IntentModelManager.getModelDir(context)
        val modelDirFile = File(modelDir)
        Log.i(TAG, "2. Model directory: $modelDir")
        Log.i(TAG, "   Directory exists: ${if (modelDirFile.exists()) "YES" else "NO"}")

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
                Log.i(TAG, "   OK $filename (${formatBytes(size)})")
            } else {
                Log.w(TAG, "   MISSING $filename")
            }
        }

        // 4. Overall status
        val isReady = isRuntimeAvailable && IntentModelManager.isModelInstalled(context)
        Log.i(TAG, "========================================")
        Log.i(TAG, "Status: ${if (isReady) "READY" else "NOT READY"}")

        if (!isRuntimeAvailable) {
            Log.w(TAG, "Action: ONNX Runtime not available")
            Log.w(TAG, "  - Check build.gradle: implementation 'com.microsoft.onnxruntime:onnxruntime-android:...'")
        }

        if (!IntentModelManager.isModelInstalled(context)) {
            Log.w(TAG, "Action: Model files missing")
            Log.w(TAG, "  - Place model files in assets/models/intend/")
            Log.w(TAG, "  - Call IntentModelManager.copyModelsFromAssets()")
        }

        Log.i(TAG, "========================================")
    }

    private fun checkRuntimeAvailable(): Boolean {
        return try {
            Class.forName("ai.onnxruntime.OrtEnvironment")
            true
        } catch (e: ClassNotFoundException) {
            false
        }
    }

    private fun formatBytes(bytes: Long): String {
        return when {
            bytes < 1024 -> "$bytes B"
            bytes < 1024 * 1024 -> "${bytes / 1024} KB"
            else -> String.format("%.2f MB", bytes / (1024.0 * 1024.0))
        }
    }

    fun testPredict(context: Context, testText: String = "今天北京天气怎么样"): Boolean {
        Log.i(TAG, "========================================")
        Log.i(TAG, "Testing Intent Recognition")
        Log.i(TAG, "========================================")
        Log.i(TAG, "Test text: \"$testText\"")

        val recognizer = IntentRecognizer()
        val modelDir = IntentModelManager.getModelDir(context)

        val initialized = recognizer.initialize(modelDir)
        Log.i(TAG, "Initialize: ${if (initialized) "SUCCESS" else "FAILED"}")

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
