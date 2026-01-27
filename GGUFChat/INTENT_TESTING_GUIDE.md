# Intent Recognition Testing Guide

## 快速开始

### 方法 1：使用内置测试（推荐）

**双击状态栏**（显示 "Model ready" 的地方）即可运行测试！

测试会：
- ✅ 使用内置的测试用例
- ✅ 自动计算准确率
- ✅ 显示失败的案例
- ✅ 输出详细日志到 Logcat

查看 Logcat (tag: `IntentTester`) 可以看到每个测试的详细结果。

### 方法 2：代码中调用

```kotlin
import com.stdemo.ggufchat.IntentTester

// 使用内置测试用例
val testCases = IntentTester.getExampleTestCases()

// 运行测试
val result = IntentTester.runBatchTest(context, testCases)

// 查看结果
Log.i("Test", "Accuracy: ${(result.hitRate * 100).toInt()}%")
Log.i("Test", "Correct: ${result.correctIntents}/${result.totalTests}")
```

## 准备自己的测试数据

### 方法 1：在代码中定义

修改 `IntentTester.getExampleTestCases()`：

```kotlin
fun getMyTestCases(): List<TestCase> {
    return listOf(
        TestCase("今天天气怎么样", "weather-weather", "天气查询"),
        TestCase("你好", "chat-chat", "问候"),
        TestCase("播放音乐", "music-play", "音乐"),
        // ... 添加更多测试用例
    )
}
```

### 方法 2：使用 CSV 文件

1. **创建 CSV 文件** (`test_cases.csv`)：

```csv
text,expected_intent,description
今天北京天气怎么样,weather-weather,天气查询-今天
明天上海会下雨吗,weather-weather,天气查询-明天
你好,chat-chat,问候
讲个笑话,chat-chat,闲聊
播放周杰伦的歌,music-play,音乐播放
```

2. **在代码中加载**：

```kotlin
// 从 assets 加载
val csvContent = context.assets.open("test_cases.csv").bufferedReader().use { it.readText() }
val testCases = IntentTester.loadTestCasesFromCSV(csvContent)

// 运行测试
val result = IntentTester.runBatchTest(context, testCases)
```

### 方法 3：导出当前测试用例为 CSV

```kotlin
val testCases = IntentTester.getExampleTestCases()
val csv = IntentTester.exportTestCasesToCSV(testCases)

// 保存到文件或分享
File(context.filesDir, "test_cases.csv").writeText(csv)
```

## 测试结果说明

### 准确率 (Accuracy)

```
准确率 = 正确识别的数量 / 总测试数量
```

**示例输出**：
```
Accuracy: 85%
Total: 20
Correct: 17
Failed: 3
Avg Confidence: 92%
```

### 理解测试结果

#### ✅ 测试通过

```
✅ [1/20] PASS: "今天北京天气怎么样"
   Expected: weather-weather, Got: weather-weather (98%)
```

#### ❌ 测试失败的类型

**类型 1：意图识别错误**
```
❌ FAIL: "播放音乐"
   Expected: music-play, Got: chat-chat (75%)
```
→ 模型将音乐意图误识别为聊天

**类型 2：应该命中但未命中**
```
❌ FAIL: "今天天气"
   Expected: weather-weather, Got: NO_HIT (70%)
```
→ 置信度不够（70% < 85%）

**类型 3：不应该命中但命中了**
```
❌ FAIL: "？？？"
   Expected: NO_HIT, Got: chat-chat (88%)
```
→ 模型过于自信地分类了无意义输入

## 测试用例设计建议

### 1. 覆盖所有意图

为每个意图准备至少 **3-5 个测试用例**：

```
weather-weather:
  - "今天北京天气怎么样"  (完整句)
  - "明天会下雨吗"        (省略地点)
  - "查天气"             (简短)

chat-chat:
  - "你好"
  - "在吗"
  - "讲个笑话"
```

### 2. 包含边界情况

```kotlin
// 短输入
TestCase("天气", "weather-weather", "单词"),

// 长输入
TestCase("我想知道今天北京的天气怎么样温度多少度", "weather-weather", "长句"),

// 多意图混合
TestCase("今天天气怎么样，顺便播放首歌", "???", "多意图"),

// 无意义输入
TestCase("asdfjkl", "NO_HIT", "随机字符"),
```

### 3. 测试 chat-chat 回退

chat-chat 意图应该在配置中回退到 LLM：

```kotlin
// 这些应该被识别为 chat-chat，但在路由时会回退到 LLM
TestCase("你好", "chat-chat", "应该回退到LLM"),
TestCase("讲个笑话", "chat-chat", "应该回退到LLM"),
```

## 分析和改进

### 1. 查看混淆案例

找出最常见的误识别模式：

```
weather-weather → chat-chat: 3 次
music-play → chat-chat: 2 次
```

→ 说明模型容易将其他意图误判为 chat

### 2. 调整阈值

如果测试显示：
- 很多 **应该命中但未命中** → 降低阈值（如 0.75）
- 很多 **不应该命中但命中了** → 提高阈值（如 0.90）

在 `IntentConfig.kt` 中修改：

```kotlin
IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD = 0.80f
```

### 3. 收集真实数据

在实际使用中收集失败案例：

```kotlin
// 在 ChatViewModel 中记录
if (intentResult != null && !intentResult.hit) {
    Log.i("IntentData", "Low confidence: ${intentResult.text} -> ${intentResult.rawIntent} (${intentResult.confidence})")
}
```

将这些案例添加到测试数据中。

## 完整测试流程

### 日常测试流程

1. **准备测试数据** (50-100 个测试用例)
2. **运行测试** (双击状态栏或代码调用)
3. **查看结果** (目标: 准确率 > 85%)
4. **分析失败案例**
5. **调整模型/阈值**
6. **重新测试**

### 自动化测试（可选）

在 CI/CD 中运行：

```kotlin
// 在 AndroidTest 中
@Test
fun testIntentRecognition() {
    val testCases = IntentTester.loadTestCasesFromCSV(csvContent)
    val result = IntentTester.runBatchTest(context, testCases)

    // 断言准确率
    assertTrue("Accuracy should be >= 85%", result.hitRate >= 0.85f)
}
```

## 测试数据示例

完整的测试数据模板：

```csv
text,expected_intent,description
# Weather
今天北京天气怎么样,weather-weather,天气-完整
明天会下雨吗,weather-weather,天气-明天
深圳周末天气,weather-weather,天气-周末
查天气,weather-weather,天气-简短
# Chat
你好,chat-chat,问候
hi,chat-chat,英文问候
在吗,chat-chat,打招呼
讲个笑话,chat-chat,闲聊
你是谁,chat-chat,询问
# Music
播放周杰伦的歌,music-play,音乐-艺术家
我想听稻香,music-play,音乐-歌名
放首轻音乐,music-play,音乐-类型
# Boundary
今天,NO_HIT,单词-低置信度
？？？,NO_HIT,无意义
asdkfjaskdf,NO_HIT,随机字符
```

## 总结

✅ **快速测试**：双击状态栏

✅ **查看日志**：Logcat → `IntentTester`

✅ **准备数据**：CSV 或代码

✅ **目标准确率**：> 85%

✅ **持续改进**：收集真实数据，调整阈值
