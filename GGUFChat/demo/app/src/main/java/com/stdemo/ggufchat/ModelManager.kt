package com.stdemo.ggufchat

import android.util.Log
import java.io.File

/**
 * Model Manager - 模型扫描、管理和验证
 *
 * 功能：
 * 1. 扫描指定目录的GGUF模型文件
 * 2. 获取模型列表和信息
 * 3. 验证模型文件
 * 4. 删除模型
 */
class ModelManager(private val modelsDir: String) {

    companion object {
        private const val TAG = "ModelManager"
        private const val GGUF_EXTENSION = ".gguf"
        private const val MIN_MODEL_SIZE_MB = 50  // 最小50MB
    }

    /**
     * 模型信息数据类
     */
    data class ModelInfo(
        val name: String,           // 文件名
        val path: String,           // 完整路径
        val sizeBytes: Long,        // 文件大小（字节）
        val sizeMB: Long,           // 文件大小（MB）
        val lastModified: Long,     // 最后修改时间
        val isValid: Boolean        // 是否有效
    )

    /**
     * 初始化目录
     */
    init {
        val dir = File(modelsDir)
        if (!dir.exists()) {
            dir.mkdirs()
            Log.d(TAG, "Created models directory: $modelsDir")
        }
    }

    /**
     * 扫描目录中的所有GGUF模型
     *
     * @return 模型列表，按最后修改时间倒序排列
     */
    fun scanModels(): List<ModelInfo> {
        return try {
            val dir = File(modelsDir)
            if (!dir.exists() || !dir.isDirectory) {
                Log.w(TAG, "Models directory not found: $modelsDir")
                return emptyList()
            }

            val models = mutableListOf<ModelInfo>()

            dir.listFiles { file ->
                file.isFile && file.name.endsWith(GGUF_EXTENSION, ignoreCase = true)
            }?.forEach { file ->
                val sizeBytes = file.length()
                val sizeMB = sizeBytes / (1024 * 1024)

                // 验证文件大小
                val isValid = sizeBytes > 0 && sizeMB >= MIN_MODEL_SIZE_MB

                val modelInfo = ModelInfo(
                    name = file.name,
                    path = file.absolutePath,
                    sizeBytes = sizeBytes,
                    sizeMB = sizeMB,
                    lastModified = file.lastModified(),
                    isValid = isValid
                )

                models.add(modelInfo)
                Log.d(TAG, "Found model: ${file.name} (${sizeMB}MB, valid=$isValid)")
            }

            models.sortedByDescending { it.lastModified }
        } catch (e: Exception) {
            Log.e(TAG, "Error scanning models", e)
            emptyList()
        }
    }

    /**
     * 获取第一个有效的模型（用于自动加载）
     *
     * @return 第一个有效模型，如果没有返回null
     */
    fun getFirstValidModel(): ModelInfo? {
        return scanModels().firstOrNull { it.isValid }
    }

    /**
     * 按名称获取模型
     *
     * @param modelName 模型文件名
     * @return 模型信息或null
     */
    fun getModel(modelName: String): ModelInfo? {
        return scanModels().find { it.name == modelName }
    }

    /**
     * 验证模型文件是否存在且有效
     *
     * @param path 模型文件路径
     * @return true如果文件存在且有效
     */
    fun validateModel(path: String): Boolean {
        return try {
            val file = File(path)
            val isValid = file.exists() && file.length() > 0
            Log.d(TAG, "Validation for $path: $isValid")
            isValid
        } catch (e: Exception) {
            Log.e(TAG, "Error validating model", e)
            false
        }
    }

    /**
     * 删除模型文件
     *
     * @param path 模型文件路径
     * @return true如果删除成功
     */
    fun deleteModel(path: String): Boolean {
        return try {
            val file = File(path)
            if (file.exists()) {
                val deleted = file.delete()
                if (deleted) {
                    Log.d(TAG, "Deleted model: ${file.name}")
                } else {
                    Log.w(TAG, "Failed to delete model: ${file.name}")
                }
                deleted
            } else {
                Log.w(TAG, "Model file not found: $path")
                false
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error deleting model", e)
            false
        }
    }

    /**
     * 获取模型目录中的总大小
     *
     * @return 总大小（字节）
     */
    fun getTotalSize(): Long {
        return scanModels().sumOf { it.sizeBytes }
    }

    /**
     * 获取模型数量
     *
     * @return 模型总数
     */
    fun getModelCount(): Int {
        return scanModels().size
    }

    /**
     * 获取有效模型的数量
     *
     * @return 有效模型数
     */
    fun getValidModelCount(): Int {
        return scanModels().count { it.isValid }
    }

    /**
     * 格式化文件大小为可读字符串
     *
     * @param bytes 字节数
     * @return 格式化后的字符串
     */
    fun formatSize(bytes: Long): String {
        return when {
            bytes >= 1024 * 1024 * 1024 -> "${bytes / (1024 * 1024 * 1024)} GB"
            bytes >= 1024 * 1024 -> "${bytes / (1024 * 1024)} MB"
            bytes >= 1024 -> "${bytes / 1024} KB"
            else -> "$bytes B"
        }
    }

    /**
     * 获取模型列表的字符串表示（用于调试）
     *
     * @return 格式化后的模型列表字符串
     */
    fun getModelsDescription(): String {
        val models = scanModels()
        if (models.isEmpty()) {
            return "No models found"
        }

        return buildString {
            append("Found ${models.size} models:\n")
            models.forEachIndexed { index, model ->
                append("${index + 1}. ${model.name}\n")
                append("   Size: ${model.sizeMB} MB\n")
                append("   Status: ${if (model.isValid) "✓ Valid" else "✗ Invalid"}\n")
                append("   Path: ${model.path}\n")
            }
            append("\nTotal size: ${formatSize(getTotalSize())}")
        }
    }

    /**
     * 获取详细的模型信息
     *
     * @param modelPath 模型路径
     * @return 格式化后的详细信息
     */
    fun getDetailedInfo(modelPath: String): String {
        val file = File(modelPath)
        return buildString {
            append("========== Model Details ==========\n")
            append("Name: ${file.name}\n")
            append("Size: ${file.length() / (1024 * 1024)} MB\n")
            append("Path: $modelPath\n")
            append("Exists: ${if (file.exists()) "✓" else "✗"}\n")
            append("Modified: ${java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss").format(file.lastModified())}\n")
            append("==================================")
        }
    }
}