package com.stdemo.ggufchat

import android.content.Context
import android.util.Log

class ModelConfig(private val context: Context) {

    companion object {
        private const val TAG = "ModelConfig"
        private const val PREFERENCE_NAME = "llama_model_config"
        private const val KEY_MODEL_PATH = "model_path"
        private const val KEY_STREAMING_MODE = "streaming_mode"
        private const val KEY_MODEL_DIR = "model_dir"
    }

    private val preferences = context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)

    fun saveModelPath(modelPath: String) {
        preferences.edit().putString(KEY_MODEL_PATH, modelPath).apply()
        Log.d(TAG, "Model path saved: $modelPath")
    }

    fun getModelPath(): String? {
        return preferences.getString(KEY_MODEL_PATH, null)
    }

    fun saveStreamingMode(enabled: Boolean) {
        preferences.edit().putBoolean(KEY_STREAMING_MODE, enabled).apply()
        Log.d(TAG, "Streaming mode saved: $enabled")
    }

    fun isStreamingModeEnabled(): Boolean {
        return preferences.getBoolean(KEY_STREAMING_MODE, true)
    }

    fun setModelDirectory(dirPath: String) {
        preferences.edit().putString(KEY_MODEL_DIR, dirPath).apply()
        Log.d(TAG, "Model directory set: $dirPath")
    }

    fun getModelDirectory(): String? {
        return preferences.getString(KEY_MODEL_DIR, null)
    }

    fun clearAllSettings() {
        preferences.edit().clear().apply()
        Log.d(TAG, "All settings cleared")
    }
}

data class ModelInfo(
    val name: String,
    val modelId: String,
    val fileName: String,
    val description: String = "",
    val size: Long = 0
)

object ModelRegistry {
    private const val TAG = "ModelRegistry"

    val availableModels = listOf(
        ModelInfo(
            name = "Qwen2.5 1.5B Chat",
            modelId = "Qwen/Qwen2.5-1.5B-Instruct-GGUF",
            fileName = "qwen2.5-1.5b-instruct-q4_k_m.gguf",
            description = "Lightweight model optimized for chat",
            size = 1024L * 1024 * 1024 // ~1GB
        ),
        ModelInfo(
            name = "Qwen2.5 3B Chat",
            modelId = "Qwen/Qwen2.5-3B-Instruct-GGUF",
            fileName = "qwen2.5-3b-instruct-q4_k_m.gguf",
            description = "Balanced model with better reasoning",
            size = 1800L * 1024 * 1024 // ~1.8GB
        ),
        ModelInfo(
            name = "Qwen2 7B Chat",
            modelId = "Qwen/Qwen2-7B-Instruct-GGUF",
            fileName = "qwen2-7b-instruct-q4_k_m.gguf",
            description = "Larger model with enhanced capabilities",
            size = 4000L * 1024 * 1024 // ~4GB
        )
    )

    fun getModelById(modelId: String): ModelInfo? {
        return availableModels.find { it.modelId == modelId }.also {
            if (it == null) {
                Log.w(TAG, "Model not found: $modelId")
            }
        }
    }

    fun getModelByName(name: String): ModelInfo? {
        return availableModels.find { it.name == name }.also {
            if (it == null) {
                Log.w(TAG, "Model not found: $name")
            }
        }
    }
}