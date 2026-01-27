package com.stdemo.ggufchat

import android.content.Context
import android.util.Log

/**
 * Intent Recognition Testing Tool
 *
 * 用于批量测试意图识别的准确率
 */
object IntentTester {
    private const val TAG = "IntentTester"

    /**
     * 测试用例
     */
    data class TestCase(
        val text: String,
        val expectedIntent: String,
        val description: String = ""
    )

    /**
     * 测试结果
     */
    data class TestResult(
        val totalTests: Int,
        val correctIntents: Int,
        val hitRate: Float,
        val avgConfidence: Float,
        val failedCases: List<FailedCase>
    )

    data class FailedCase(
        val text: String,
        val expected: String,
        val actual: String,
        val confidence: Float,
        val wasHit: Boolean
    )

    /**
     * 运行批量测试
     *
     * @param context Application context
     * @param testCases 测试用例列表
     * @return 测试结果
     */
    fun runBatchTest(context: Context, testCases: List<TestCase>): TestResult {
        Log.i(TAG, "========================================")
        Log.i(TAG, "Starting Intent Recognition Batch Test")
        Log.i(TAG, "Total test cases: ${testCases.size}")
        Log.i(TAG, "========================================")

        if (!IntentRecognizerManager.isReady()) {
            IntentRecognizerManager.initialize(context)
        }

        var correctCount = 0
        var totalConfidence = 0f
        val failedCases = mutableListOf<FailedCase>()

        testCases.forEachIndexed { index, testCase ->
            val result = IntentRecognizerManager.predict(testCase.text)

            if (result != null) {
                val actualIntent = if (result.hit) result.intent else "NO_HIT"
                val isCorrect = actualIntent.equals(testCase.expectedIntent, ignoreCase = true)

                totalConfidence += result.confidence

                if (isCorrect) {
                    correctCount++
                    Log.i(TAG, "✅ [${index + 1}/${testCases.size}] PASS: \"${testCase.text}\"")
                    Log.i(TAG, "   Expected: ${testCase.expectedIntent}, Got: $actualIntent (${(result.confidence * 100).toInt()}%)")
                } else {
                    failedCases.add(
                        FailedCase(
                            text = testCase.text,
                            expected = testCase.expectedIntent,
                            actual = actualIntent,
                            confidence = result.confidence,
                            wasHit = result.hit
                        )
                    )
                    Log.w(TAG, "❌ [${index + 1}/${testCases.size}] FAIL: \"${testCase.text}\"")
                    Log.w(TAG, "   Expected: ${testCase.expectedIntent}, Got: $actualIntent (${(result.confidence * 100).toInt()}%)")
                }
            } else {
                Log.e(TAG, "❌ [${index + 1}/${testCases.size}] ERROR: Could not get prediction for \"${testCase.text}\"")
                failedCases.add(
                    FailedCase(
                        text = testCase.text,
                        expected = testCase.expectedIntent,
                        actual = "ERROR",
                        confidence = 0f,
                        wasHit = false
                    )
                )
            }
        }

        val avgConfidence = if (testCases.isNotEmpty()) totalConfidence / testCases.size else 0f
        val accuracy = if (testCases.isNotEmpty()) correctCount.toFloat() / testCases.size else 0f

        Log.i(TAG, "========================================")
        Log.i(TAG, "Test Results:")
        Log.i(TAG, "  Total: ${testCases.size}")
        Log.i(TAG, "  Correct: $correctCount")
        Log.i(TAG, "  Failed: ${failedCases.size}")
        Log.i(TAG, "  Accuracy: ${(accuracy * 100).toInt()}%")
        Log.i(TAG, "  Avg Confidence: ${(avgConfidence * 100).toInt()}%")
        Log.i(TAG, "========================================")

        return TestResult(
            totalTests = testCases.size,
            correctIntents = correctCount,
            hitRate = accuracy,
            avgConfidence = avgConfidence,
            failedCases = failedCases
        )
    }

    /**
     * 生成测试报告
     */
    fun generateReport(result: TestResult): String {
        return buildString {
            appendLine("# Intent Recognition Test Report")
            appendLine()
            appendLine("## Summary")
            appendLine("- Total Tests: ${result.totalTests}")
            appendLine("- Correct: ${result.correctIntents}")
            appendLine("- Failed: ${result.failedCases.size}")
            appendLine("- **Accuracy: ${(result.hitRate * 100).toInt()}%**")
            appendLine("- Average Confidence: ${(result.avgConfidence * 100).toInt()}%")
            appendLine()

            if (result.failedCases.isNotEmpty()) {
                appendLine("## Failed Cases (${result.failedCases.size})")
                appendLine()
                result.failedCases.forEachIndexed { index, case ->
                    appendLine("### ${index + 1}. \"${case.text}\"")
                    appendLine("- Expected: `${case.expected}`")
                    appendLine("- Actual: `${case.actual}`")
                    appendLine("- Confidence: ${(case.confidence * 100).toInt()}%")
                    appendLine("- Hit: ${if (case.wasHit) "Yes" else "No"}")
                    appendLine()
                }
            }
        }
    }

    /**
     * 示例测试数据集
     *
     * 你可以根据自己的意图标签修改这些测试用例
     */
    fun getExampleTestCases(): List<TestCase> {
        return listOf(
            // Chat-chat 意图（应该回退到 LLM）
            TestCase("你好", "chat-chat", "问候"),
            TestCase("hi", "chat-chat", "英文问候"),
            TestCase("在吗", "chat-chat", "打招呼"),
            TestCase("讲个笑话", "chat-chat", "闲聊请求"),
            TestCase("你是谁", "chat-chat", "询问身份"),

            // Weather 意图
            TestCase("今天北京天气怎么样", "weather-weather", "天气查询-今天"),
            TestCase("明天上海会下雨吗", "weather-weather", "天气查询-明天"),
            TestCase("深圳这周末天气如何", "weather-weather", "天气查询-周末"),
            TestCase("查一下杭州的天气", "weather-weather", "天气查询-简单"),

            // Music 意图（如果你有的话）
            TestCase("播放周杰伦的歌", "music-play", "音乐播放-艺术家"),
            TestCase("我想听稻香", "music-play", "音乐播放-歌名"),
            TestCase("放首轻音乐", "music-play", "音乐播放-类型"),

            // 边界情况
            TestCase("今天", "NO_HIT", "单词-低置信度"),
            TestCase("？？？", "NO_HIT", "无意义输入"),
            TestCase("asdkfjaskdf", "NO_HIT", "随机字符"),
        )
    }

    /**
     * 从文件加载测试用例（CSV格式）
     *
     * 格式: text,expected_intent,description
     */
    fun loadTestCasesFromCSV(csvContent: String): List<TestCase> {
        val lines = csvContent.lines().filter { it.isNotBlank() }
        if (lines.isEmpty()) return emptyList()

        return lines.drop(1).mapNotNull { line ->
            val parts = line.split(",").map { it.trim() }
            if (parts.size >= 2) {
                TestCase(
                    text = parts[0],
                    expectedIntent = parts[1],
                    description = parts.getOrNull(2) ?: ""
                )
            } else {
                null
            }
        }
    }

    /**
     * 导出测试用例为 CSV 格式
     */
    fun exportTestCasesToCSV(testCases: List<TestCase>): String {
        return buildString {
            appendLine("text,expected_intent,description")
            testCases.forEach { case ->
                appendLine("\"${case.text}\",${case.expectedIntent},\"${case.description}\"")
            }
        }
    }
}
