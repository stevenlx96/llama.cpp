#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>
#include "llama.h"
#include "ggml-backend.h"
#include "ggml-hexagon.h"

#define TAG "LlamaJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

struct llama_android_context {
    llama_model* model;
    llama_context* ctx;
};

// Global variables to store Java callback (only used for streaming)
thread_local JNIEnv* g_env = nullptr;
thread_local jobject g_callback_obj = nullptr;
thread_local jmethodID g_callback_method = nullptr;

// CRITICAL FIX: Validate UTF-8 encoding to prevent JNI crashes
bool is_valid_utf8(const char* data, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)data[i];
        if (c < 0x80) {
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len) return false;
            unsigned char c2 = (unsigned char)data[i + 1];
            if ((c2 & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len) return false;
            unsigned char c2 = (unsigned char)data[i + 1];
            unsigned char c3 = (unsigned char)data[i + 2];
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len) return false;
            unsigned char c2 = (unsigned char)data[i + 1];
            unsigned char c3 = (unsigned char)data[i + 2];
            unsigned char c4 = (unsigned char)data[i + 3];
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

// Find last complete UTF-8 character boundary
size_t find_utf8_boundary(const char* data, size_t len) {
    if (len == 0) return 0;
    size_t boundary = 0;
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)data[i];
        size_t char_len = 0;

        if (c < 0x80) {
            char_len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            char_len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            char_len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            char_len = 4;
        } else {
            return boundary;
        }

        if (i + char_len > len) {
            return boundary;
        }

        bool valid = true;
        for (size_t j = 1; j < char_len; j++) {
            if (((unsigned char)data[i + j] & 0xC0) != 0x80) {
                valid = false;
                break;
            }
        }

        if (valid) {
            boundary = i + char_len;
            i += char_len;
        } else {
            return boundary;
        }
    }
    return boundary;
}

// Callback function - called when C++ generates a token (only for streaming)
void token_callback(const std::string& token) {
    if (g_env && g_callback_obj && g_callback_method) {
        if (token.empty()) {
            return;
        }

        jstring jtoken = g_env->NewStringUTF(token.c_str());
        if (!jtoken) {
            LOGE("token_callback: Failed to create jstring");
            return;
        }

        g_env->CallVoidMethod(g_callback_obj, g_callback_method, jtoken);

        if (g_env->ExceptionCheck()) {
            LOGE("token_callback: Exception in Java callback");
            g_env->ExceptionClear();
        }

        g_env->DeleteLocalRef(jtoken);
    } else {
        LOGE("token_callback: callback not set");
    }
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_stdemo_ggufchat_GGUFChatEngine_nativeInit(
        JNIEnv* env, jobject thiz, jstring modelPath, jint nThreads) {

    const char* path = env->GetStringUTFChars(modelPath, nullptr);
    LOGI("========================================");
    LOGI("🚀 GGUFChat Hexagon NPU Initialization");
    LOGI("========================================");
    LOGI("Model path: %s", path);
    LOGI("Threads: %d", nThreads);

    // NOTE: Environment variables (ADSP_LIBRARY_PATH, CDSP_LIBRARY_PATH)
    // are set on the Kotlin side in GGUFChatEngine init{} block
    // BEFORE System.loadLibrary() - this is the correct timing!
    // DO NOT set them here - it's too late for DSP initialization

    // 初始化 llama 后端
    LOGI("----------------------------------------");
    llama_backend_init();
    LOGI("✓ llama backend initialized");

    // 列出所有可用的后端设备
    LOGI("----------------------------------------");
    LOGI("Listing available backends...");

    size_t dev_count = ggml_backend_dev_count();
    LOGI("Total backend devices: %zu", dev_count);

    ggml_backend_dev_t hexagon_dev = nullptr;
    ggml_backend_dev_t cpu_dev = nullptr;

    // 安全地遍历所有设备，查找 Hexagon 和 CPU backend
    for (size_t i = 0; i < dev_count; i++) {
        ggml_backend_dev_t dev = ggml_backend_dev_get(i);
        if (!dev) {
            LOGE("  [%zu] NULL device (skipping)", i);
            continue;
        }

        const char* dev_name = ggml_backend_dev_name(dev);
        const char* dev_desc = ggml_backend_dev_description(dev);

        if (!dev_name || !dev_desc) {
            LOGE("  [%zu] Invalid device name/description (skipping)", i);
            continue;
        }

        LOGI("  [%zu] %s - %s", i, dev_name, dev_desc);

        // 查找 Hexagon NPU (HTP0 或包含 "Hexagon" 的设备)
        if (strstr(dev_name, "HTP") != nullptr || strstr(dev_name, "Hexagon") != nullptr) {
            hexagon_dev = dev;
            LOGI("    → Found Hexagon NPU!");
        }

        // 查找 CPU backend
        if (strstr(dev_name, "CPU") != nullptr) {
            cpu_dev = dev;
            LOGI("    → Found CPU backend");
        }
    }

    // 决定使用哪个设备
    LOGI("----------------------------------------");
    ggml_backend_dev_t primary_dev = nullptr;
    const char* backend_name = "Unknown";

    if (hexagon_dev) {
        primary_dev = hexagon_dev;
        backend_name = "Hexagon NPU";
        LOGI("✓ Using Hexagon NPU as primary device");

        // 获取 NPU 内存信息
        size_t npu_mem_free = 0, npu_mem_total = 0;
        ggml_backend_dev_memory(hexagon_dev, &npu_mem_free, &npu_mem_total);
        LOGI("  Memory: %.2f MB free / %.2f MB total",
             npu_mem_free / (1024.0 * 1024.0),
             npu_mem_total / (1024.0 * 1024.0));
    } else if (cpu_dev) {
        primary_dev = cpu_dev;
        backend_name = "CPU";
        LOGE("⚠ Hexagon NPU not available, falling back to CPU");
        LOGE("  Check that ADSP_LIBRARY_PATH and CDSP_LIBRARY_PATH are set correctly");
        LOGE("  in GGUFChatEngine init{} block (should use semicolon ; as separator)");
    } else {
        LOGE("❌ No usable backend found (neither NPU nor CPU)");
        env->ReleaseStringUTFChars(modelPath, path);
        return 0;
    }

    // 配置模型参数
    LOGI("----------------------------------------");
    LOGI("Loading model with %s backend...", backend_name);

    llama_model_params model_params = llama_model_default_params();

    // 创建设备列表（NULL 结尾）
    static ggml_backend_dev_t devices[2];
    devices[0] = primary_dev;
    devices[1] = nullptr;
    model_params.devices = devices;

    LOGI("Model params configured:");
    LOGI("  - Primary device: %s", backend_name);
    LOGI("  - CPU fallback: %s", hexagon_dev ? "disabled (NPU only)" : "N/A (using CPU)");

    // 加载模型
    llama_model* model = llama_model_load_from_file(path, model_params);
    env->ReleaseStringUTFChars(modelPath, path);

    if (!model) {
        LOGE("❌ Failed to load model on %s backend", backend_name);
        return 0;
    }

    const llama_vocab* vocab = llama_model_get_vocab(model);
    int32_t n_vocab = llama_vocab_n_tokens(vocab);
    LOGI("✓ Model loaded successfully on %s", backend_name);
    LOGI("  Vocab size: %d", n_vocab);

    // 创建 context
    LOGI("----------------------------------------");
    LOGI("Creating llama context...");

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;
    ctx_params.n_threads = nThreads;
    ctx_params.n_threads_batch = nThreads;

    llama_context* ctx = llama_new_context_with_model(model, ctx_params);
    if (!ctx) {
        LOGE("❌ Failed to create llama context");
        llama_model_free(model);
        return 0;
    }

    LOGI("✓ Context created");
    LOGI("  Context size: %d tokens", ctx_params.n_ctx);
    LOGI("  Threads: %d", nThreads);
    LOGI("========================================");
    LOGI("✅ Initialization complete (%s)!", backend_name);
    LOGI("========================================");

    // 封装到结构体
    llama_android_context* android_ctx = new llama_android_context;
    android_ctx->model = model;
    android_ctx->ctx = ctx;

    return reinterpret_cast<jlong>(android_ctx);
}

// ... rest of the file remains the same ...
// (Include all the other native functions: nativeCompletion, nativeCompletionStreaming, nativeFree)

} // extern "C"
