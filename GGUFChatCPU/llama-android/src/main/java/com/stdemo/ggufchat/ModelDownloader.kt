package com.stdemo.ggufchat

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.InputStream
import java.io.OutputStream
import java.net.URL

/**
 * Model Downloader - 从ModelScope下载模型文件
 *
 * 功能：
 * 1. 从ModelScope下载GGUF模型
 * 2. 跟踪下载进度
 * 3. 支持用户自定义ModelScope ID和文件名
 */
class ModelDownloader {

    companion object {
        private const val TAG = "ModelDownloader"
        private const val CONNECT_TIMEOUT = 60000      // 60秒连接超时
        private const val READ_TIMEOUT = 300000        // 5分钟读取超时
        private const val BUFFER_SIZE = 65536          // 64KB缓冲区
        private const val MODELSCOPE_BASE_URL = "https://www.modelscope.cn/models"
    }

    /**
     * 下载进度监听接口
     */
    interface DownloadProgressListener {
        /**
         * 下载进度回调
         *
         * @param percentage 下载百分比 (0-100)
         * @param downloadedBytes 已下载字节数
         * @param totalBytes 总字节数
         */
        fun onProgress(percentage: Int, downloadedBytes: Long, totalBytes: Long)

        /**
         * 下载成功回调
         *
         * @param filePath 下载后的文件路径
         */
        fun onSuccess(filePath: String)

        /**
         * 下载失败回调
         *
         * @param message 错误信息
         */
        fun onError(message: String)
    }

    /**
     * 构建ModelScope下载URL
     *
     * @param modelScopeId ModelScope模型ID (如 "Qwen/Qwen2.5-1.5B-Instruct-GGUF")
     * @param fileName 文件名 (如 "qwen2.5-1.5b-instruct-q4_k_m.gguf")
     * @return 完整的下载URL
     */
    fun buildDownloadUrl(modelScopeId: String, fileName: String): String {
        return "$MODELSCOPE_BASE_URL/$modelScopeId/resolve/master/$fileName"
    }

    /**
     * 从ModelScope下载模型
     *
     * @param modelScopeId ModelScope模型ID
     * @param fileName 文件名
     * @param downloadDir 下载目录
     * @param listener 进度监听器（可选）
     * @return Result with file path on success
     */
    suspend fun downloadModel(
        modelScopeId: String,
        fileName: String,
        downloadDir: String,
        listener: DownloadProgressListener? = null
    ): Result<String> = withContext(Dispatchers.IO) {
        try {
            Log.d(TAG, "Download request: modelScopeId=$modelScopeId, fileName=$fileName")

            if (modelScopeId.isBlank() || fileName.isBlank()) {
                val errorMsg = "ModelScope ID and filename cannot be empty"
                listener?.onError(errorMsg)
                return@withContext Result.failure(Exception(errorMsg))
            }

            val downloadUrl = buildDownloadUrl(modelScopeId, fileName)
            Log.d(TAG, "Download URL: $downloadUrl")

            val outputFile = File(downloadDir, fileName)
            if (outputFile.parentFile?.exists() == false) {
                outputFile.parentFile?.mkdirs()
                Log.d(TAG, "Created download directory: ${outputFile.parentFile?.absolutePath}")
            }

            downloadFile(downloadUrl, outputFile, listener)

            if (!outputFile.exists() || outputFile.length() == 0L) {
                val errorMsg = "Download failed: file is empty"
                listener?.onError(errorMsg)
                return@withContext Result.failure(Exception(errorMsg))
            }

            Log.d(TAG, "Download completed: ${outputFile.absolutePath} (${outputFile.length() / (1024 * 1024)} MB)")
            listener?.onSuccess(outputFile.absolutePath)
            Result.success(outputFile.absolutePath)
        } catch (e: Exception) {
            Log.e(TAG, "Download failed", e)
            listener?.onError("Download failed: ${e.message}")
            Result.failure(e)
        }
    }

    /**
     * 下载文件并跟踪进度
     *
     * @param urlString 下载URL
     * @param outputFile 输出文件
     * @param listener 进度监听器
     */
    private suspend fun downloadFile(
        urlString: String,
        outputFile: File,
        listener: DownloadProgressListener?
    ) = withContext(Dispatchers.IO) {
        var inputStream: InputStream? = null
        var outputStream: OutputStream? = null

        try {
            val url = URL(urlString)
            val connection = url.openConnection()
            connection.connectTimeout = CONNECT_TIMEOUT
            connection.readTimeout = READ_TIMEOUT

            val totalBytes = connection.contentLength.toLong()
            if (totalBytes <= 0) {
                throw Exception("Unable to get file size from server")
            }

            Log.d(TAG, "Total bytes to download: ${totalBytes / (1024 * 1024)} MB")

            inputStream = connection.getInputStream()
            outputStream = outputFile.outputStream()

            val buffer = ByteArray(BUFFER_SIZE)
            var bytesRead: Int
            var downloadedBytes = 0L
            var lastUpdateTime = System.currentTimeMillis()

            while (inputStream!!.read(buffer).also { bytesRead = it } != -1) {
                outputStream!!.write(buffer, 0, bytesRead)
                downloadedBytes += bytesRead

                val currentTime = System.currentTimeMillis()
                // 每500ms或下载完成时更新一次进度
                if (currentTime - lastUpdateTime > 500 || downloadedBytes == totalBytes) {
                    val percentage = ((downloadedBytes * 100) / totalBytes).toInt()
                    listener?.onProgress(percentage, downloadedBytes, totalBytes)
                    Log.d(TAG, "Download progress: $percentage% (${downloadedBytes / (1024 * 1024)} / ${totalBytes / (1024 * 1024)} MB)")
                    lastUpdateTime = currentTime
                }
            }

            // 确保发送最后的100%更新
            listener?.onProgress(100, downloadedBytes, totalBytes)
            Log.d(TAG, "Download 100% complete")

        } finally {
            try {
                inputStream?.close()
                outputStream?.close()
                Log.d(TAG, "Closed download streams")
            } catch (e: Exception) {
                Log.w(TAG, "Failed to close streams", e)
            }
        }
    }
}