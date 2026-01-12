package com.stdemo.ggufchat

import android.content.Context
import android.util.Log
import java.io.File
import java.io.FileOutputStream

/**
 * Hexagon HTP Library Deployer
 *
 * This class attempts to make HTP libraries accessible to the Hexagon DSP
 * by copying them from the APK's jniLibs to locations that might be accessible
 * via FastRPC.
 */
object HexagonHtpDeployer {
    private const val TAG = "HexagonHtpDeployer"

    private val HTP_LIBRARIES = listOf(
        "libggml-htp-v73.so",
        "libggml-htp-v75.so",
        "libggml-htp-v79.so",
        "libggml-htp-v81.so"
    )

    /**
     * Deploy HTP libraries to various accessible locations
     *
     * Tries multiple strategies:
     * 1. App's private directory (/data/data/pkg/files/htp/)
     * 2. App's native library directory
     * 3. Sets LD_LIBRARY_PATH environment variable
     *
     * Returns true if deployment succeeded
     */
    fun deployHtpLibraries(context: Context): Boolean {
        Log.i(TAG, "========================================")
        Log.i(TAG, "Hexagon HTP Library Deployment")
        Log.i(TAG, "========================================")

        try {
            // Strategy 1: Deploy to app's private files directory
            val htpDir = File(context.filesDir, "htp")
            if (!htpDir.exists()) {
                htpDir.mkdirs()
                Log.i(TAG, "Created HTP directory: ${htpDir.absolutePath}")
            }

            var successCount = 0

            for (libName in HTP_LIBRARIES) {
                try {
                    // Check if library exists in APK
                    val sourceLib = File(context.applicationInfo.nativeLibraryDir, libName)
                    val destLib = File(htpDir, libName)

                    if (sourceLib.exists()) {
                        // Copy library to app's private directory
                        sourceLib.inputStream().use { input ->
                            FileOutputStream(destLib).use { output ->
                                input.copyTo(output)
                            }
                        }

                        // Set executable permission
                        destLib.setReadable(true, false)
                        destLib.setExecutable(true, false)

                        Log.i(TAG, "✓ Deployed $libName")
                        Log.i(TAG, "  Source: ${sourceLib.absolutePath}")
                        Log.i(TAG, "  Dest:   ${destLib.absolutePath}")
                        successCount++
                    } else {
                        Log.w(TAG, "⚠ Library not found in APK: $libName")
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "✗ Failed to deploy $libName", e)
                }
            }

            if (successCount > 0) {
                Log.i(TAG, "----------------------------------------")
                Log.i(TAG, "Deployed $successCount/${HTP_LIBRARIES.size} HTP libraries")
                Log.i(TAG, "Location: ${htpDir.absolutePath}")

                // Try to set LD_LIBRARY_PATH (may not work for DSP, but worth trying)
                try {
                    System.setProperty("java.library.path",
                        "${htpDir.absolutePath}:${System.getProperty("java.library.path")}")
                    Log.i(TAG, "Set java.library.path to include HTP directory")
                } catch (e: Exception) {
                    Log.w(TAG, "Could not set java.library.path", e)
                }

                // Log environment for debugging
                Log.i(TAG, "----------------------------------------")
                Log.i(TAG, "Environment information:")
                Log.i(TAG, "  App native lib dir: ${context.applicationInfo.nativeLibraryDir}")
                Log.i(TAG, "  HTP deployment dir: ${htpDir.absolutePath}")
                Log.i(TAG, "========================================")

                return true
            } else {
                Log.e(TAG, "❌ No HTP libraries could be deployed")
                return false
            }

        } catch (e: Exception) {
            Log.e(TAG, "❌ HTP library deployment failed", e)
            return false
        }
    }

    /**
     * Get the deployment directory path
     */
    fun getHtpDeploymentPath(context: Context): String {
        return File(context.filesDir, "htp").absolutePath
    }
}
