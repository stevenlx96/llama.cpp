#include <jni.h>
#include <string>
#include <vector>
#include <chrono>
#include <android/log.h>
#include <EGL/egl.h>  // For EGL context initialization (required for OpenCL on Android)
#include "llama.h"
#include "ggml-backend.h"
#include <stdlib.h>

#define TAG "LlamaJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

// Custom log callback to redirect ggml logs to Android logcat
void ggml_log_callback_android(enum ggml_log_level level, const char * text, void * user_data) {
    (void) user_data;

    size_t len = strlen(text);

    // ============================================================
    // FILTER: Skip verbose/repetitive messages (reduce log spam)
    // ============================================================

    // FILTER 1: Skip repetitive KV cache layer messages
    if (strstr(text, "llama_kv_cache: layer") != nullptr &&
        strstr(text, "dev =") != nullptr) {
        return;
    }

    // FILTER 3: Skip verbose repack progress messages
    if (strstr(text, "repack:") != nullptr ||
        strstr(text, "repack tensor") != nullptr ||
        strstr(text, "create_tensor:") != nullptr) {
        return;  // Skip repack progress spam
    }

    // FILTER 4: Skip individual control token debug messages (keep summary only)
    if (strstr(text, "control token:") != nullptr &&
        strstr(text, "is not marked as EOG") != nullptr) {
        return;  // Skip individual control token warnings
    }

    // FILTER 5: Skip detailed model loader key-value pairs (too verbose)
    if (strstr(text, "llama_model_loader: - kv") != nullptr ||
        strstr(text, "llama_model_loader: - type") != nullptr) {
        return;  // Skip individual KV pair dumps
    }

    // FILTER 6: Skip graph reserve debug messages
    if (strstr(text, "graph_reserve:") != nullptr) {
        return;  // Skip graph reservation details
    }

    // FILTER 7: Skip detailed print_info lines (keep summary only)
    if (strstr(text, "print_info:") != nullptr &&
        (strstr(text, "n_embd") != nullptr ||
         strstr(text, "n_head") != nullptr ||
         strstr(text, "n_expert") != nullptr ||
         strstr(text, "rope") != nullptr ||
         strstr(text, "f_norm") != nullptr ||
         strstr(text, "f_clamp") != nullptr ||
         strstr(text, "f_max_alibi") != nullptr ||
         strstr(text, "f_logit") != nullptr ||
         strstr(text, "f_attn") != nullptr ||
         strstr(text, "n_rot") != nullptr ||
         strstr(text, "n_swa") != nullptr ||
         strstr(text, "is_swa") != nullptr ||
         strstr(text, "n_gqa") != nullptr ||
         strstr(text, "causal attn") != nullptr ||
         strstr(text, "pooling type") != nullptr ||
         strstr(text, "rope type") != nullptr ||
         strstr(text, "freq_") != nullptr ||
         strstr(text, "n_ctx_orig") != nullptr)) {
        return;  // Skip verbose architecture details
    }

    // FILTER 8: Skip token-related verbose info
    if (strstr(text, "EOG token        =") != nullptr ||
        strstr(text, "FIM") != nullptr ||
        strstr(text, "token to piece cache") != nullptr) {
        return;  // Skip token detail spam
    }

    // FILTER 9: Skip backend enumeration spam
    if (strstr(text, "llama_context: enumerating backends") != nullptr ||
        strstr(text, "llama_context: backend_ptrs.size()") != nullptr ||
        strstr(text, "llama_context: max_nodes") != nullptr ||
        strstr(text, "llama_context: reserving") != nullptr ||
        strstr(text, "llama_context: worst-case") != nullptr) {
        return;  // Skip backend enumeration details
    }

    // FILTER 10: Skip async upload messages
    if (strstr(text, "load_all_data:") != nullptr) {
        return;  // Skip async upload details
    }

    // FILTER 11: Skip progress dots
    if (len == 2 && text[0] == '.' && text[1] == '\n') {
        return;  // Skip progress dots
    }
    if (len == 1 && text[0] == '.') {
        return;  // Skip progress dots
    }

    // ============================================================
    // ALLOW THROUGH: Critical diagnostic information
    // ============================================================

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
    if (len > 0 && text[len - 1] == '\n') {
        char * text_copy = strdup(text);
        text_copy[len - 1] = '\0';
        __android_log_write(android_priority, "llama.cpp", text_copy);
        free(text_copy);
    } else {
        __android_log_write(android_priority, "llama.cpp", text);
    }
}

// ============================================================
// EGL Context Initialization - Required for OpenCL on Android
// ============================================================

// Many Android devices (especially Qualcomm) require an EGL context
// to be initialized before OpenCL can be used. This is a Qualcomm-specific
// requirement where OpenCL shares resources with OpenGL ES.
static EGLDisplay g_egl_display = EGL_NO_DISPLAY;
static EGLContext g_egl_context = EGL_NO_CONTEXT;
static EGLSurface g_egl_surface = EGL_NO_SURFACE;

static bool init_egl_for_opencl() {
    LOGI("========================================");
    LOGI("Initializing EGL Context for OpenCL");
    LOGI("========================================");

    // Get default display
    g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_egl_display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed: 0x%x", eglGetError());
        return false;
    }

    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(g_egl_display, &major, &minor)) {
        LOGE("eglInitialize failed: 0x%x", eglGetError());
        return false;
    }
    LOGI("EGL initialized: version %d.%d", major, minor);

    // Choose config for OpenGL ES 3.0
    const EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,  // Use pbuffer (offscreen)
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(g_egl_display, config_attribs, &config, 1, &num_configs)) {
        LOGE("eglChooseConfig failed: 0x%x", eglGetError());
        eglTerminate(g_egl_display);
        g_egl_display = EGL_NO_DISPLAY;
        return false;
    }

    if (num_configs == 0) {
        LOGE("No suitable EGL config found");
        eglTerminate(g_egl_display);
        g_egl_display = EGL_NO_DISPLAY;
        return false;
    }
    LOGI("EGL config chosen");

    // Create pbuffer surface (1x1 offscreen surface)
    const EGLint surface_attribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    g_egl_surface = eglCreatePbufferSurface(g_egl_display, config, surface_attribs);
    if (g_egl_surface == EGL_NO_SURFACE) {
        LOGE("eglCreatePbufferSurface failed: 0x%x", eglGetError());
        eglTerminate(g_egl_display);
        g_egl_display = EGL_NO_DISPLAY;
        return false;
    }
    LOGI("EGL pbuffer surface created");

    // Create OpenGL ES 3.0 context
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,  // OpenGL ES 3.0
        EGL_NONE
    };
    g_egl_context = eglCreateContext(g_egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (g_egl_context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed: 0x%x", eglGetError());
        eglDestroySurface(g_egl_display, g_egl_surface);
        eglTerminate(g_egl_display);
        g_egl_display = EGL_NO_DISPLAY;
        g_egl_surface = EGL_NO_SURFACE;
        return false;
    }
    LOGI("EGL context created (OpenGL ES 3.0)");

    // Make context current
    if (!eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface, g_egl_context)) {
        LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
        eglDestroyContext(g_egl_display, g_egl_context);
        eglDestroySurface(g_egl_display, g_egl_surface);
        eglTerminate(g_egl_display);
        g_egl_display = EGL_NO_DISPLAY;
        g_egl_surface = EGL_NO_SURFACE;
        g_egl_context = EGL_NO_CONTEXT;
        return false;
    }

    LOGI("EGL context made current");
    LOGI("========================================");
    LOGI("EGL initialization complete!");
    LOGI("OpenCL should now be able to access GPU");
    LOGI("========================================");
    return true;
}

static void cleanup_egl() {
    if (g_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(g_egl_display, g_egl_context);
        }
        if (g_egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_egl_display, g_egl_surface);
        }
        eglTerminate(g_egl_display);
    }
    g_egl_display = EGL_NO_DISPLAY;
    g_egl_context = EGL_NO_CONTEXT;
    g_egl_surface = EGL_NO_SURFACE;
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

// ============================================================================
// NOTE: CPU affinity configuration
// ============================================================================
// The official tool uses: --cpu-mask 0xfc --cpu-strict 1 --poll 1000
// This is handled internally by llama.cpp's threadpool when n_threads is set.
// We rely on the library's default threadpool behavior.

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_stdemo_ggufchat_GGUFChatEngine_nativeInit(
        JNIEnv* env, jobject thiz, jstring modelPath, jint nThreads, jstring libPath, jstring dspLibPath) {
    (void)thiz;  // Unused parameter (standard JNI pattern)

    const char* path = env->GetStringUTFChars(modelPath, nullptr);

    LOGI("========================================");
    LOGI("GGUFChat with NPU/GPU Acceleration");
    LOGI("========================================");

    // Set log callback for llama.cpp
    llama_log_set(ggml_log_callback_android, nullptr);

    // CRITICAL: Initialize EGL context FIRST (required for OpenCL on Qualcomm Adreno GPU)
    // This must be done before loading backends to allow OpenCL to detect GPU devices
    if (!init_egl_for_opencl()) {
        LOGE("EGL initialization failed - OpenCL GPU acceleration may not be available");
    }

    // Get paths
    const char* nativeLibPath = env->GetStringUTFChars(libPath, nullptr);
    const char* dspPath = env->GetStringUTFChars(dspLibPath, nullptr);

    LOGI("Native library path: %s", nativeLibPath);
    LOGI("DSP library path: %s", dspPath);

    // CRITICAL: Set ADSP_LIBRARY_PATH to the external storage directory
    // where HTP skel libraries were copied. The DSP can access external storage
    // but NOT the app's private /data/app/ directory.
    LOGI("Setting Hexagon DSP environment variables...");

    // Build search path: DSP external dir first, then fallback paths
    std::string dspSearchPath = std::string(dspPath) + ";" +
                                std::string(nativeLibPath) +
                                ";/vendor/dsp/cdsp;/vendor/lib/rfsa/adsp;/system/lib/rfsa/adsp;/dsp";
    setenv("ADSP_LIBRARY_PATH", dspSearchPath.c_str(), 1);
    LOGI("ADSP_LIBRARY_PATH = %s", dspSearchPath.c_str());

    // Set LD_LIBRARY_PATH for the stub libraries on Android side
    setenv("LD_LIBRARY_PATH", nativeLibPath, 1);

    // Load all backends from native library path
    ggml_backend_load_all_from_path(nativeLibPath);

    env->ReleaseStringUTFChars(libPath, nativeLibPath);
    env->ReleaseStringUTFChars(dspLibPath, dspPath);
    LOGI("Backends loaded dynamically");

    // Initialize llama backend
    llama_backend_init();
    LOGI("llama backend initialized");

    // Load model with optimized parameters
    LOGI("Loading model: %s", path);

    llama_model_params model_params = llama_model_default_params();

    // CRITICAL: Use single device mode to avoid splitting across CPU/GPU/NPU
    // This prevents the 264+ graph splits that cause massive overhead
    model_params.split_mode = LLAMA_SPLIT_MODE_NONE;
    model_params.n_gpu_layers = 99;  // Offload all layers to GPU (OpenCL)
    LOGI("Model params: split_mode=NONE, n_gpu_layers=99 (all to GPU)");

    llama_model* model = llama_model_load_from_file(path, model_params);
    env->ReleaseStringUTFChars(modelPath, path);

    if (!model) {
        LOGE("Failed to load model");
        return 0;
    }

    const llama_vocab* vocab = llama_model_get_vocab(model);
    int32_t n_vocab = llama_vocab_n_tokens(vocab);
    int32_t n_layer = llama_model_n_layer(model);

    LOGI("Model loaded successfully");
    LOGI("  Vocab size: %d", n_vocab);
    LOGI("  Total layers: %d", n_layer);

    // Create context with official parameters
    // Reference: examples/llama.android/lib/src/main/cpp/ai_chat.cpp:89-99
    LOGI("----------------------------------------");
    LOGI("Creating llama context...");

    llama_context_params ctx_params = llama_context_default_params();

    // Configuration matching official command line tool
    const int DEFAULT_CONTEXT_SIZE = 8192;
    const int BATCH_SIZE = 128;  // Official uses 128, not 512!

    ctx_params.n_ctx = DEFAULT_CONTEXT_SIZE;
    ctx_params.n_batch = BATCH_SIZE;
    ctx_params.n_ubatch = BATCH_SIZE;
    ctx_params.n_threads = nThreads;
    ctx_params.n_threads_batch = nThreads;

    // CRITICAL: Enable Flash Attention for better performance
    ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;

    LOGI("Context params (matching official CLI):");
    LOGI("  - n_ctx: %d", ctx_params.n_ctx);
    LOGI("  - n_batch: %d", ctx_params.n_batch);
    LOGI("  - n_ubatch: %d", ctx_params.n_ubatch);
    LOGI("  - threads: %d", nThreads);
    LOGI("  - flash_attn: ENABLED");

    llama_context* ctx = llama_init_from_model(model, ctx_params);

    if (!ctx) {
        LOGE("❌ Failed to create context");
        llama_model_free(model);
        return 0;
    }

    LOGI("✓ Context created successfully");

    llama_android_context* android_ctx = new llama_android_context();
    android_ctx->model = model;
    android_ctx->ctx = ctx;

    LOGI("========================================");
    LOGI("✅ Initialization complete!");
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
    (void)thiz;  // Unused parameter (standard JNI pattern)

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

    // Start timing for performance measurement
    auto gen_start_time = std::chrono::high_resolution_clock::now();

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

    // Synchronize to ensure all backend operations complete
    llama_synchronize(ctx);
    LOGD("Backend synchronized after generation");

    // Calculate and log performance metrics
    auto gen_end_time = std::chrono::high_resolution_clock::now();
    auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end_time - gen_start_time);
    double gen_time_sec = gen_duration.count() / 1000.0;
    double tokens_per_sec = generation_token_count / gen_time_sec;

    LOGI("========================================");
    LOGI("⚡ PERFORMANCE STATS:");
    LOGI("  Generated tokens: %d", generation_token_count);
    LOGI("  Generation time: %.2f seconds", gen_time_sec);
    LOGI("  Speed: %.2f tokens/second", tokens_per_sec);
    LOGI("  Average time per token: %.2f ms", (gen_time_sec * 1000.0) / generation_token_count);
    LOGI("========================================");

    // 🔍 CRITICAL DEBUG: Print backend usage statistics
    LOGI("========================================");
    LOGI("🔍 BACKEND USAGE STATISTICS:");
    LOGI("========================================");

    // Get performance statistics from llama.cpp
    // This will show which backends were actually used during inference
    struct llama_perf_context_data perf = llama_perf_context(ctx);

    LOGI("📊 Context Performance:");
    LOGI("  - Prompt eval time: %.2f ms", perf.t_p_eval_ms);
    LOGI("  - Prompt eval count: %d", perf.n_p_eval);
    LOGI("  - Token eval time: %.2f ms", perf.t_eval_ms);
    LOGI("  - Token eval count: %d", perf.n_eval);
    LOGI("  - Total time: %.2f ms", perf.t_p_eval_ms + perf.t_eval_ms);

    if (perf.n_eval > 0) {
        double avg_token_time = perf.t_eval_ms / perf.n_eval;
        LOGI("  - Average per token: %.2f ms (%.2f tokens/s)",
             avg_token_time, 1000.0 / avg_token_time);
    }

    LOGI("========================================");

    LOGD("Generated %zu bytes of text (%d tokens)", result.size(), generation_token_count);
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
    (void)thiz;  // Unused parameter (standard JNI pattern)

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

// Process prompt in batches (critical for prompts larger than n_batch)
    // This prevents crashes when prompt exceeds n_batch size (e.g., 355 tokens > 128 batch size)
    int n_batch = llama_n_batch(ctx);
    LOGD("Processing prompt in batches (n_batch=%d, n_tokens=%d)", n_batch, n_tokens);

    for (int i = 0; i < n_tokens; i += n_batch) {
        int n_eval = std::min(n_batch, n_tokens - i);
        llama_batch batch = llama_batch_get_one(tokens.data() + i, n_eval);

        if (llama_decode(ctx, batch) != 0) {
            LOGE("Failed to decode prompt batch [%d-%d]", i, i + n_eval - 1);
            llama_sampler_free(sampler);
            env->DeleteGlobalRef(g_callback_obj);
            return env->NewStringUTF("Error: Failed to decode prompt");
        }

        LOGD("Decoded prompt batch [%d-%d]", i, i + n_eval - 1);
    }

    LOGD("Prompt decoded, starting streaming generation");

    std::string total_generated_text; // To store the complete result for final checks/return
    total_generated_text.reserve(nPredict * 4);

    // This buffer accumulates token pieces, especially for split UTF-8 characters and end markers.
    std::string pending_token_buffer;
    int generation_token_count = 0;

    // Start timing for performance measurement
    auto gen_start_time = std::chrono::high_resolution_clock::now();

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
        // Simple stop check: limit to 256 tokens for stability
        if (generation_token_count >= 256) {
            LOGD("Stopping: reached 256 token limit");
            break;
        }

// Only continue decoding if we haven't found end marker
        if (!found_end) {
// Decode next token
            llama_batch batch = llama_batch_get_one(&new_token, 1);
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

    // Synchronize to ensure all backend operations complete
    llama_synchronize(ctx);
    LOGD("Backend synchronized after generation");

    // Calculate and log performance metrics
    auto gen_end_time = std::chrono::high_resolution_clock::now();
    auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end_time - gen_start_time);
    double gen_time_sec = gen_duration.count() / 1000.0;
    double tokens_per_sec = generation_token_count / gen_time_sec;

    LOGI("========================================");
    LOGI("⚡ PERFORMANCE STATS:");
    LOGI("  Generated tokens: %d", generation_token_count);
    LOGI("  Generation time: %.2f seconds", gen_time_sec);
    LOGI("  Speed: %.2f tokens/second", tokens_per_sec);
    LOGI("  Average time per token: %.2f ms", (gen_time_sec * 1000.0) / generation_token_count);
    LOGI("========================================");

    // 🔍 CRITICAL DEBUG: Print backend usage statistics
    LOGI("========================================");
    LOGI("🔍 BACKEND USAGE STATISTICS:");
    LOGI("========================================");

    // Get performance statistics from llama.cpp
    // This will show which backends were actually used during inference
    struct llama_perf_context_data perf = llama_perf_context(ctx);

    LOGI("📊 Context Performance:");
    LOGI("  - Prompt eval time: %.2f ms", perf.t_p_eval_ms);
    LOGI("  - Prompt eval count: %d", perf.n_p_eval);
    LOGI("  - Token eval time: %.2f ms", perf.t_eval_ms);
    LOGI("  - Token eval count: %d", perf.n_eval);
    LOGI("  - Total time: %.2f ms", perf.t_p_eval_ms + perf.t_eval_ms);

    if (perf.n_eval > 0) {
        double avg_token_time = perf.t_eval_ms / perf.n_eval;
        LOGI("  - Average per token: %.2f ms (%.2f tokens/s)",
             avg_token_time, 1000.0 / avg_token_time);
    }

    LOGI("========================================");

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
    (void)env;   // Unused parameter (standard JNI pattern)
    (void)thiz;  // Unused parameter (standard JNI pattern)

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

    // Cleanup EGL resources
    cleanup_egl();
    LOGD("EGL resources cleaned up");
}

}  // extern "C"
