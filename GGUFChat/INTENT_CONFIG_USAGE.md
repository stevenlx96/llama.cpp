# Intent Recognition Configuration

## Quick Start - 只需改一个地方！

要修改意图识别的置信度阈值，只需要修改一个文件：

**`app/src/main/java/com/stdemo/ggufchat/IntentConfig.kt`**

```kotlin
object IntentConfig {
    // 🎯 只需要修改这里！
    var DEFAULT_CONFIDENCE_THRESHOLD = 0.85f

    var DEFAULT_NUM_THREADS = 4
}
```

## 使用场景

### 场景 1：在代码中修改（编译时）

直接修改 `IntentConfig.kt` 文件中的值：

```kotlin
object IntentConfig {
    var DEFAULT_CONFIDENCE_THRESHOLD = 0.75f  // 改成你想要的值
}
```

### 场景 2：在运行时动态修改

在应用启动时或任何时候修改：

```kotlin
class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // 在初始化之前设置阈值
        IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD = 0.90f

        // 然后初始化意图识别
        IntentRecognizerManager.initialize(this)
    }
}
```

### 场景 3：打包成 AAR 供他人使用

如果你要把这个项目打包成 AAR 给别人用，用户可以这样配置：

```kotlin
// 在用户的应用中
import com.stdemo.ggufchat.IntentConfig
import com.stdemo.ggufchat.IntentRecognizerManager

class UserApp : Application() {
    override fun onCreate() {
        super.onCreate()

        // 用户只需要设置这一个值
        IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD = 0.95f

        // 然后初始化
        IntentRecognizerManager.initialize(this)
    }
}
```

### 场景 4：根据用户设置动态调整

```kotlin
// 在设置界面
class SettingsActivity : AppCompatActivity() {

    fun onThresholdChanged(newThreshold: Float) {
        // 保存到 SharedPreferences
        getSharedPreferences("intent_prefs", MODE_PRIVATE)
            .edit()
            .putFloat("threshold", newThreshold)
            .apply()

        // 立即应用（如果已经初始化）
        IntentRecognizerManager.setThreshold(newThreshold)
    }
}

// 在应用启动时加载
class MyApp : Application() {
    override fun onCreate() {
        super.onCreate()

        // 从 SharedPreferences 加载用户的偏好设置
        val threshold = getSharedPreferences("intent_prefs", MODE_PRIVATE)
            .getFloat("threshold", 0.85f)

        IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD = threshold
        IntentRecognizerManager.initialize(this)
    }
}
```

## 阈值推荐值

| 阈值 | 适用场景 | 特点 |
|------|---------|------|
| 0.5 - 0.6 | 宽松匹配 | 更多意图命中，可能有误判 |
| 0.7 - 0.8 | 平衡模式 | 适合大多数场景 |
| **0.85** | **推荐默认值** | 较严格，减少误判 |
| 0.9 - 0.95 | 严格模式 | 只有非常确定的才命中 |
| 0.95+ | 极严格 | 几乎只接受完美匹配 |

## C++ 配置（高级用户）

C++ 层的默认值在 `intent_recognizer.h` 中，但**通常不需要修改**，因为 Kotlin 层会传递正确的值：

```cpp
// GGUFChat/app/src/main/cpp/include/intent_recognizer.h
struct IntentConfig {
    float confidence_threshold = 0.85f;  // 这个值只是 C++ 的 fallback
    // ...
};
```

**注意**：Kotlin 层的 `IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD` 优先级更高，会覆盖 C++ 的默认值。

## 验证配置

查看日志确认配置生效：

```
IntentRecognizerCPP: Initializing IntentRecognizer...
IntentJNI: Initializing intent recognizer: threshold=0.85
IntentRecognizer: Intent recognizer initialized: true (threshold: 85.0%)
```

## 示例项目结构（AAR 打包）

```
your-aar-project/
├── IntentConfig.kt          👈 用户只需要改这个文件
├── IntentRecognizerManager.kt
├── IntentRecognizer.kt
└── README.md               👈 告诉用户如何配置
```

在 README 中告诉用户：

> **配置意图识别阈值**
>
> 在使用前，修改 `IntentConfig.kt` 中的 `DEFAULT_CONFIDENCE_THRESHOLD` 值，
> 或在运行时调用：
> ```kotlin
> IntentConfig.DEFAULT_CONFIDENCE_THRESHOLD = 0.90f
> ```

## 总结

✅ **只需要修改一个地方：`IntentConfig.kt`**

✅ **支持编译时和运行时配置**

✅ **适合打包成 AAR 供他人使用**

✅ **配置变更立即生效（下次初始化时）**
