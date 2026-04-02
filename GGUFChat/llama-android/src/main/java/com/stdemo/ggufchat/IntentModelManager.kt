package com.stdemo.ggufchat

import android.content.Context
import android.util.Log
import java.io.File
import java.io.FileOutputStream

/**
 * Helper class to manage intent recognition model files
 *
 * This class helps copy model files from assets to internal storage,
 * which is required for ONNX Runtime to load them.
 */
object IntentModelManager {
    private const val TAG = "IntentModelManager"

    /**
     * Get the model directory path in internal storage
     *
     * @param context Application context
     * @return Path to model directory (e.g., /data/data/com.stdemo.ggufchat/files/models/intend)
     */
    fun getModelDir(context: Context): String {
        return "${context.filesDir}/models/intend"
    }

    /**
     * Check if model files exist in internal storage
     *
     * @param context Application context
     * @return true if all required files exist
     */
    fun isModelInstalled(context: Context): Boolean {
        val modelDir = File(getModelDir(context))
        if (!modelDir.exists()) {
            return false
        }

        val requiredFiles = listOf(
            "joint_model_quantized.onnx",
            "intent_label.txt",
            "slot_label.txt",
            "vocab.txt",
            "android_config.json"
        )

        return requiredFiles.all { filename ->
            File(modelDir, filename).exists()
        }
    }

    /**
     * Copy model files from assets to internal storage
     *
     * Usage:
     * ```
     * // Put model files in app/src/main/assets/models/intend/
     * IntentModelManager.copyModelsFromAssets(context)
     * ```
     *
     * @param context Application context
     * @param assetPath Path in assets folder (default: "models/intend")
     * @return true if copy succeeds, false otherwise
     */
    fun copyModelsFromAssets(
        context: Context,
        assetPath: String = "models/intend"
    ): Boolean {
        return try {
            val modelDir = File(getModelDir(context))
            modelDir.mkdirs()

            val assetManager = context.assets
            val files = assetManager.list(assetPath) ?: emptyArray()

            Log.d(TAG, "Copying ${files.size} files from assets/$assetPath")

            for (filename in files) {
                val inputStream = assetManager.open("$assetPath/$filename")
                val outFile = File(modelDir, filename)

                FileOutputStream(outFile).use { output ->
                    inputStream.copyTo(output)
                }

                Log.d(TAG, "Copied: $filename (${outFile.length()} bytes)")
            }

            Log.i(TAG, "✓ Model files copied successfully to: $modelDir")
            true

        } catch (e: Exception) {
            Log.e(TAG, "Failed to copy model files from assets", e)
            false
        }
    }

    /**
     * Import model files from external storage (e.g., Downloads folder)
     *
     * This is useful for updating models without rebuilding the APK.
     *
     * @param context Application context
     * @param sourceDir Source directory containing model files
     * @return true if import succeeds
     */
    fun importModels(context: Context, sourceDir: File): Boolean {
        return try {
            if (!sourceDir.exists() || !sourceDir.isDirectory) {
                Log.e(TAG, "Source directory does not exist: $sourceDir")
                return false
            }

            val modelDir = File(getModelDir(context))
            modelDir.mkdirs()

            val files = sourceDir.listFiles() ?: emptyArray()
            Log.d(TAG, "Importing ${files.size} files from: $sourceDir")

            for (file in files) {
                if (file.isFile) {
                    val destFile = File(modelDir, file.name)
                    file.copyTo(destFile, overwrite = true)
                    Log.d(TAG, "Imported: ${file.name} (${destFile.length()} bytes)")
                }
            }

            Log.i(TAG, "✓ Model files imported successfully")
            true

        } catch (e: Exception) {
            Log.e(TAG, "Failed to import model files", e)
            false
        }
    }

    /**
     * Delete all model files from internal storage
     *
     * @param context Application context
     * @return true if delete succeeds
     */
    fun deleteModels(context: Context): Boolean {
        return try {
            val modelDir = File(getModelDir(context))
            if (modelDir.exists()) {
                modelDir.deleteRecursively()
                Log.i(TAG, "Model files deleted")
                true
            } else {
                false
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to delete model files", e)
            false
        }
    }

    /**
     * Get model files info
     *
     * @param context Application context
     * @return Map of filename to file size in bytes
     */
    fun getModelInfo(context: Context): Map<String, Long> {
        val modelDir = File(getModelDir(context))
        if (!modelDir.exists()) {
            return emptyMap()
        }

        return modelDir.listFiles()?.associate { file ->
            file.name to file.length()
        } ?: emptyMap()
    }

    /**
     * Get total size of model files in MB
     *
     * @param context Application context
     * @return Total size in megabytes
     */
    fun getModelSizeMB(context: Context): Double {
        val totalBytes = getModelInfo(context).values.sum()
        return totalBytes / (1024.0 * 1024.0)
    }
}

/**
 * Extension function for easy usage in Activity/Fragment
 */
fun Context.getIntentModelDir(): String {
    return IntentModelManager.getModelDir(this)
}

/**
 * Extension function to check if models are ready
 */
fun Context.isIntentModelReady(): Boolean {
    return IntentModelManager.isModelInstalled(this)
}
