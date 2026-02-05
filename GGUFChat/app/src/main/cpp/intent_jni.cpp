/**
 * ============================================================================
 * intent_jni.cpp - Intent 意图识别 JNI 桥接层
 * ============================================================================
 *
 * 【文件作用】
 * 这个文件是 Intent 意图识别功能的 JNI 桥接层。
 * 它连接 Kotlin 代码 (IntentRecognizer.kt) 和 C++ 实现 (intent_recognizer.cpp)。
 *
 * 【什么是 Intent 意图识别？】
 * 当用户说 "打开音乐" 时，我们需要判断这是一个 "播放音乐" 的意图，
 * 而不是一个需要 LLM 回答的问题。
 *
 * Intent 识别的作用就是：
 * - 判断用户的输入是否是一个已知的命令/意图
 * - 如果是，提取出相关的参数 (slot)
 * - 如果不是，就把输入交给 LLM 处理
 *
 * 【工作流程图】
 *
 *   用户输入: "播放周杰伦的歌"
 *         │
 *         ▼
 *   ┌─────────────────────────────────┐
 *   │  Intent 识别器                    │
 *   │  - 意图: play_music              │
 *   │  - 槽位: artist=周杰伦            │
 *   │  - 置信度: 95%                   │
 *   └─────────────────────────────────┘
 *         │
 *         ▼
 *   ┌─────────────────────────────────┐
 *   │  置信度 > 阈值?                   │
 *   │  是 → 执行命令                   │
 *   │  否 → 交给 LLM 处理              │
 *   └─────────────────────────────────┘
 *
 * 【与 llama-android-jni.cpp 的关系】
 * - llama-android-jni.cpp: LLM 大语言模型推理 (用于对话)
 * - intent_jni.cpp (本文件): Intent 识别 (用于命令识别)
 *
 * 两者是独立的功能，可以单独使用，也可以组合使用。
 *
 * ============================================================================
 */

// ============================================================================
// 头文件引入
// ============================================================================

#include <jni.h>              // JNI 核心库
#include <android/log.h>      // Android 日志库
#include <string>             // C++ 字符串
#include <memory>             // 智能指针 (std::unique_ptr 等)
#include <vector>             // 动态数组
#include "intent_recognizer.h"  // Intent 识别器的 C++ 实现

// ============================================================================
// 日志宏定义
// ============================================================================

#define INTENT_TAG "IntentJNI"  // 日志标签

// Info 级别日志
#define INTENT_LOGI(...) __android_log_print(ANDROID_LOG_INFO, INTENT_TAG, __VA_ARGS__)

// Error 级别日志
#define INTENT_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, INTENT_TAG, __VA_ARGS__)

// 使用 intent 命名空间，这样可以直接使用 IntentRecognizer 而不用写 intent::IntentRecognizer
using namespace intent;

// ============================================================================
// JNI 函数定义
// ============================================================================

extern "C" {

// ============================================================================
// 函数 1: nativeIntentInit - 初始化 Intent 识别器
// ============================================================================
/**
 * 【函数】初始化 Intent 识别器
 *
 * 【Kotlin 调用示例】
 * val contextPtr = nativeIntentInit(modelDir, 4, 0.7f)
 *
 * 【参数说明】
 * @param env                 - JNI 环境指针 (自动传入)
 * @param thiz                - Java 对象 (自动传入)
 * @param jmodelDir           - 模型目录路径 (包含 ONNX 模型和词汇表)
 * @param numThreads          - 推理使用的线程数
 * @param confidenceThreshold - 置信度阈值 (0.0-1.0)
 *                              只有置信度 >= 阈值时才认为识别成功
 *
 * @return 识别器指针 (成功) 或 0 (失败)
 *
 * 【模型目录结构】
 * modelDir/
 *   ├── joint_model_quantized.onnx  // ONNX 模型文件
 *   ├── vocab.txt                   // 词汇表
 *   ├── intent_labels.txt           // 意图标签列表
 *   ├── slot_labels.txt             // 槽位标签列表
 *   └── config.json                 // 配置文件 (可选)
 */
JNIEXPORT jlong JNICALL
Java_com_stdemo_ggufchat_IntentRecognizer_nativeIntentInit(
    JNIEnv* env,
    jobject thiz,
    jstring jmodelDir,
    jint numThreads,
    jfloat confidenceThreshold) {

    // 将 Java 字符串转换为 C 字符串
    const char* model_dir = env->GetStringUTFChars(jmodelDir, nullptr);
    if (!model_dir) {
        INTENT_LOGE("Failed to get model directory string");
        return 0;
    }

    INTENT_LOGI("Initializing intent recognizer: dir=%s, threads=%d, threshold=%.2f",
                model_dir, numThreads, confidenceThreshold);

    try {
        // ============================================================
        // 步骤 1: 创建配置
        // ============================================================
        IntentConfig config;
        config.model_dir = model_dir;                      // 模型目录
        config.num_threads = numThreads;                   // 线程数
        config.confidence_threshold = confidenceThreshold; // 置信度阈值
        config.use_gpu = false;                            // 不使用 GPU (ONNX Runtime 在移动端 GPU 支持有限)

        // ============================================================
        // 步骤 2: 创建识别器实例
        // ============================================================
        // new 关键字在堆上创建对象，返回指针
        // 这个指针会转换为 jlong 传给 Kotlin，需要在 nativeIntentFree 中释放
        IntentRecognizer* recognizer = new IntentRecognizer(config);

        // ============================================================
        // 步骤 3: 初始化 (加载模型、词汇表等)
        // ============================================================
        if (!recognizer->initialize()) {
            INTENT_LOGE("Failed to initialize recognizer");
            delete recognizer;  // 初始化失败，释放内存
            env->ReleaseStringUTFChars(jmodelDir, model_dir);
            return 0;
        }

        env->ReleaseStringUTFChars(jmodelDir, model_dir);
        INTENT_LOGI("Intent recognizer initialized successfully");

        // 将指针转换为 jlong 返回
        // reinterpret_cast 是 C++ 的强制类型转换
        return reinterpret_cast<jlong>(recognizer);

    } catch (const std::exception& e) {
        // 捕获所有标准异常
        INTENT_LOGE("Exception during initialization: %s", e.what());
        env->ReleaseStringUTFChars(jmodelDir, model_dir);
        return 0;
    }
}

// ============================================================================
// 函数 2: nativeIntentPredict - 执行意图识别
// ============================================================================
/**
 * 【函数】对输入文本进行意图识别
 *
 * 【Kotlin 调用示例】
 * val result = nativeIntentPredict(contextPtr, "打开音乐")
 *
 * 【参数说明】
 * @param env        - JNI 环境指针
 * @param thiz       - Java 对象
 * @param contextPtr - 识别器指针 (从 nativeIntentInit 获得)
 * @param jtext      - 要识别的文本
 *
 * @return IntentResult 对象，包含:
 *         - text: 原始输入文本
 *         - hit: 是否识别成功 (置信度 >= 阈值)
 *         - intent: 识别到的意图 (如 "play_music")
 *         - confidence: 置信度 (0.0-1.0)
 *         - slots: 槽位列表 (如 [("artist", "周杰伦")])
 *         - rawIntent: 原始识别结果 (即使置信度低也会有)
 *
 * 【返回值解释】
 * - hit=true: 置信度高，可以执行对应的命令
 * - hit=false: 置信度低，应该交给 LLM 处理
 */
JNIEXPORT jobject JNICALL
Java_com_stdemo_ggufchat_IntentRecognizer_nativeIntentPredict(
    JNIEnv* env,
    jobject thiz,
    jlong contextPtr,
    jstring jtext) {

    // 将 jlong 转回指针
    IntentRecognizer* recognizer = reinterpret_cast<IntentRecognizer*>(contextPtr);
    if (!recognizer) {
        INTENT_LOGE("Invalid recognizer pointer");
        return nullptr;
    }

    // 获取输入文本
    const char* text = env->GetStringUTFChars(jtext, nullptr);
    if (!text) {
        INTENT_LOGE("Failed to get input text");
        return nullptr;
    }

    try {
        // ============================================================
        // 步骤 1: 调用 C++ 层进行预测
        // ============================================================
        PredictionResult result = recognizer->predict(text);
        env->ReleaseStringUTFChars(jtext, text);

        // ============================================================
        // 步骤 2: 创建 Java IntentResult 对象
        // ============================================================
        // 这部分代码展示了如何在 JNI 中创建和操作 Java 对象

        // 2.1 找到 IntentResult 类
        jclass resultClass = env->FindClass("com/stdemo/ggufchat/IntentResult");
        if (!resultClass) {
            INTENT_LOGE("Failed to find IntentResult class");
            return nullptr;
        }

        // 2.2 找到无参构造函数
        // "<init>" 是 Java 构造函数的内部名称
        // "()V" 是方法签名：() 表示无参数，V 表示返回 void
        jmethodID constructor = env->GetMethodID(resultClass, "<init>", "()V");
        if (!constructor) {
            INTENT_LOGE("Failed to find IntentResult constructor");
            return nullptr;
        }

        // 2.3 创建新对象
        jobject resultObj = env->NewObject(resultClass, constructor);
        if (!resultObj) {
            INTENT_LOGE("Failed to create IntentResult object");
            return nullptr;
        }

        // ============================================================
        // 步骤 3: 设置对象的字段值
        // ============================================================
        // 每个字段需要先获取 FieldID，然后设置值

        // 获取所有字段的 ID
        // 字段签名说明:
        // - "Ljava/lang/String;" = String 类型
        // - "Z" = boolean 类型
        // - "F" = float 类型
        // - "Ljava/util/List;" = List 类型
        jfieldID textField = env->GetFieldID(resultClass, "text", "Ljava/lang/String;");
        jfieldID hitField = env->GetFieldID(resultClass, "hit", "Z");
        jfieldID intentField = env->GetFieldID(resultClass, "intent", "Ljava/lang/String;");
        jfieldID confidenceField = env->GetFieldID(resultClass, "confidence", "F");
        jfieldID slotsField = env->GetFieldID(resultClass, "slots", "Ljava/util/List;");
        jfieldID rawIntentField = env->GetFieldID(resultClass, "rawIntent", "Ljava/lang/String;");

        // 设置字段值
        env->SetObjectField(resultObj, textField, env->NewStringUTF(result.text.c_str()));
        env->SetBooleanField(resultObj, hitField, result.hit);
        env->SetObjectField(resultObj, intentField, env->NewStringUTF(result.intent.c_str()));
        env->SetFloatField(resultObj, confidenceField, result.intent_confidence);
        env->SetObjectField(resultObj, rawIntentField, env->NewStringUTF(result.raw_intent.c_str()));

        // ============================================================
        // 步骤 4: 创建 slots 列表
        // ============================================================
        // Slot 是从用户输入中提取的关键信息
        // 比如 "播放周杰伦的歌" → slot_type="artist", slot_value="周杰伦"

        // 4.1 创建 ArrayList 对象
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
        jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

        jobject slotsList = env->NewObject(arrayListClass, arrayListConstructor);

        // 4.2 找到 IntentSlot 类
        jclass slotClass = env->FindClass("com/stdemo/ggufchat/IntentSlot");
        // 构造函数签名: (String, String) -> void
        jmethodID slotConstructor = env->GetMethodID(slotClass, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;)V");

        // 4.3 遍历 C++ 的 slots 列表，创建 Java 对象并添加到 ArrayList
        for (const auto& slot : result.slots) {
            // 创建 Java 字符串
            jstring slotType = env->NewStringUTF(slot.slot_type.c_str());
            jstring slotValue = env->NewStringUTF(slot.slot_value.c_str());

            // 创建 IntentSlot 对象
            jobject slotObj = env->NewObject(slotClass, slotConstructor, slotType, slotValue);

            // 添加到列表
            env->CallBooleanMethod(slotsList, arrayListAdd, slotObj);

            // 释放本地引用 (避免内存泄漏)
            env->DeleteLocalRef(slotType);
            env->DeleteLocalRef(slotValue);
            env->DeleteLocalRef(slotObj);
        }

        // 4.4 设置 slots 字段
        env->SetObjectField(resultObj, slotsField, slotsList);

        // ============================================================
        // 步骤 5: 清理本地引用
        // ============================================================
        env->DeleteLocalRef(arrayListClass);
        env->DeleteLocalRef(slotClass);
        env->DeleteLocalRef(slotsList);
        env->DeleteLocalRef(resultClass);

        // 输出日志
        if (result.hit) {
            INTENT_LOGI("Prediction HIT: intent=%s, confidence=%.2f%%",
                        result.intent.c_str(), result.intent_confidence * 100.0f);
        } else {
            INTENT_LOGI("Prediction NO HIT: confidence=%.2f%% (raw=%s)",
                        result.intent_confidence * 100.0f, result.raw_intent.c_str());
        }

        return resultObj;

    } catch (const std::exception& e) {
        INTENT_LOGE("Exception during prediction: %s", e.what());
        env->ReleaseStringUTFChars(jtext, text);
        return nullptr;
    }
}

// ============================================================================
// 函数 3: nativeIntentSetThreshold - 设置置信度阈值
// ============================================================================
/**
 * 【函数】动态设置置信度阈值
 *
 * 【用途】
 * 在运行时调整阈值，不需要重新初始化识别器。
 * - 阈值高: 更严格，只有非常确定的意图才会被识别
 * - 阈值低: 更宽松，更多输入会被识别为意图
 *
 * @param contextPtr - 识别器指针
 * @param threshold  - 新的阈值 (0.0-1.0)
 */
JNIEXPORT void JNICALL
Java_com_stdemo_ggufchat_IntentRecognizer_nativeIntentSetThreshold(
    JNIEnv* env,
    jobject thiz,
    jlong contextPtr,
    jfloat threshold) {

    IntentRecognizer* recognizer = reinterpret_cast<IntentRecognizer*>(contextPtr);
    if (recognizer) {
        recognizer->set_threshold(threshold);
        INTENT_LOGI("Threshold updated to: %.2f%%", threshold * 100.0f);
    } else {
        INTENT_LOGE("Invalid recognizer pointer");
    }
}

// ============================================================================
// 函数 4: nativeIntentGetThreshold - 获取当前置信度阈值
// ============================================================================
/**
 * 【函数】获取当前的置信度阈值
 *
 * @param contextPtr - 识别器指针
 * @return 当前阈值 (0.0-1.0)
 */
JNIEXPORT jfloat JNICALL
Java_com_stdemo_ggufchat_IntentRecognizer_nativeIntentGetThreshold(
    JNIEnv* env,
    jobject thiz,
    jlong contextPtr) {

    IntentRecognizer* recognizer = reinterpret_cast<IntentRecognizer*>(contextPtr);
    if (recognizer) {
        return recognizer->get_threshold();
    } else {
        INTENT_LOGE("Invalid recognizer pointer");
        return 0.0f;
    }
}

// ============================================================================
// 函数 5: nativeIntentFree - 释放资源
// ============================================================================
/**
 * 【函数】释放 Intent 识别器占用的资源
 *
 * 【重要】
 * 必须在不再使用识别器时调用此函数，否则会导致内存泄漏！
 *
 * @param contextPtr - 识别器指针
 */
JNIEXPORT void JNICALL
Java_com_stdemo_ggufchat_IntentRecognizer_nativeIntentFree(
    JNIEnv* env,
    jobject thiz,
    jlong contextPtr) {

    IntentRecognizer* recognizer = reinterpret_cast<IntentRecognizer*>(contextPtr);
    if (recognizer) {
        delete recognizer;  // 释放内存
        INTENT_LOGI("Intent recognizer freed");
    }
}

} // extern "C"

/**
 * ============================================================================
 * 文件结束
 * ============================================================================
 *
 * 【总结：Intent JNI 的核心流程】
 *
 * 1. Kotlin 调用 nativeIntentInit()
 *    ↓
 * 2. 创建配置，创建 IntentRecognizer 实例
 *    ↓
 * 3. 加载 ONNX 模型、词汇表、标签
 *    ↓
 * 4. 返回识别器指针给 Kotlin
 *
 * ---
 *
 * 5. Kotlin 调用 nativeIntentPredict()
 *    ↓
 * 6. C++ 层进行推理
 *    ↓
 * 7. 创建 Java IntentResult 对象
 *    ↓
 * 8. 返回结果给 Kotlin
 *
 * ---
 *
 * 9. Kotlin 调用 nativeIntentFree()
 *    ↓
 * 10. 释放资源
 *
 * ============================================================================
 */
