package com.stdemo.ggufchat

import android.content.Context
import java.io.File

/**
 * 持久化存储助手
 *
 * 模型存储在应用内部存储中：
 * - LLM模型: /data/data/{pkg}/files/models/llm/
 * - 意图模型: /data/data/{pkg}/files/models/intend/
 *
 * 特点：
 * - 不需要额外存储权限
 * - 读写速度快（内部Flash）
 * - 卸载应用时会被删除
 */
object PersistentStorageHelper {

    /**
     * 获取LLM模型存储目录
     *
     * 路径: /data/data/{pkg}/files/models/llm
     */
    fun getLlmModelsDir(context: Context): File {
        return File(context.filesDir, "models/llm").also {
            if (!it.exists()) it.mkdirs()
        }
    }

    /**
     * 获取意图识别模型存储目录
     *
     * 路径: /data/data/{pkg}/files/models/intend
     */
    fun getIntentModelsDir(context: Context): File {
        return File(context.filesDir, "models/intend").also {
            if (!it.exists()) it.mkdirs()
        }
    }

    /**
     * 获取models根目录
     *
     * 路径: /data/data/{pkg}/files/models
     */
    fun getModelsRootDir(context: Context): File {
        return File(context.filesDir, "models").also {
            if (!it.exists()) it.mkdirs()
        }
    }

    /**
     * 获取存储信息（用于调试）
     */
    fun getStorageInfo(context: Context): String {
        return buildString {
            appendLine("=== 存储信息 ===")
            appendLine()

            // models根目录
            val modelsRoot = getModelsRootDir(context)
            appendLine("【Models 根目录】")
            appendLine("路径: ${modelsRoot.absolutePath}")
            appendLine("存在: ${modelsRoot.exists()}")
            appendLine()

            // LLM模型目录
            val llmDir = getLlmModelsDir(context)
            appendLine("【LLM 模型目录】")
            appendLine("路径: ${llmDir.absolutePath}")
            val llmFiles = llmDir.listFiles()
            if (llmFiles.isNullOrEmpty()) {
                appendLine("文件: (空)")
            } else {
                appendLine("文件: ${llmFiles.size} 个")
                llmFiles.forEach { file ->
                    appendLine("  - ${file.name} (${file.length() / (1024 * 1024)} MB)")
                }
            }
            appendLine()

            // 意图模型目录
            val intentDir = getIntentModelsDir(context)
            appendLine("【Intent 模型目录】")
            appendLine("路径: ${intentDir.absolutePath}")
            val intentFiles = intentDir.listFiles()
            if (intentFiles.isNullOrEmpty()) {
                appendLine("文件: (空)")
            } else {
                appendLine("文件: ${intentFiles.size} 个")
                intentFiles.forEach { file ->
                    appendLine("  - ${file.name} (${file.length() / 1024} KB)")
                }
            }
            appendLine()

            // 存储空间
            appendLine("【存储空间】")
            val usableSpace = context.filesDir.usableSpace / (1024 * 1024)
            val totalSpace = context.filesDir.totalSpace / (1024 * 1024)
            appendLine("可用: $usableSpace MB")
            appendLine("总计: $totalSpace MB")
        }
    }
}
