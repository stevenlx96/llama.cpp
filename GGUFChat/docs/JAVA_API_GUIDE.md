# ☕ GGUF Chat Android Library - Java API 调用指南

## 📖 概述

本指南详细介绍 GGUF Chat Android Library 的 Java/Kotlin API，包括所有公开接口、方法和回调。

### 核心 API 类

- **LlamaHelper** - 简化的辅助类，推荐用于 UE/Unity 集成
- **LlamaManagerJava** - 完整功能的管理类
- **LlamaEngine** - 核心引擎（内部使用）
- **ModelManager** - 模型管理（内部使用）

---

## 🚀 LlamaHelper API

### 概述

`LlamaHelper` 是专为 UE/Unity 集成设计的简化 API，自动处理 Context 获取和生命周期管理。

### 创建实例

#### Kotlin

```kotlin
// 方式 1: 自动获取 Context（推荐）
val llama = LlamaHelper.create()
if (llama != null) {
    // 使用 llama
}

// 方式 2: 手动指定 Context
val llama = LlamaHelper.createWithContext(context)
```

#### Java

```java
// 方式 1: 自动获取 Context（推荐）
LlamaHelper llama = LlamaHelper.create();
if (llama != null) {
    // 使用 llama
}

// 方式 2: 手动指定 Context
LlamaHelper llama = LlamaHelper.createWithContext(context);
```

### 初始化

#### Kotlin

```kotlin
// 默认初始化
val success = llama.initialize()
if (success) {
    // 初始化成功
}

// 使用指定模型初始化
val success = llama.initialize("/path/to/model.gguf")
if (success) {
    // 初始化成功
}
```

#### Java

```java
// 默认初始化
boolean success = llama.initialize();
if (success) {
    // 初始化成功
}

// 使用指定模型初始化
boolean success = llama.initialize("/path/to/model.gguf");
if (success) {
    // 初始化成功
}
```

### 设置回调

#### Kotlin

```kotlin
// 方式 1: 使用接口
llama.setOnResponse(object : LlamaHelper.ResponseCallback {
    override fun onResponse(text: String) {
        Log.i(TAG, "Response: $text")
    }

    override fun onError(error: String) {
        Log.e(TAG, "Error: $error")
    }
})

// 方式 2: 使用 Lambda（仅响应）
llama.setOnResponse { text ->
    Log.i(TAG, "Response: $text")
}
```

#### Java

```java
// 使用接口
llama.setOnResponse(new LlamaHelper.ResponseCallback() {
    @Override
    public void onResponse(String text) {
        Log.i(TAG, "Response: " + text);
    }

    @Override
    public void onError(String error) {
        Log.e(TAG, "Error: " + error);
    }
});
```

### 发送消息

#### Kotlin

```kotlin
llama.sendMessage("你好，请介绍一下自己")
```

#### Java

```java
llama.sendMessage("你好，请介绍一下自己");
```

### 对话控制

#### Kotlin

```kotlin
// 开始新对话
llama.startNewChat()

// 停止生成
llama.stopGeneration()

// 释放资源
llama.release()
```

#### Java

```java
// 开始新对话
llama.startNewChat();

// 停止生成
llama.stopGeneration();

// 释放资源
llama.release();
```

### 状态查询

#### Kotlin

```kotlin
// 检查是否已初始化
if (llama.isInitialized()) {
    // 已初始化
}

// 检查是否正在生成
if (llama.isGenerating()) {
    // 正在生成
}

// 获取引擎信息
val info = llama.info
Log.i(TAG, info)
```

#### Java

```java
// 检查是否已初始化
if (llama.isInitialized()) {
    // 已初始化
}

// 检查是否正在生成
if (llama.isGenerating()) {
    // 正在生成
}

// 获取引擎信息
String info = llama.getInfo();
Log.i(TAG, info);
```

### 配置参数

#### Kotlin

```kotlin
// 设置生成配置
llama.setGenerationConfig(
    temperature = 0.7f,
    maxTokens = 512,
    topP = 0.9f
)

// 设置系统提示词
llama.setSystemPrompt("你是一个友好的助手")
```

#### Java

```java
// 设置生成配置
llama.setGenerationConfig(
    0.7f,  // temperature
    512,   // maxTokens
    0.9f   // topP
);

// 设置系统提示词
llama.setSystemPrompt("你是一个友好的助手");
```

### 完整示例

#### Kotlin

```kotlin
class ChatManager {
    private val TAG = "ChatManager"
    private var llama: LlamaHelper? = null

    fun initialize(): Boolean {
        // 创建实例
        llama = LlamaHelper.create()
        if (llama == null) {
            Log.e(TAG, "Failed to create LlamaHelper")
            return false
        }

        // 设置回调
        llama?.setOnResponse { text ->
            Log.i(TAG, "Response: $text")
            // 更新 UI 或处理响应
        }

        // 初始化
        val success = llama?.initialize() ?: false
        if (!success) {
            Log.e(TAG, "Failed to initialize")
            return false
        }

        Log.i(TAG, "Initialized successfully")
        return true
    }

    fun sendMessage(message: String) {
        llama?.sendMessage(message)
    }

    fun startNewChat() {
        llama?.startNewChat()
    }

    fun release() {
        llama?.release()
        llama = null
    }
}
```

#### Java

```java
public class ChatManager {
    private static final String TAG = "ChatManager";
    private LlamaHelper llama;

    public boolean initialize() {
        // 创建实例
        llama = LlamaHelper.create();
        if (llama == null) {
            Log.e(TAG, "Failed to create LlamaHelper");
            return false;
        }

        // 设置回调
        llama.setOnResponse(new LlamaHelper.ResponseCallback() {
            @Override
            public void onResponse(String text) {
                Log.i(TAG, "Response: " + text);
                // 更新 UI 或处理响应
            }

            @Override
            public void onError(String error) {
                Log.e(TAG, "Error: " + error);
            }
        });

        // 初始化
        boolean success = llama.initialize();
        if (!success) {
            Log.e(TAG, "Failed to initialize");
            return false;
        }

        Log.i(TAG, "Initialized successfully");
        return true;
    }

    public void sendMessage(String message) {
        if (llama != null) {
            llama.sendMessage(message);
        }
    }

    public void startNewChat() {
        if (llama != null) {
            llama.startNewChat();
        }
    }

    public void release() {
        if (llama != null) {
            llama.release();
            llama = null;
        }
    }
}
```

---

## 🔧 LlamaManagerJava API

### 概述

`LlamaManagerJava` 提供了完整的 API 功能，支持同步和异步操作，需要手动处理 Context。

### 创建实例

#### Kotlin

```kotlin
val context: Context = ... // 获取 Context
val manager = LlamaManagerJava(context)
```

#### Java

```java
Context context = ... // 获取 Context
LlamaManagerJava manager = new LlamaManagerJava(context);
```

### 初始化

#### Kotlin

```kotlin
// 同步初始化
val success = manager.initializeSync()

// 同步初始化（指定模型）
val success = manager.initializeSync("/path/to/model.gguf")

// 异步初始化
manager.initializeAsync(object : LlamaCallback<Boolean> {
    override fun onSuccess(result: Boolean) {
        Log.i(TAG, "Initialized successfully")
    }

    override fun onError(error: Throwable) {
        Log.e(TAG, "Initialization failed", error)
    }
})
```

#### Java

```java
// 同步初始化
boolean success = manager.initializeSync();

// 同步初始化（指定模型）
boolean success = manager.initializeSync("/path/to/model.gguf");

// 异步初始化
manager.initializeAsync(new LlamaCallback<Boolean>() {
    @Override
    public void onSuccess(Boolean result) {
        Log.i(TAG, "Initialized successfully");
    }

    @Override
    public void onError(Throwable error) {
        Log.e(TAG, "Initialization failed", error);
    }
});
```

### 设置回调

#### Kotlin

```kotlin
// 响应回调
manager.setOnResponse(object : LlamaCallback<String> {
    override fun onSuccess(result: String) {
        Log.i(TAG, "Response: $result")
    }

    override fun onError(error: Throwable) {
        Log.e(TAG, "Error: $error")
    }
})

// 状态变化回调
manager.setOnStateChanged(object : StateChangedCallback {
    override fun onStateChanged(state: String) {
        Log.i(TAG, "State: $state")
    }
})

// 错误回调
manager.setOnError(object : ErrorCallback {
    override fun onError(error: String) {
        Log.e(TAG, "Error: $error")
    }
})
```

#### Java

```java
// 响应回调
manager.setOnResponse(new LlamaCallback<String>() {
    @Override
    public void onSuccess(String result) {
        Log.i(TAG, "Response: " + result);
    }

    @Override
    public void onError(Throwable error) {
        Log.e(TAG, "Error: " + error);
    }
});

// 状态变化回调
manager.setOnStateChanged(new StateChangedCallback() {
    @Override
    public void onStateChanged(String state) {
        Log.i(TAG, "State: " + state);
    }
});

// 错误回调
manager.setOnError(new ErrorCallback() {
    @Override
    public void onError(String error) {
        Log.e(TAG, "Error: " + error);
    }
});
```

### 消息操作

#### Kotlin

```kotlin
// 发送消息
manager.sendMessage("你好")

// 开始新对话
manager.startNewChat()

// 停止生成
manager.stopGeneration()
```

#### Java

```java
// 发送消息
manager.sendMessage("你好");

// 开始新对话
manager.startNewChat();

// 停止生成
manager.stopGeneration();
```

### 状态查询

#### Kotlin

```kotlin
// 检查模型是否就绪
if (manager.isModelReady()) {
    // 模型已就绪
}

// 获取当前状态
val state = manager.state
Log.i(TAG, "State: $state")

// 检查是否正在生成
if (manager.isGenerating()) {
    // 正在生成
}

// 获取对话历史
val history = manager.chatHistory
history.forEach { msg ->
    Log.i(TAG, "History: $msg")
}

// 获取模型目录
val modelDir = manager.modelDir
Log.i(TAG, "Model dir: $modelDir")
```

#### Java

```java
// 检查模型是否就绪
if (manager.isModelReady()) {
    // 模型已就绪
}

// 获取当前状态
String state = manager.getState();
Log.i(TAG, "State: " + state);

// 检查是否正在生成
if (manager.isGenerating()) {
    // 正在生成
}

// 获取对话历史
List<String> history = manager.getChatHistory();
for (String msg : history) {
    Log.i(TAG, "History: " + msg);
}

// 获取模型目录
String modelDir = manager.getModelDir();
Log.i(TAG, "Model dir: " + modelDir);
```

### 配置参数

#### Kotlin

```kotlin
// 设置生成配置
manager.setGenerationConfig(
    temperature = 0.7f,
    maxTokens = 512,
    topP = 0.9f
)

// 设置系统提示词
manager.setSystemPrompt("你是一个友好的助手")
```

#### Java

```java
// 设置生成配置
manager.setGenerationConfig(
    0.7f,  // temperature
    512,   // maxTokens
    0.9f   // topP
);

// 设置系统提示词
manager.setSystemPrompt("你是一个友好的助手");
```

### 生命周期管理

#### Kotlin

```kotlin
// 获取引擎信息
val info = manager.engineInfo
Log.i(TAG, info)

// 释放资源
manager.release()
```

#### Java

```java
// 获取引擎信息
String info = manager.getEngineInfo();
Log.i(TAG, info);

// 释放资源
manager.release();
```

---

## 📋 回调接口

### LlamaHelper.ResponseCallback

用于接收生成响应的回调接口。

#### Kotlin

```kotlin
interface ResponseCallback {
    fun onResponse(text: String)
    fun onError(error: String)
}
```

#### Java

```java
interface ResponseCallback {
    void onResponse(String text);
    void onError(String error);
}
```

### LlamaCallback<T>

通用回调接口，支持泛型。

#### Kotlin

```kotlin
interface LlamaCallback<T> {
    fun onSuccess(result: T)
    fun onError(error: Throwable)
}
```

#### Java

```java
interface LlamaCallback<T> {
    void onSuccess(T result);
    void onError(Throwable error);
}
```

### StateChangedCallback

状态变化回调接口。

#### Kotlin

```kotlin
interface StateChangedCallback {
    fun onStateChanged(state: String)
}
```

#### Java

```java
interface StateChangedCallback {
    void onStateChanged(String state);
}
```

### ErrorCallback

错误回调接口。

#### Kotlin

```kotlin
interface ErrorCallback {
    fun onError(error: String)
}
```

#### Java

```java
interface ErrorCallback {
    void onError(String error);
}
```

---

## 🔍 最佳实践

### 1. 生命周期管理

```kotlin
class ChatApplication : Application() {
    private lateinit var llama: LlamaHelper

    override fun onCreate() {
        super.onCreate()
        llama = LlamaHelper.create() ?: return
        llama.initialize()
    }

    override fun onTerminate() {
        llama.release()
        super.onTerminate()
    }
}
```

### 2. 线程安全

```kotlin
// 所有回调都在主线程执行
llama.setOnResponse { text ->
    // 可以安全地更新 UI
    textView.text = text
}
```

### 3. 错误处理

```kotlin
llama.setOnResponse(object : LlamaHelper.ResponseCallback {
    override fun onResponse(text: String) {
        // 处理响应
    }

    override fun onError(error: String) {
        // 处理错误
        when {
            error.contains("model") -> {
                // 模型相关错误
            }
            error.contains("init") -> {
                // 初始化错误
            }
            else -> {
                // 其他错误
            }
        }
    }
})
```

### 4. 内存管理

```kotlin
// 及时释放资源
override fun onDestroy() {
    llama?.release()
    llama = null
    super.onDestroy()
}
```

---

## ❓ 常见问题

### 1. 初始化失败

**问题**：`initialize()` 返回 false

**解决**：
```kotlin
// 检查模型文件
val modelFile = File("/path/to/model.gguf")
if (!modelFile.exists()) {
    Log.e(TAG, "Model file not found")
}

// 检查权限
if (ContextCompat.checkSelfPermission(context, Manifest.permission.READ_EXTERNAL_STORAGE)
    != PackageManager.PERMISSION_GRANTED) {
    // 请求权限
}
```

### 2. 回调未触发

**问题**：设置回调后没有响应

**解决**：
```kotlin
// 确保在初始化成功后设置回调
if (llama.initialize()) {
    llama.setOnResponse { text ->
        // 处理响应
    }
}
```

### 3. 内存泄漏

**问题**：内存占用持续增长

**解决**：
```kotlin
// 在生命周期结束时释放资源
override fun onDestroy() {
    llama?.release()
    llama = null
}
```

---

## 🔗 相关文档

- [编译构建指南](BUILD_GUIDE.md) - 如何编译 AAR 文件
- [UE 集成调用指南](UE_INTEGRATION_GUIDE.md) - UE 集成详细说明
- [项目总览](README.md) - 项目概述

---

**文档版本**: v1.0
**最后更新**: 2026-04-14
