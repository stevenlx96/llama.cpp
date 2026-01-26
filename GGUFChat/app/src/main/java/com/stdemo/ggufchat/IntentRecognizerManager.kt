package com.stdemo.ggufchat

import android.content.Context
import android.util.Log

/**
 * Intent Recognizer Manager (Singleton)
 *
 * Manages the lifecycle of IntentRecognizer across the app.
 * Initialize once in Application.onCreate() or MainActivity.onCreate()
 */
object IntentRecognizerManager {
    private const val TAG = "IntentRecognizerMgr"

    private var recognizer: IntentRecognizer? = null
    private var isInitialized = false

    /**
     * Initialize the intent recognizer
     *
     * Call this once in Application.onCreate() or MainActivity.onCreate()
     *
     * @param context Application context
     * @param confidenceThreshold Confidence threshold (default: 0.6)
     * @return true if initialization succeeds
     */
    fun initialize(context: Context, confidenceThreshold: Float = 0.6f): Boolean {
        if (isInitialized && recognizer != null) {
            Log.i(TAG, "Already initialized")
            return true
        }

        try {
            val modelDir = IntentModelManager.getModelDir(context)

            // Check if models are installed
            if (!IntentModelManager.isModelInstalled(context)) {
                Log.w(TAG, "Intent models not installed, skipping initialization")
                return false
            }

            recognizer = IntentRecognizer()
            isInitialized = recognizer!!.initialize(modelDir, confidenceThreshold = confidenceThreshold)

            if (isInitialized) {
                Log.i(TAG, "✅ Intent recognizer initialized successfully")
            } else {
                Log.w(TAG, "❌ Intent recognizer initialization failed")
                recognizer = null
            }

            return isInitialized
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize intent recognizer", e)
            recognizer = null
            isInitialized = false
            return false
        }
    }

    /**
     * Predict intent for user input
     *
     * @param text User input text
     * @return IntentResult or null if not initialized
     */
    fun predict(text: String): IntentResult? {
        if (!isInitialized || recognizer == null) {
            return null
        }

        return try {
            recognizer!!.predict(text)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to predict intent", e)
            null
        }
    }

    /**
     * Check if initialized and ready
     */
    fun isReady(): Boolean = isInitialized && recognizer != null

    /**
     * Set confidence threshold
     */
    fun setThreshold(threshold: Float) {
        recognizer?.setThreshold(threshold)
    }

    /**
     * Get current threshold
     */
    fun getThreshold(): Float {
        return recognizer?.getThreshold() ?: 0.6f
    }

    /**
     * Release resources
     */
    fun release() {
        recognizer?.release()
        recognizer = null
        isInitialized = false
        Log.i(TAG, "Intent recognizer released")
    }
}
