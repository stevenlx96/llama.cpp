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

// Custom log callback to redirect ggml logs to Android logcat
void ggml_log_callback_android(enum ggml_log_level level, const char * text, void * user_data) {
    (void) user_data;

    // Map ggml log levels to Android log priorities
    int android_priority;
    switch (level) {
        case GGML_LOG_LEVEL_ERROR:
            android_priority = ANDROID_LOG_ERROR;
            break;
        case GGML_LOG_LEVEL_WARN:
            android_priority = ANDROID_LOG_WARN;
            break;
        case GGML_LOG_LEVEL_INFO:
            android_priority = ANDROID_LOG_INFO;
            break;
        case GGML_LOG_LEVEL_DEBUG:
            android_priority = ANDROID_LOG_DEBUG;
            break;
        default:
            android_priority = ANDROID_LOG_VERBOSE;
            break;
    }

    // Remove trailing newline if present (logcat adds its own)
    size_t len = strlen(text);
    if (len > 0 && text[len - 1] == '\n') {
        char * text_copy = strdup(text);
        text_copy[len - 1] = '\0';
        __android_log_write(android_priority, "llama.cpp", text_copy);
        free(text_copy);
    } else {
        __android_log_write(android_priority, "llama.cpp", text);
    }
}

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
// This function finds the longest valid UTF-8 prefix in the given data.
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
            // Invalid lead byte, stop here.
            return boundary;
        }

        if (i + char_len > len) {
            // Not enough bytes left for a full character, stop here.
            return boundary;
        }

        // Check continuation bytes for 2, 3, and 4-byte sequences
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
            // Invalid continuation byte sequence, stop here.
            return boundary;
        }
    }
    return boundary;
}

// Callback function - called when C++ generates a token (only for streaming)
// MODIFIED: Simplified token_callback - the UTF-8 safety check is now in nativeCompletionStreaming.
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

        // Call Java callback
        g_env->CallVoidMethod(g_callback_obj, g_callback_method, jtoken);

        // Check for exceptions
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

    // CRITICAL: Set custom log callback BEFORE llama_backend_init()
    // This redirects BOTH ggml AND llama logs to Android logcat
    // Using llama_log_set() instead of ggml_log_set() ensures we capture
    // llama's own messages including "offloaded X/Y layers to GPU"
    llama_log_set(ggml_log_callback_android, nullptr);
    LOGI("✓ Android logcat callback installed for ggml and llama");

    // 初始化 llama 后端
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
        LOGE("  This is likely because HTP libraries are not accessible to DSP");
    } else {
        LOGE("❌ No usable backend found (neither NPU nor CPU)");
        env->ReleaseStringUTFChars(modelPath, path);
        return 0;
    }

    // 配置模型参数
    LOGI("----------------------------------------");
    LOGI("Loading model with %s backend...", backend_name);

    llama_model_params model_params = llama_model_default_params();

    // EXPERIMENT: Let llama.cpp auto-select devices instead of manual configuration
    // Official docs use D=HTP0 parameter which lets llama.cpp auto-detect
    // Maybe manual device list configuration is preventing HTP0-REPACK from being used?

    // OPTION 1: Auto device selection (llama.cpp default logic)
    // Leave model_params.devices = NULL (default)
    // llama.cpp will automatically discover and configure devices

    // OPTION 2: Manual NPU-only (no CPU in list)
    // static ggml_backend_dev_t devices[2];
    // if (hexagon_dev) {
    //     devices[0] = hexagon_dev;
    //     devices[1] = nullptr;
    //     model_params.devices = devices;
    //     LOGI("Device list: HTP0 only (manual)");
    // }

    // Using OPTION 1 for now
    model_params.devices = nullptr;  // Let llama.cpp auto-select devices
    LOGI("Device selection: AUTO (llama.cpp will use all available GPUs)");

    // CRITICAL: Disable mmap (match official --no-mmap flag)
    // Official Hexagon benchmark explicitly uses --no-mmap for best performance
    model_params.use_mmap = false;
    LOGI("Memory mapping: DISABLED (--no-mmap, matches official config)");

    // CRITICAL: Enable extra buffer types for HTP0-REPACK!
    // Hexagon NPU uses HTP0-REPACK buffer type for weight repacking
    // This is essential for NPU performance - without it, weights stay in CPU memory
    model_params.use_extra_bufts = true;
    LOGI("Extra buffer types: ENABLED (for HTP0-REPACK weight repacking)");

    // CRITICAL FIX: Offload ALL layers to NPU/GPU!
    // -1 means all layers (essential for good performance)
    // Without this, only model weights are on NPU but computation stays on CPU!
    // This was causing the 10x slowdown (CPU<->NPU data transfer overhead)
    model_params.n_gpu_layers = -1;

    LOGI("Model params configured:");
    LOGI("  - Primary device: %s", backend_name);
    LOGI("  - Offloaded layers: ALL (n_gpu_layers = -1)");
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
    int32_t n_layer = llama_model_n_layer(model);

    LOGI("✓ Model loaded successfully on %s", backend_name);
    LOGI("  Vocab size: %d", n_vocab);
    LOGI("  Total layers: %d", n_layer);
    LOGI("  Requested offload: ALL layers (n_gpu_layers = -1)");

    if (hexagon_dev) {
        LOGI("  ⚠ Note: Check logs above for 'offloaded X/%d layers' message", n_layer);
        LOGI("  ⚠ If not all layers offloaded, NPU performance will be poor!");
    }

    // 创建 context
    LOGI("----------------------------------------");
    LOGI("Creating llama context...");

    llama_context_params ctx_params = llama_context_default_params();

    // Start with conservative settings, then optimize incrementally
    // Official config (8192 ctx, 128 batch, FA on) made it WORSE (2 tokens/s vs 10 tokens/s)
    // Trying smaller values optimized for mobile hardware
    ctx_params.n_ctx = 2048;              // Keep original (not 8192)
    ctx_params.n_batch = 512;             // Default value (not 128)
    ctx_params.n_ubatch = 512;            // Match n_batch
    ctx_params.n_threads = nThreads;
    ctx_params.n_threads_batch = nThreads;

    // EXPERIMENT: Disable Flash Attention - it might use operations NPU doesn't support
    // This could be causing the HTP0 -> CPU -> HTP0 splits
    ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_DISABLED;

    // CRITICAL: Enable KV cache offloading to NPU
    // offload_kqv MUST be true for NPU performance
    ctx_params.offload_kqv = true;

    LOGI("Context params (conservative mobile config):");
    LOGI("  - Context size: %d", ctx_params.n_ctx);
    LOGI("  - Batch size: %d", ctx_params.n_batch);
    LOGI("  - Flash Attention: DISABLED (testing if it causes CPU splits)");
    LOGI("  - KV cache offload: %s", ctx_params.offload_kqv ? "ENABLED" : "DISABLED");

    llama_context* ctx = llama_init_from_model(model, ctx_params);

    if (!ctx) {
        LOGE("❌ Failed to create context");
        llama_model_free(model);
        return 0;
    }

    LOGI("✓ Context created");
    LOGI("  Context size: %d tokens", ctx_params.n_ctx);
    LOGI("  Threads: %d", ctx_params.n_threads);

    llama_android_context* android_ctx = new llama_android_context();
    android_ctx->model = model;
    android_ctx->ctx = ctx;

    LOGI("========================================");
    LOGI("✅ Initialization complete (%s)!", backend_name);
    LOGI("========================================");

    return reinterpret_cast<jlong>(android_ctx);
}

bool should_stop_generation(const std::string& generated_text, int token_count) {
    // Stop if too many tokens generated
    if (token_count > 256) {
        LOGD("Stopping: reached max reasonable tokens (%d)", token_count);
        return true;
    }

    // Stop if end marker found
    if (generated_text.find("<|im_end|>") != std::string::npos) {
        LOGD("Stopping: found end token marker");
        return true;
    }

    // Stop if new role marker found (except at beginning)
    size_t first_marker = generated_text.find("<|im_start|>");
    if (first_marker != std::string::npos && first_marker > 10) {
        LOGD("Stopping: found new role marker");
        return true;
    }

    // Check for complete Chinese sentence
    if (token_count > 50 && generated_text.length() > 0) {
        // Check for Chinese period (UTF-8: 0xE3 0x80 0x82)
        if (generated_text.length() >= 3) {
            std::string last_three = generated_text.substr(generated_text.length() - 3);
            if (last_three == "\xe3\x80\x82") {
                if (token_count < 100) {
                    LOGD("Stopping: short complete response at %d tokens", token_count);
                    return true;
                }
            }
        }

        // Check for ASCII punctuation
        char last_char = generated_text.back();
        if (last_char == '.' || last_char == '!' || last_char == '?' || last_char == '\n') {
            if (token_count < 100) {
                LOGD("Stopping: short complete response at %d tokens", token_count);
                return true;
            }
        }
    }

    return false;
}

// ============================================================================
// FUNCTION 1: Static (non-streaming) completion (NO CHANGES)
// ============================================================================
JNIEXPORT jstring JNICALL
Java_com_stdemo_ggufchat_GGUFChatEngine_nativeCompletion(
        JNIEnv* env, jobject thiz,
        jlong contextPtr,
        jstring prompt,
        jint nPredict,
        jfloat temperature,
        jfloat topP,
        jint topK) {

    llama_android_context* android_ctx = reinterpret_cast<llama_android_context*>(contextPtr);
    if (!android_ctx || !android_ctx->ctx || !android_ctx->model) {
        LOGE("Invalid context");
        return env->NewStringUTF("Error: Invalid context");
    }

    const char* prompt_text = env->GetStringUTFChars(prompt, nullptr);
    LOGD("Generating static completion for prompt (length: %zu)", strlen(prompt_text));

    llama_context* ctx = android_ctx->ctx;
    llama_model* model = android_ctx->model;
    const llama_vocab* vocab = llama_model_get_vocab(model);

// Clear KV cache
    llama_memory_seq_rm(llama_get_memory(ctx), -1, 0, -1);
    LOGD("KV Cache cleared");

// Create sampler chain
    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(topK));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(topP, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    LOGD("Sampler created with temp: %.2f, top_p: %.2f, top_k: %d", temperature, topP, topK);

// Tokenize prompt
    std::vector<llama_token> tokens;
    int max_tokens = strlen(prompt_text) + 32;
    tokens.resize(max_tokens);

    int n_tokens = llama_tokenize(
            vocab,
            prompt_text,
            strlen(prompt_text),
            tokens.data(),
            tokens.size(),
            true,
            false
    );

    if (n_tokens < 0) {
        LOGD("Tokenization failed, trying with larger buffer");
        tokens.resize(-n_tokens);
        n_tokens = llama_tokenize(
                vocab,
                prompt_text,
                strlen(prompt_text),
                tokens.data(),
                tokens.size(),
                true,
                false
        );
        if (n_tokens < 0) {
            env->ReleaseStringUTFChars(prompt, prompt_text);
            llama_sampler_free(sampler);
            return env->NewStringUTF("Error: Tokenization failed");
        }
    }

    tokens.resize(n_tokens);
    env->ReleaseStringUTFChars(prompt, prompt_text);

    LOGD("Tokenized to %d tokens", n_tokens);

// Process prompt
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);

    if (llama_decode(ctx, batch) != 0) {
        LOGE("Failed to decode prompt");
        llama_sampler_free(sampler);
        return env->NewStringUTF("Error: Failed to decode prompt");
    }

    LOGD("Prompt decoded, starting static generation");

    std::string result;
    result.reserve(nPredict * 4);

    int generation_token_count = 0;
    const std::string end_marker = "<|im_end|>";
    bool found_end = false;

// Generation loop - no streaming, just collect all tokens
    for (int i = 0; i < nPredict; i++) {
        llama_token new_token = llama_sampler_sample(sampler, ctx, -1);

// Check if end of generation
        if (llama_vocab_is_eog(vocab, new_token)) {
            LOGD("End of generation at token %d", i);
            break;
        }

// Convert token to text
        char buf[256];
        int n = llama_token_to_piece(
                vocab,
                new_token,
                buf,
                sizeof(buf),
                0,
                false
        );

        if (n > 0) {
            result.append(buf, n);
            generation_token_count++;

// Check if we hit end marker
            size_t marker_pos = result.find(end_marker);
            if (marker_pos != std::string::npos) {
                LOGD("Found end marker at pos %zu, truncating", marker_pos);
                result = result.substr(0, marker_pos);
                found_end = true;
                break;
            }
        }

// Check if should stop generation (only if we haven't found end marker)
        if (!found_end && should_stop_generation(result, generation_token_count)) {
            LOGD("Stopping generation early at token %d", i);
            break;
        }

// Decode next token
        if (!found_end) {
            batch = llama_batch_get_one(&new_token, 1);
            if (llama_decode(ctx, batch) != 0) {
                LOGE("Failed to decode token %d", i);
                break;
            }

            llama_sampler_accept(sampler, new_token);
        }
    }

    llama_sampler_free(sampler);

    LOGD("Generated %zu bytes of text (%d tokens)", result.size(), generation_token_count);
    LOGD("Final static result: '%s'", result.c_str());
    return env->NewStringUTF(result.c_str());
}

// ============================================================================
// FUNCTION 2: Streaming completion with token callback (MODIFIED)
// This version uses a buffer to accumulate split UTF-8 tokens before sending.
// ============================================================================
JNIEXPORT jstring JNICALL
Java_com_stdemo_ggufchat_GGUFChatEngine_nativeCompletionStreaming(
        JNIEnv* env, jobject thiz,
        jlong contextPtr,
        jstring prompt,
        jint nPredict,
        jfloat temperature,
        jfloat topP,
        jint topK,
        jobject tokenCallback) {

    llama_android_context* android_ctx = reinterpret_cast<llama_android_context*>(contextPtr);
    if (!android_ctx || !android_ctx->ctx || !android_ctx->model) {
        LOGE("Invalid context");
        return env->NewStringUTF("Error: Invalid context");
    }

// Get callback method ID
    jclass callback_class = env->GetObjectClass(tokenCallback);
    jmethodID callback_method = env->GetMethodID(callback_class, "onToken", "(Ljava/lang/String;)V");
    if (!callback_method) {
        LOGE("Failed to find onToken method");
        env->DeleteLocalRef(callback_class);
        return env->NewStringUTF("Error: Callback method not found");
    }

// Store global callback info (thread-local)
    g_env = env;
    g_callback_obj = env->NewGlobalRef(tokenCallback);
    g_callback_method = callback_method;

    const char* prompt_text = env->GetStringUTFChars(prompt, nullptr);
    LOGD("Generating streaming completion for prompt (length: %zu)", strlen(prompt_text));

    llama_context* ctx = android_ctx->ctx;
    llama_model* model = android_ctx->model;
    const llama_vocab* vocab = llama_model_get_vocab(model);

// Clear KV cache
    llama_memory_seq_rm(llama_get_memory(ctx), -1, 0, -1);
    LOGD("KV Cache cleared");

// Create sampler chain
    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(topK));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(topP, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    LOGD("Sampler created with temp: %.2f, top_p: %.2f, top_k: %d", temperature, topP, topK);

// Tokenize prompt
    std::vector<llama_token> tokens;
    int max_tokens = strlen(prompt_text) + 32;
    tokens.resize(max_tokens);

    int n_tokens = llama_tokenize(
            vocab,
            prompt_text,
            strlen(prompt_text),
            tokens.data(),
            tokens.size(),
            true,
            false
    );

    if (n_tokens < 0) {
        LOGD("Tokenization failed, trying with larger buffer");
        tokens.resize(-n_tokens);
        n_tokens = llama_tokenize(
                vocab,
                prompt_text,
                strlen(prompt_text),
                tokens.data(),
                tokens.size(),
                true,
                false
        );
        if (n_tokens < 0) {
            env->ReleaseStringUTFChars(prompt, prompt_text);
            llama_sampler_free(sampler);
            env->DeleteGlobalRef(g_callback_obj);
            return env->NewStringUTF("Error: Tokenization failed");
        }
    }

    tokens.resize(n_tokens);
    env->ReleaseStringUTFChars(prompt, prompt_text);

    LOGD("Tokenized to %d tokens", n_tokens);

// Process prompt
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);

    if (llama_decode(ctx, batch) != 0) {
        LOGE("Failed to decode prompt");
        llama_sampler_free(sampler);
        env->DeleteGlobalRef(g_callback_obj);
        return env->NewStringUTF("Error: Failed to decode prompt");
    }

    LOGD("Prompt decoded, starting streaming generation");

    std::string total_generated_text; // To store the complete result for final checks/return
    total_generated_text.reserve(nPredict * 4);

    // This buffer accumulates token pieces, especially for split UTF-8 characters and end markers.
    std::string pending_token_buffer;
    int generation_token_count = 0;

// Generation loop - stream tokens as they are generated
    const std::string end_marker = "<|im_end|>";
    bool found_end = false;

    for (int i = 0; i < nPredict; i++) {
        llama_token new_token = llama_sampler_sample(sampler, ctx, -1);

// Check if end of generation
        if (llama_vocab_is_eog(vocab, new_token)) {
            LOGD("End of generation at token %d", i);
            break;
        }

// Convert token to text
        char buf[256];
        int n = llama_token_to_piece(
                vocab,
                new_token,
                buf,
                sizeof(buf),
                0,
                false
        );

        if (n > 0) {
            std::string token_str(buf, n);

// Add token to the pending buffer
            pending_token_buffer.append(token_str);
            total_generated_text.append(token_str); // Keep track of the full output
            generation_token_count++;

// --- UTF-8 and End Marker Handling ---

            // 1. Check for end marker in the accumulated buffer
            size_t marker_pos = pending_token_buffer.find(end_marker);
            if (marker_pos != std::string::npos) {
                LOGD("Found end marker at buffer pos %zu, stopping", marker_pos);

                // Send content before marker
                std::string before_marker = pending_token_buffer.substr(0, marker_pos);
                if (!before_marker.empty()) {
                    LOGD("Sending content before marker: '%s'", before_marker.c_str());
                    token_callback(before_marker);
                }

                found_end = true;
                break;
            }

            // 2. Determine how much of the buffer is a complete, valid UTF-8 sequence
            size_t boundary = find_utf8_boundary(pending_token_buffer.c_str(), pending_token_buffer.length());

            // 3. Send the complete part if applicable
            if (boundary > 0) {
                // Check if the tail (incomplete part) might be the start of the end_marker
                std::string complete_part = pending_token_buffer.substr(0, boundary);
                std::string tail = pending_token_buffer.substr(boundary);

                bool is_marker_prefix = false;
                if (!tail.empty()) {
                    for (size_t j = 1; j < end_marker.length(); j++) {
                        if (tail == end_marker.substr(0, tail.length())) {
                            is_marker_prefix = true;
                            LOGD("Tail matches %zu-char prefix of marker, holding full token.", j);
                            break;
                        }
                    }
                } else {
                    // Check if the WHOLE token is a marker prefix (e.g. "<")
                    for (size_t j = 1; j < end_marker.length(); j++) {
                        if (complete_part == end_marker.substr(0, complete_part.length())) {
                            is_marker_prefix = true;
                            LOGD("Full token matches %zu-char prefix of marker, holding.", j);
                            break;
                        }
                    }
                }

                if (!is_marker_prefix) {
                    // If it's a complete UTF-8 sequence and not a prefix of the end marker, stream it out.
                    LOGD("Sending complete part: '%s'", complete_part.c_str());
                    token_callback(complete_part);
                    pending_token_buffer = tail; // Keep the tail (which is incomplete UTF-8 or empty)
                }
            }
        }

// Only continue decoding if we haven't found end marker
        if (!found_end) {
// Decode next token
            batch = llama_batch_get_one(&new_token, 1);
            if (llama_decode(ctx, batch) != 0) {
                LOGE("Failed to decode token %d", i);
                break;
            }

            llama_sampler_accept(sampler, new_token);
        }
    }

// Flush remaining buffer (any remaining complete text or valid marker prefix)
    if (!pending_token_buffer.empty()) {
        size_t marker_pos = pending_token_buffer.find(end_marker);

        if (marker_pos != std::string::npos) {
            // Only send content before the marker, and truncate total_generated_text
            std::string before_marker = pending_token_buffer.substr(0, marker_pos);
            if (!before_marker.empty()) {
                LOGD("Final flush buffer before marker: '%s'", before_marker.c_str());
                token_callback(before_marker);
            }
        } else {
            // Send whatever is left (e.g., a final sentence or punctuation)
            LOGD("Final flush buffer: '%s'", pending_token_buffer.c_str());
            token_callback(pending_token_buffer);
        }
    }

    llama_sampler_free(sampler);

    LOGD("Generated %zu bytes of text (%d tokens)", total_generated_text.size(), generation_token_count);

// Clean up global callback
    env->DeleteGlobalRef(g_callback_obj);
    g_env = nullptr;
    g_callback_obj = nullptr;
    g_callback_method = nullptr;

    // Return the total generated text (or a placeholder)
    return env->NewStringUTF(total_generated_text.c_str());
}

JNIEXPORT void JNICALL
Java_com_stdemo_ggufchat_GGUFChatEngine_nativeFree(
        JNIEnv* env, jobject thiz, jlong contextPtr) {

    llama_android_context* android_ctx = reinterpret_cast<llama_android_context*>(contextPtr);
    if (android_ctx) {
        if (android_ctx->ctx) {
            llama_free(android_ctx->ctx);
            LOGD("Context freed");
        }
        if (android_ctx->model) {
            llama_model_free(android_ctx->model);
            LOGD("Model freed");
        }
        delete android_ctx;
    }

    llama_backend_free();
}

}  // extern "C"