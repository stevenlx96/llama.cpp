package com.stdemo.ggufchat

import android.content.Context
import android.os.Build
import android.os.Environment
import java.io.File

/**
 * 持久化存储助手
 *
 * 解决问题：getExternalFilesDir() 的数据在卸载应用时会被删除
 *
 * 提供两种存储方案：
 * 1. 应用私有存储（卸载时删除）- 默认方案
 * 2. 公共存储（卸载时保留）- 推荐用于大型模型文件
 */
object PersistentStorageHelper {

    /**
     * 获取应用私有存储目录（卸载时删除）
     *
     * 路径示例: /storage/emulated/0/Android/data/com.stdemo.ggufchat/files/models
     *
     * 特点：
     * - ✅ 不需要额外权限
     * - ✅ 自动随应用管理
     * - ❌ 卸载应用时会被删除
     * - ❌ 清除数据时会被删除
     */
    fun getAppPrivateModelsDir(context: Context): File? {
        return context.getExternalFilesDir("models")
    }

    /**
     * 获取公共存储目录（卸载时保留）
     *
     * 路径示例: /storage/emulated/0/GGUFChat/models
     *
     * 特点：
     * - ✅ 卸载应用后文件保留
     * - ✅ 可以手动管理模型文件
     * - ✅ 多个应用可以共享模型
     * - ⚠️ 需要存储权限（Android 13以下）
     * - ⚠️ 需要手动清理
     */
    fun getPublicModelsDir(): File {
        val baseDir = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // Android 10+ 推荐使用公共目录
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)
        } else {
            // Android 9及以下
            Environment.getExternalStorageDirectory()
        }

        val modelsDir = File(baseDir, "GGUFChat/models")
        if (!modelsDir.exists()) {
            modelsDir.mkdirs()
        }

        return modelsDir
    }

    /**
     * 获取推荐的存储目录
     *
     * 优先使用公共存储（如果可用），否则使用应用私有存储
     */
    fun getRecommendedModelsDir(context: Context): File {
        return try {
            // 尝试使用公共存储
            val publicDir = getPublicModelsDir()
            if (publicDir.exists() && publicDir.canWrite()) {
                publicDir
            } else {
                // 回退到应用私有存储
                getAppPrivateModelsDir(context) ?: File(context.filesDir, "models")
            }
        } catch (e: Exception) {
            // 出错时使用应用私有存储
            getAppPrivateModelsDir(context) ?: File(context.filesDir, "models")
        }
    }

    /**
     * 检查公共存储是否可用
     */
    fun isPublicStorageAvailable(): Boolean {
        val state = Environment.getExternalStorageState()
        return Environment.MEDIA_MOUNTED == state
    }

    /**
     * 获取存储信息（用于调试）
     */
    fun getStorageInfo(context: Context): String {
        return buildString {
            appendLine("=== 存储方案对比 ===")
            appendLine()

            // 应用私有存储
            appendLine("【方案1: 应用私有存储】")
            val privateDir = getAppPrivateModelsDir(context)
            if (privateDir != null) {
                appendLine("路径: ${privateDir.absolutePath}")
                appendLine("存在: ${privateDir.exists()}")
                appendLine("可写: ${privateDir.canWrite()}")
                appendLine("优点: 不需要额外权限")
                appendLine("缺点: 卸载应用时会被删除")
            } else {
                appendLine("❌ 不可用")
            }
            appendLine()

            // 公共存储
            appendLine("【方案2: 公共存储（推荐）】")
            val publicDir = getPublicModelsDir()
            appendLine("路径: ${publicDir.absolutePath}")
            appendLine("存在: ${publicDir.exists()}")
            appendLine("可写: ${publicDir.canWrite()}")
            appendLine("可用: ${isPublicStorageAvailable()}")
            appendLine("优点: 卸载应用后文件保留")
            appendLine("缺点: 需要手动管理")
            appendLine()

            // 推荐方案
            appendLine("【当前推荐使用】")
            val recommended = getRecommendedModelsDir(context)
            appendLine("路径: ${recommended.absolutePath}")
            appendLine("存在: ${recommended.exists()}")
        }
    }

    /**
     * 迁移模型文件从私有存储到公共存储
     */
    fun migrateToPublicStorage(context: Context): Result<String> {
        return try {
            val privateDir = getAppPrivateModelsDir(context)
            val publicDir = getPublicModelsDir()

            if (privateDir == null || !privateDir.exists()) {
                return Result.failure(Exception("私有存储目录不存在"))
            }

            if (!publicDir.exists()) {
                publicDir.mkdirs()
            }

            val files = privateDir.listFiles { file ->
                file.isFile && file.name.endsWith(".gguf", ignoreCase = true)
            }

            if (files.isNullOrEmpty()) {
                return Result.failure(Exception("没有找到需要迁移的模型文件"))
            }

            var successCount = 0
            var failCount = 0

            files.forEach { file ->
                try {
                    val targetFile = File(publicDir, file.name)
                    file.copyTo(targetFile, overwrite = false)
                    successCount++
                } catch (e: Exception) {
                    failCount++
                }
            }

            val message = "迁移完成！成功: $successCount, 失败: $failCount\n" +
                          "新位置: ${publicDir.absolutePath}"

            Result.success(message)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }
}
