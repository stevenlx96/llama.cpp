#include <jni.h>
#include <android/log.h>
#include <string>
#include <memory>
#include "intent_recognizer.h"

#define LOG_TAG "IntentJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace intent;

// Global recognizer instance
static std::unique_ptr<IntentRecognizer> g_recognizer = nullptr;

extern "C" {

/**
 * Initialize the intent recognizer
 *
 * @param env JNI environment
 * @param thiz Java object
 * @param modelDir Path to model directory
 * @param numThreads Number of CPU threads
 * @param confidenceThreshold Minimum confidence for intent "hit"
 * @return true if successful, false otherwise
 */
JNIEXPORT jboolean JNICALL
Java_com_llama_cpp_IntentRecognizer_nativeInit(
    JNIEnv* env,
    jobject thiz,
    jstring modelDir,
    jint numThreads,
    jfloat confidenceThreshold) {

    const char* model_dir_cstr = env->GetStringUTFChars(modelDir, nullptr);
    if (!model_dir_cstr) {
        LOGE("Failed to get model directory string");
        return JNI_FALSE;
    }

    LOGI("Initializing intent recognizer with model dir: %s", model_dir_cstr);
    LOGI("Confidence threshold: %.2f%%", confidenceThreshold * 100.0f);

    try {
        IntentConfig config;
        config.model_dir = model_dir_cstr;
        config.num_threads = numThreads;
        config.confidence_threshold = confidenceThreshold;
        config.use_gpu = false; // Can be made configurable

        g_recognizer = std::make_unique<IntentRecognizer>(config);

        if (!g_recognizer->initialize()) {
            LOGE("Failed to initialize recognizer");
            env->ReleaseStringUTFChars(modelDir, model_dir_cstr);
            g_recognizer.reset();
            return JNI_FALSE;
        }

        env->ReleaseStringUTFChars(modelDir, model_dir_cstr);
        LOGI("Intent recognizer initialized successfully");
        return JNI_TRUE;

    } catch (const std::exception& e) {
        LOGE("Exception during initialization: %s", e.what());
        env->ReleaseStringUTFChars(modelDir, model_dir_cstr);
        g_recognizer.reset();
        return JNI_FALSE;
    }
}

/**
 * Predict intent and slots for input text
 *
 * @param env JNI environment
 * @param thiz Java object
 * @param inputText Input text string
 * @return Java IntentResult object (or null on error)
 */
JNIEXPORT jobject JNICALL
Java_com_llama_cpp_IntentRecognizer_nativePredict(
    JNIEnv* env,
    jobject thiz,
    jstring inputText) {

    if (!g_recognizer) {
        LOGE("Recognizer not initialized");
        return nullptr;
    }

    const char* input_cstr = env->GetStringUTFChars(inputText, nullptr);
    if (!input_cstr) {
        LOGE("Failed to get input text string");
        return nullptr;
    }

    try {
        // Predict
        PredictionResult result = g_recognizer->predict(input_cstr);
        env->ReleaseStringUTFChars(inputText, input_cstr);

        // Find IntentResult class
        jclass resultClass = env->FindClass("com/llama/cpp/IntentResult");
        if (!resultClass) {
            LOGE("Failed to find IntentResult class");
            return nullptr;
        }

        // Find constructor
        jmethodID constructor = env->GetMethodID(resultClass, "<init>", "()V");
        if (!constructor) {
            LOGE("Failed to find IntentResult constructor");
            return nullptr;
        }

        // Create IntentResult object
        jobject resultObj = env->NewObject(resultClass, constructor);
        if (!resultObj) {
            LOGE("Failed to create IntentResult object");
            return nullptr;
        }

        // Set fields
        jfieldID textField = env->GetFieldID(resultClass, "text", "Ljava/lang/String;");
        jfieldID hitField = env->GetFieldID(resultClass, "hit", "Z");
        jfieldID intentField = env->GetFieldID(resultClass, "intent", "Ljava/lang/String;");
        jfieldID confidenceField = env->GetFieldID(resultClass, "confidence", "F");
        jfieldID slotsField = env->GetFieldID(resultClass, "slots", "Ljava/util/List;");
        jfieldID rawIntentField = env->GetFieldID(resultClass, "rawIntent", "Ljava/lang/String;");

        env->SetObjectField(resultObj, textField, env->NewStringUTF(result.text.c_str()));
        env->SetBooleanField(resultObj, hitField, result.hit);
        env->SetObjectField(resultObj, intentField, env->NewStringUTF(result.intent.c_str()));
        env->SetFloatField(resultObj, confidenceField, result.intent_confidence);
        env->SetObjectField(resultObj, rawIntentField, env->NewStringUTF(result.raw_intent.c_str()));

        // Create slots list
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
        jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

        jobject slotsList = env->NewObject(arrayListClass, arrayListConstructor);

        // Find Slot class
        jclass slotClass = env->FindClass("com/llama/cpp/IntentSlot");
        jmethodID slotConstructor = env->GetMethodID(slotClass, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;)V");

        // Add slots
        for (const auto& slot : result.slots) {
            jstring slotType = env->NewStringUTF(slot.slot_type.c_str());
            jstring slotValue = env->NewStringUTF(slot.slot_value.c_str());

            jobject slotObj = env->NewObject(slotClass, slotConstructor, slotType, slotValue);
            env->CallBooleanMethod(slotsList, arrayListAdd, slotObj);

            env->DeleteLocalRef(slotType);
            env->DeleteLocalRef(slotValue);
            env->DeleteLocalRef(slotObj);
        }

        env->SetObjectField(resultObj, slotsField, slotsList);

        // Cleanup
        env->DeleteLocalRef(arrayListClass);
        env->DeleteLocalRef(slotClass);
        env->DeleteLocalRef(slotsList);
        env->DeleteLocalRef(resultClass);

        if (result.hit) {
            LOGI("Prediction HIT: intent=%s, confidence=%.2f%%",
                 result.intent.c_str(), result.intent_confidence * 100.0f);
        } else {
            LOGI("Prediction NO HIT: confidence=%.2f%% (raw_intent=%s)",
                 result.intent_confidence * 100.0f, result.raw_intent.c_str());
        }

        return resultObj;

    } catch (const std::exception& e) {
        LOGE("Exception during prediction: %s", e.what());
        env->ReleaseStringUTFChars(inputText, input_cstr);
        return nullptr;
    }
}

/**
 * Set confidence threshold
 *
 * @param env JNI environment
 * @param thiz Java object
 * @param threshold New threshold value
 */
JNIEXPORT void JNICALL
Java_com_llama_cpp_IntentRecognizer_nativeSetThreshold(
    JNIEnv* env,
    jobject thiz,
    jfloat threshold) {

    if (g_recognizer) {
        g_recognizer->set_threshold(threshold);
        LOGI("Threshold updated to: %.2f%%", threshold * 100.0f);
    } else {
        LOGE("Recognizer not initialized");
    }
}

/**
 * Get current confidence threshold
 *
 * @param env JNI environment
 * @param thiz Java object
 * @return Current threshold value
 */
JNIEXPORT jfloat JNICALL
Java_com_llama_cpp_IntentRecognizer_nativeGetThreshold(
    JNIEnv* env,
    jobject thiz) {

    if (g_recognizer) {
        return g_recognizer->get_threshold();
    } else {
        LOGE("Recognizer not initialized");
        return 0.0f;
    }
}

/**
 * Release resources
 *
 * @param env JNI environment
 * @param thiz Java object
 */
JNIEXPORT void JNICALL
Java_com_llama_cpp_IntentRecognizer_nativeRelease(
    JNIEnv* env,
    jobject thiz) {

    LOGI("Releasing intent recognizer");
    g_recognizer.reset();
}

/**
 * Check if recognizer is initialized
 *
 * @param env JNI environment
 * @param thiz Java object
 * @return true if initialized, false otherwise
 */
JNIEXPORT jboolean JNICALL
Java_com_llama_cpp_IntentRecognizer_nativeIsInitialized(
    JNIEnv* env,
    jobject thiz) {

    return (g_recognizer && g_recognizer->is_initialized()) ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
