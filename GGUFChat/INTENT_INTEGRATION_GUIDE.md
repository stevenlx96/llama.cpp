# GGUFChat 意图识别集成指南

## 概述

GGUFChat现在集成了ONNX意图识别功能，可以智能路由用户查询：
- **HIT** (置信度 ≥ 阈值): 执行特定handler (天气API、音乐控制等)
- **NO HIT** (置信度 < 阈值): 回退到LLaMA生成通用回复

## 工作流程

```
用户输入: "今天北京天气怎么样"
    ↓
IntentRecognizer.predict()
    ↓
confidence = 95% ≥ 60% (threshold)
    ↓
✅ HIT! intent = "weather-query"
slots = [time="今天", location="北京"]
    ↓
调用天气API获取数据
    ↓
返回: "今天北京的天气是晴，25°C"
```

```
用户输入: "帮我写一首诗"
    ↓
IntentRecognizer.predict()
    ↓
confidence = 35% < 60% (threshold)
    ↓
❌ NO HIT
    ↓
Fallback到LLaMA
    ↓
LLaMA生成诗歌
```

## 集成步骤

### 1. 准备ONNX Runtime库

下载ONNX Runtime Android AAR:
```bash
# 下载地址
https://github.com/microsoft/onnxruntime/releases

# 查找文件
onnxruntime-android-1.17.0.aar

# 解压并提取
unzip onnxruntime-android-1.17.0.aar
cp jni/arm64-v8a/libonnxruntime.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
```

### 2. 准备意图识别模型

将你的ONNX模型文件放到应用存储:
```
/sdcard/Documents/GGUFChat/models/intend/
├── joint_model_quantized.onnx
├── intent_label.txt
├── slot_label.txt
├── vocab.txt
└── android_config.json
```

### 3. 在ChatViewModel中集成

```kotlin
class ChatViewModel(application: Application) : AndroidViewModel(application) {

    private val intentRecognizer = IntentRecognizer()

    init {
        // 初始化意图识别器
        val intentModelDir = "${getApplication<Application>().filesDir}/models/intend"
        intentRecognizer.initialize(
            modelDir = intentModelDir,
            numThreads = 4,
            confidenceThreshold = 0.6f
        )
    }

    fun sendMessage(text: String) {
        viewModelScope.launch {
            // 先尝试意图识别
            val intentResult = withContext(Dispatchers.IO) {
                intentRecognizer.predict(text)
            }

            if (intentResult.hit) {
                // ✅ HIT: 执行特定handler
                handleIntent(intentResult)
            } else {
                // ❌ NO HIT: 使用LLaMA
                generateWithLLaMA(text)
            }
        }
    }

    private suspend fun handleIntent(result: IntentResult) {
        when {
            result.intent.contains("weather") -> {
                handleWeatherIntent(result.slots)
            }
            result.intent.contains("music") -> {
                handleMusicIntent(result.slots)
            }
            else -> {
                // 未知意图，回退到LLaMA
                generateWithLLaMA(result.text)
            }
        }
    }

    private suspend fun handleWeatherIntent(slots: List<IntentSlot>) {
        val location = slots.find { it.slotType == "location" }?.slotValue ?: "当前位置"
        val time = slots.find { it.slotType == "time" }?.slotValue ?: "今天"

        // TODO: 调用天气API
        val weatherInfo = fetchWeather(location, time)

        // 添加回复消息
        val response = "${time}${location}的天气是：${weatherInfo}"
        addMessage(Message(content = response, isUser = false))
    }

    private suspend fun handleMusicIntent(slots: List<IntentSlot>) {
        val artist = slots.find { it.slotType == "artist" }?.slotValue
        val song = slots.find { it.slotType == "song" }?.slotValue

        // TODO: 调用音乐播放API
        val response = when {
            artist != null -> "正在为您播放${artist}的歌曲..."
            song != null -> "正在为您播放《${song}》..."
            else -> "正在为您播放音乐..."
        }

        addMessage(Message(content = response, isUser = false))
    }

    private suspend fun generateWithLLaMA(text: String) {
        // 原有的LLaMA生成逻辑
        llamaEngine.generate(text) { token ->
            updateLastMessage(token)
        }
    }

    override fun onCleared() {
        super.onCleared()
        intentRecognizer.release()
    }
}
```

### 4. 在MainActivity中添加调试功能

```kotlin
class MainActivity : AppCompatActivity() {

    private fun setupIntentDebugButton() {
        binding.intentDebugButton.setOnClickListener {
            val text = binding.inputEditText.text.toString()
            if (text.isNotBlank()) {
                testIntent(text)
            }
        }
    }

    private fun testIntent(text: String) {
        lifecycleScope.launch {
            val result = viewModel.intentRecognizer.predict(text)

            val debugInfo = buildString {
                appendLine("输入: $text")
                appendLine("---")
                if (result.hit) {
                    appendLine("✅ HIT!")
                    appendLine("意图: ${result.intent}")
                    appendLine("置信度: ${(result.confidence * 100).toInt()}%")
                    if (result.slots.isNotEmpty()) {
                        appendLine("槽位:")
                        result.slots.forEach { slot ->
                            appendLine("  - ${slot.slotType}: ${slot.slotValue}")
                        }
                    }
                } else {
                    appendLine("❌ NO HIT")
                    appendLine("置信度: ${(result.confidence * 100).toInt()}%")
                    appendLine("原始意图: ${result.rawIntent}")
                }
            }

            Toast.makeText(this@MainActivity, debugInfo, Toast.LENGTH_LONG).show()
        }
    }
}
```

## 使用示例

### 基本使用

```kotlin
val recognizer = IntentRecognizer()

// 初始化
if (recognizer.initialize(modelDir = "/path/to/models/intend", threshold = 0.6f)) {

    // 预测
    val result = recognizer.predict("今天北京天气怎么样")

    // 处理结果
    result.routeOrFallback(
        onIntent = { intent, slots ->
            // 执行特定handler
            Log.d(TAG, "Intent: $intent")
            slots.forEach { slot ->
                Log.d(TAG, "  ${slot.slotType}: ${slot.slotValue}")
            }
        },
        onFallback = { text ->
            // 回退到LLM
            useLLM(text)
        }
    )

    // 释放资源
    recognizer.release()
}
```

### 动态调整阈值

```kotlin
// 开始时保守
recognizer.setThreshold(0.7f)

// 根据反馈调整
if (tooManyFallbacks) {
    recognizer.setThreshold(0.5f)  // 更宽松
} else if (tooManyFalsePositives) {
    recognizer.setThreshold(0.8f)  // 更严格
}
```

### 日志调试

```kotlin
val result = recognizer.predict(text)
Log.d(TAG, result.formatLog())

// 输出示例:
// ✅ HIT: weather-query (95%) slots=2
// 或
// ❌ NO HIT: 35% (raw: general-chat)
```

## 支持的意图示例

根据你的模型训练，可能支持的意图包括:

| 意图类别 | 示例查询 | 槽位 |
|---------|---------|------|
| weather-query | "今天北京天气怎么样" | time, location |
| music-play | "播放周杰伦的歌" | artist, song |
| alarm-set | "明天早上7点叫我" | time, action |
| news-query | "今天的新闻" | time, category |
| general-chat | "你好" | - |

## 阈值调优指南

| 阈值 | 命中率 | 适用场景 |
|------|--------|---------|
| 0.4-0.5 | 高 (~70%) | 探索性，想捕获更多意图 |
| 0.6-0.7 | 中 (~40%) | **推荐** - 平衡准确性和覆盖 |
| 0.8-0.9 | 低 (~20%) | 保守，只处理非常明确的意图 |

## 错误处理

```kotlin
try {
    val result = recognizer.predict(text)
    // 处理结果
} catch (e: Exception) {
    Log.e(TAG, "Intent prediction failed", e)
    // 直接回退到LLM
    useLLM(text)
}
```

## 性能考虑

- **初始化**: 首次加载模型约需1-2秒
- **推理**: 每次预测约50-150ms
- **内存**: 约100MB (量化模型)
- **建议**: 在Application onCreate时初始化，复用同一个实例

## 最佳实践

1. **单例模式**: 整个应用只创建一个IntentRecognizer实例
2. **后台线程**: 始终在Dispatchers.IO上执行predict()
3. **错误回退**: 意图识别失败时自动回退到LLM
4. **日志记录**: 记录所有预测结果用于分析优化
5. **A/B测试**: 测试不同阈值找到最优值

## 故障排除

### 1. ONNX Runtime未找到

**错误**: `Failed to load llama-android library`

**解决**:
```bash
# 确认库存在
ls -la GGUFChat/app/src/main/jniLibs/arm64-v8a/libonnxruntime.so

# 如果不存在，下载并解压ONNX Runtime AAR
```

### 2. 模型文件未找到

**错误**: `Failed to initialize intent recognizer`

**解决**:
```kotlin
// 确认模型目录包含所有必需文件
val files = File(modelDir).listFiles()
files?.forEach { file ->
    Log.d(TAG, "Found: ${file.name}")
}

// 应该看到:
// joint_model_quantized.onnx
// intent_label.txt
// slot_label.txt
// vocab.txt
// android_config.json
```

### 3. 推理速度慢

**优化**:
```kotlin
// 使用量化模型
config.model_file = "joint_model_quantized.onnx"

// 减少线程数（移动设备）
recognizer.initialize(modelDir, numThreads = 2)

// 减小max_seq_len (在android_config.json)
{"max_seq_len": 32}
```

## 扩展功能

### 添加新的意图handler

```kotlin
private suspend fun handleCustomIntent(result: IntentResult) {
    when (result.intent) {
        "shopping-query" -> handleShopping(result.slots)
        "booking-request" -> handleBooking(result.slots)
        "translation-request" -> handleTranslation(result.slots)
        else -> generateWithLLaMA(result.text)
    }
}
```

### 多模型支持

```kotlin
class MultiDomainRouter {
    private val generalRecognizer = IntentRecognizer()
    private val domainSpecificRecognizer = IntentRecognizer()

    fun route(text: String): IntentResult {
        // 先尝试领域专用模型
        val domainResult = domainSpecificRecognizer.predict(text)
        if (domainResult.hit) return domainResult

        // 回退到通用模型
        return generalRecognizer.predict(text)
    }
}
```

## 参考文档

- [完整API文档](../examples/intent-recognition/README.md)
- [阈值使用指南](../examples/intent-recognition/THRESHOLD_GUIDE.md)
- [模型准备脚本](../examples/intent-recognition/models/prepare_model.py)

## 许可证

Intent Recognition集成使用ONNX Runtime (MIT License)
