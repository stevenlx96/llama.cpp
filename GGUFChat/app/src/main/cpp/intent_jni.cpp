#include <jni.h>
#include <android/log.h>
#include <string>
#include <memory>
#include <vector>
#include "intent_recognizer.h"

#define INTENT_TAG "IntentJNI"
#define INTENT_LOGI(...) __android_log_print(ANDROID_LOG_INFO, INTENT_TAG, __VA_ARGS__)
#define INTENT_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, INTENT_TAG, __VA_ARGS__)

using namespace intent;

extern "C" {

/**
 * Initialize intent recognizer
 *
 * Java signature: nativeIntentInit(String, Int, Float): Long
 */
JNIEXPORT jlong JNICALL
Java_com_stdemo_ggufchat_IntentRecognizer_nativeIntentInit(
    JNIEnv* env,
    jobject thiz,
    jstring jmodelDir,
    jint numThreads,
    jfloat confidenceThreshold) {

    const char* model_dir = env->GetStringUTFChars(jmodelDir, nullptr);
    if (!model_dir) {
        INTENT_LOGE("Failed to get model directory string");
        return 0;
    }

    INTENT_LOGI("Initializing intent recognizer: dir=%s, threads=%d, threshold=%.2f",
                model_dir, numThreads, confidenceThreshold);

    try {
        // Create config
        IntentConfig config;
        config.model_dir = model_dir;
        config.num_threads = numThreads;
        config.confidence_threshold = confidenceThreshold;
        config.use_gpu = false;

        // Create recognizer
        IntentRecognizer* recognizer = new IntentRecognizer(config);

        // Initialize
        if (!recognizer->initialize()) {
            INTENT_LOGE("Failed to initialize recognizer");
            delete recognizer;
            env->ReleaseStringUTFChars(jmodelDir, model_dir);
            return 0;
        }

        env->ReleaseStringUTFChars(jmodelDir, model_dir);
        INTENT_LOGI("Intent recognizer initialized successfully");

        return reinterpret_cast<jlong>(recognizer);

    } catch (const std::exception& e) {
        INTENT_LOGE("Exception during initialization: %s", e.what());
        env->ReleaseStringUTFChars(jmodelDir, model_dir);
        return 0;
    }
}

/**
 * Predict intent
 *
 * Java signature: nativeIntentPredict(Long, String): IntentResult?
 */
JNIEXPORT jobject JNICALL
Java_com_stdemo_ggufchat_IntentRecognizer_nativeIntentPredict(
    JNIEnv* env,
    jobject thiz,
    jlong contextPtr,
    jstring jtext) {

    IntentRecognizer* recognizer = reinterpret_cast<IntentRecognizer*>(contextPtr);
    if (!recognizer) {
        INTENT_LOGE("Invalid recognizer pointer");
        return nullptr;
    }

    const char* text = env->GetStringUTFChars(jtext, nullptr);
    if (!text) {
        INTENT_LOGE("Failed to get input text");
        return nullptr;
    }

    try {
        // Predict
        PredictionResult result = recognizer->predict(text);
        env->ReleaseStringUTFChars(jtext, text);

        // Create IntentResult object
        jclass resultClass = env->FindClass("com/stdemo/ggufchat/IntentResult");
        if (!resultClass) {
            INTENT_LOGE("Failed to find IntentResult class");
            return nullptr;
        }

        jmethodID constructor = env->GetMethodID(resultClass, "<init>", "()V");
        if (!constructor) {
            INTENT_LOGE("Failed to find IntentResult constructor");
            return nullptr;
        }

        jobject resultObj = env->NewObject(resultClass, constructor);
        if (!resultObj) {
            INTENT_LOGE("Failed to create IntentResult object");
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

        // Add slots
        jclass slotClass = env->FindClass("com/stdemo/ggufchat/IntentSlot");
        jmethodID slotConstructor = env->GetMethodID(slotClass, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;)V");

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

/**
 * Set threshold
 *
 * Java signature: nativeIntentSetThreshold(Long, Float): Void
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

/**
 * Get threshold
 *
 * Java signature: nativeIntentGetThreshold(Long): Float
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

/**
 * Free resources
 *
 * Java signature: nativeIntentFree(Long): Void
 */
JNIEXPORT void JNICALL
Java_com_stdemo_ggufchat_IntentRecognizer_nativeIntentFree(
    JNIEnv* env,
    jobject thiz,
    jlong contextPtr) {

    IntentRecognizer* recognizer = reinterpret_cast<IntentRecognizer*>(contextPtr);
    if (recognizer) {
        delete recognizer;
        INTENT_LOGI("Intent recognizer freed");
    }
}

} // extern "C"
