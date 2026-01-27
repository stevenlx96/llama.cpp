# Intent Recognition Threshold Guide

This guide explains how to use the confidence threshold feature to route between intent handling and LLM fallback.

## Overview

The intent recognizer uses a **confidence threshold** to determine whether to execute a specific intent handler or fallback to general LLM inference:

- **HIT** (`confidence >= threshold`): Execute intent-specific handler
- **NO HIT** (`confidence < threshold`): Fallback to LLM for general response

```
┌─────────────┐
│ User Input  │
└──────┬──────┘
       │
       v
┌──────────────────┐
│ Intent Recognizer│
│  (with threshold)│
└──────┬───────────┘
       │
       ├─── confidence >= threshold ───→ ✅ HIT!  ───→ Execute Intent Handler
       │                                              (Weather API, Music API, etc.)
       │
       └─── confidence < threshold  ───→ ❌ NO HIT ───→ Fallback to LLaMA
                                                        (General conversation)
```

## Why Use Threshold?

1. **Avoid false positives**: Don't execute weather API for "帮我写一首诗"
2. **User experience**: Specific intents get fast, accurate responses
3. **Cost efficiency**: Save LLM costs for queries that can be handled by APIs
4. **Graceful fallback**: Unknown queries still get handled by LLM

## Configuration

### Default Threshold

The default threshold is **0.6 (60%)**, which provides a good balance:

- Weather queries: ~95% confidence → HIT
- Music commands: ~90% confidence → HIT
- Random chat: ~30% confidence → NO HIT

### Adjusting Threshold

**Lower threshold** (e.g., 0.4):
- ✅ More sensitive - catches more intents
- ❌ More false positives - may misclassify chat as intent

**Higher threshold** (e.g., 0.8):
- ✅ More precise - fewer false positives
- ❌ Less sensitive - may miss valid intents

## C++ Usage

### Basic Example

```cpp
#include "intent_recognizer.h"

using namespace intent;

int main() {
    // Initialize with threshold
    IntentConfig config;
    config.model_dir = "./data/file/models/intend";
    config.confidence_threshold = 0.6f;  // 60% threshold

    IntentRecognizer recognizer(config);
    recognizer.initialize();

    // Predict
    auto result = recognizer.predict("今天北京天气怎么样");

    if (result.hit) {
        // HIT: Execute intent handler
        std::cout << "✅ Intent: " << result.intent << std::endl;
        handle_weather(result.slots);
    } else {
        // NO HIT: Fallback to LLM
        std::cout << "❌ Fallback to LLM" << std::endl;
        use_llama(result.text);
    }

    return 0;
}
```

### Dynamic Threshold Adjustment

```cpp
IntentRecognizer recognizer(config);
recognizer.initialize();

// Start with conservative threshold
recognizer.set_threshold(0.7f);

// Process inputs
for (const auto& text : inputs) {
    auto result = recognizer.predict(text);

    if (result.hit) {
        handle_intent(result);
    } else {
        // Lower threshold for this user if needed
        if (too_many_fallbacks) {
            recognizer.set_threshold(0.5f);
        }
        use_llama(text);
    }
}
```

### Command-Line Options

```bash
# Default threshold (0.6)
./intent-recognition

# Custom threshold
./intent-recognition --threshold 0.8

# Single prediction
./intent-recognition --text "今天北京天气怎么样" --threshold 0.7

# Interactive mode with threshold
./intent-recognition --interactive --threshold 0.6

# Integration demo
./intent-recognition --demo
```

### Interactive Commands

In interactive mode:

```bash
$ ./intent-recognition --interactive

>>> threshold 0.8
Threshold updated to: 80%

>>> debug
Debug mode: ON

>>> 今天北京天气怎么样
✅ HIT! Intent: weather-query
...

>>> 帮我写一首诗
❌ NO HIT (confidence: 45% < threshold)
[Debug] Best guess was: general-chat
→ Fallback to LLaMA inference
```

## Android/Kotlin Usage

### Basic Example

```kotlin
import com.llama.cpp.IntentRecognizer
import com.llama.cpp.routeOrFallback

class ChatActivity : AppCompatActivity() {
    private lateinit var recognizer: IntentRecognizer

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Initialize with threshold
        recognizer = IntentRecognizer()
        recognizer.initialize(
            modelDir = "${filesDir}/models/intend",
            numThreads = 4,
            confidenceThreshold = 0.6f  // 60% threshold
        )
    }

    fun handleUserInput(text: String) {
        lifecycleScope.launch(Dispatchers.IO) {
            val result = recognizer.predict(text)

            withContext(Dispatchers.Main) {
                // Route based on hit
                result.routeOrFallback(
                    onIntent = { intent, slots ->
                        // ✅ HIT: Execute intent handler
                        when {
                            intent.contains("weather") -> handleWeather(slots)
                            intent.contains("music") -> handleMusic(slots)
                            else -> useLLM(text)
                        }
                    },
                    onFallback = { fallbackText ->
                        // ❌ NO HIT: Fallback to LLM
                        useLLM(fallbackText)
                    }
                )
            }
        }
    }

    private fun handleWeather(slots: List<IntentSlot>) {
        // Call weather API
        val city = slots.find { it.slotType == "location" }?.slotValue ?: "当前位置"
        weatherAPI.getWeather(city) { weather ->
            showResponse("${city}的天气是：${weather}")
        }
    }

    private fun handleMusic(slots: List<IntentSlot>) {
        // Call music API
        val artist = slots.find { it.slotType == "artist" }?.slotValue
        if (artist != null) {
            musicPlayer.play(artist)
            showResponse("正在播放${artist}的歌曲")
        }
    }

    private fun useLLM(text: String) {
        // Fallback to LLaMA or other LLM
        llamaModel.generate(text) { response ->
            showResponse(response)
        }
    }
}
```

### Advanced: Manual Check

```kotlin
fun handleUserInput(text: String) {
    val result = recognizer.predict(text)

    if (result.hit) {
        // ✅ HIT: High confidence
        Log.d(TAG, "Intent hit: ${result.intent} (${result.confidence * 100}%)")

        // Execute intent handler
        when (result.intent) {
            "weather-query" -> {
                val location = result.slots.find { it.slotType == "location" }
                fetchWeather(location?.slotValue)
            }
            "music-play" -> {
                val artist = result.slots.find { it.slotType == "artist" }
                playMusic(artist?.slotValue)
            }
            else -> useLLM(text)
        }
    } else {
        // ❌ NO HIT: Low confidence, fallback
        Log.d(TAG, "Intent miss: ${result.rawIntent} (${result.confidence * 100}%)")
        useLLM(text)
    }
}
```

### Dynamic Threshold Adjustment

```kotlin
class AdaptiveRecognizer(private val recognizer: IntentRecognizer) {
    private var hitCount = 0
    private var totalCount = 0

    fun predict(text: String): IntentResult {
        val result = recognizer.predict(text)

        totalCount++
        if (result.hit) {
            hitCount++
        }

        // Adjust threshold based on hit rate
        val hitRate = hitCount.toFloat() / totalCount
        when {
            hitRate < 0.1f -> recognizer.setThreshold(0.4f)  // Too strict, lower it
            hitRate > 0.7f -> recognizer.setThreshold(0.7f)  // Too loose, raise it
        }

        return result
    }
}
```

## Integration Patterns

### Pattern 1: Intent Router

```cpp
void process_input(const std::string& text) {
    auto result = recognizer.predict(text);

    if (!result.hit) {
        // Fallback to LLM
        return llama_generate(text);
    }

    // Route to handler based on intent domain
    if (result.intent.starts_with("weather-")) {
        return handle_weather(result.slots);
    } else if (result.intent.starts_with("music-")) {
        return handle_music(result.slots);
    } else if (result.intent.starts_with("alarm-")) {
        return handle_alarm(result.slots);
    } else {
        // Unknown intent domain, fallback
        return llama_generate(text);
    }
}
```

### Pattern 2: Multi-Stage Routing

```cpp
void advanced_routing(const std::string& text) {
    auto result = recognizer.predict(text);

    if (result.hit) {
        // Stage 1: Try domain-specific handler
        if (try_domain_handler(result.intent, result.slots)) {
            return;  // Success
        }
    }

    // Stage 2: Try semantic search (RAG)
    auto rag_result = semantic_search(text);
    if (rag_result.confidence > 0.7f) {
        return answer_from_knowledge_base(rag_result);
    }

    // Stage 3: Fallback to LLM
    return llama_generate(text);
}
```

### Pattern 3: Confidence-Based Response

```cpp
void confidence_based_response(const std::string& text) {
    auto result = recognizer.predict(text);

    if (result.intent_confidence > 0.9f) {
        // Very confident: Direct action
        execute_intent_immediately(result);
    } else if (result.intent_confidence > 0.6f) {
        // Confident: Action with confirmation
        ask_user_confirmation(result);
    } else {
        // Not confident: Use LLM
        llama_generate(text);
    }
}
```

## Threshold Selection Guide

| Threshold | Hit Rate | Use Case |
|-----------|----------|----------|
| 0.4-0.5   | High (~70%) | Exploratory, want to catch most intents |
| 0.6-0.7   | Medium (~40%) | **Recommended** - balanced approach |
| 0.8-0.9   | Low (~20%) | Conservative, only very clear intents |

### A/B Testing Example

```python
# Test different thresholds on your data
thresholds = [0.4, 0.5, 0.6, 0.7, 0.8]
results = {}

for threshold in thresholds:
    recognizer.set_threshold(threshold)

    hits = 0
    false_positives = 0

    for text, expected_intent in test_data:
        result = recognizer.predict(text)

        if result.hit:
            hits += 1
            if result.intent != expected_intent:
                false_positives += 1

    results[threshold] = {
        'hit_rate': hits / len(test_data),
        'precision': 1 - (false_positives / hits) if hits > 0 else 0
    }

# Find optimal threshold
optimal = max(results.items(), key=lambda x: x[1]['precision'])
print(f"Optimal threshold: {optimal[0]}")
```

## Debugging

### C++ Debug Mode

```bash
# Show raw intent when !hit
./intent-recognition --debug

# Output:
# ❌ NO HIT (confidence: 45% < threshold)
#    [Debug] Best guess was: general-chat
```

### Kotlin Debug Mode

```kotlin
// Print formatted result with debug info
val result = recognizer.predict(text)
Log.d(TAG, result.format(showDebug = true))

// Manual debug info
if (!result.hit) {
    Log.d(TAG, "Raw intent: ${result.rawIntent}")
    Log.d(TAG, "Confidence: ${result.confidence}")
    Log.d(TAG, "Threshold: ${recognizer.getThreshold()}")
}
```

### Logging Pattern

```cpp
void log_prediction(const PredictionResult& result) {
    if (result.hit) {
        LOG_INFO("INTENT_HIT", {
            {"text", result.text},
            {"intent", result.intent},
            {"confidence", result.intent_confidence},
            {"slots", result.slots}
        });
    } else {
        LOG_INFO("INTENT_MISS", {
            {"text", result.text},
            {"raw_intent", result.raw_intent},
            {"confidence", result.intent_confidence},
            {"threshold", recognizer.get_threshold()}
        });
    }
}
```

## Best Practices

1. **Start with default threshold (0.6)** and adjust based on metrics
2. **Log all predictions** to analyze hit/miss patterns
3. **A/B test** different thresholds with real users
4. **Monitor false positives** (wrong intents) and false negatives (missed intents)
5. **Provide manual override** for users (e.g., "I meant to search, not play music")
6. **Graceful fallback**: Always have LLM ready for NO HIT cases
7. **Domain-specific thresholds**: Use lower threshold for critical intents (e.g., emergency)

## Example Metrics Dashboard

Track these metrics to optimize threshold:

```
Threshold: 0.6

Intent Hit Rate: 45%
├─ True Positives: 42%  ✅
├─ False Positives: 3%  ⚠️
└─ False Negatives: 8%  ⚠️

LLM Fallback Rate: 55%

Average Response Time:
├─ Intent Handler: 50ms  ⚡
└─ LLM Fallback: 2000ms  🐌

User Satisfaction: 4.5/5.0
```

## Troubleshooting

### Too Many False Positives

**Problem**: Wrong intents being executed

**Solution**:
```cpp
// Increase threshold
recognizer.set_threshold(0.75f);
```

### Too Many Misses

**Problem**: Valid intents not being recognized

**Solution**:
```cpp
// Decrease threshold
recognizer.set_threshold(0.5f);

// Or retrain model with more data
```

### Inconsistent Predictions

**Problem**: Same input gets different results

**Solution**:
- Check if model is properly quantized
- Ensure consistent tokenization
- Verify input preprocessing

## Summary

The confidence threshold is a critical parameter for balancing precision and recall:

- **Lower threshold**: More hits, but more false positives
- **Higher threshold**: Fewer false positives, but more misses
- **Default (0.6)**: Good starting point for most applications

Use the `hit` field in `PredictionResult` to route between intent handlers and LLM fallback, providing users with fast, accurate responses for known intents while maintaining flexibility for open-ended conversation.

## Further Reading

- [README.md](README.md) - Complete documentation
- [QUICKSTART.md](QUICKSTART.md) - 5-minute quick start
- [INTEGRATION_SUMMARY.md](INTEGRATION_SUMMARY.md) - Integration overview
