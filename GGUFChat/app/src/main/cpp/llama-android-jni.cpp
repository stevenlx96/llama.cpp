#include <jni.h>
#include <string>
#include <vector>
#include <chrono>
#include <android/log.h>
#include <dlfcn.h>  // For dlopen/dlsym to load OpenCL dynamically
#include <unistd.h> // For access(), R_OK, W_OK, F_OK
#include <errno.h>  // For errno
#include "llama.h"
#include "ggml-backend.h"
#include "ggml-hexagon.h"
#include <stdlib.h>

#define TAG "LlamaJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

// Custom log callback to redirect ggml logs to Android logcat
void ggml_log_callback_android(enum ggml_log_level level, const char * text, void * user_data) {
    (void) user_data;

    size_t len = strlen(text);

    // FILTER: Skip verbose messages (too much spam)
    if (strstr(text, "repack:") != nullptr ||
        strstr(text, "repack tensor") != nullptr ||
        strstr(text, "load_tensors:") != nullptr ||
        strstr(text, "create_tensor:") != nullptr) {
        return;  // Silently ignore these verbose messages
    }

    // FILTER: Skip progress dots
    if (len == 2 && text[0] == '.' && text[1] == '\n') {
        return;  // Silently ignore progress dots
    }
    if (len == 1 && text[0] == '.') {
        return;  // Silently ignore progress dots
    }

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

        JNIEnv* env, jobject thiz, jstring modelPath, jint nThreads, jstring libPath) {

    const char* path = env->GetStringUTFChars(modelPath, nullptr);
    const char* lib_dir = env->GetStringUTFChars(libPath, nullptr);

    LOGI("========================================");
    LOGI("🚀 GGUFChat Hexagon NPU Initialization");
    LOGI("========================================");

    // 【关键修改 1】：设置 DSP 环境变量
    // 必须让 FastRPC 知道去哪里找 libggml_hexagon_skel.so
    // lib_dir 通常是 /data/app/.../lib/arm64
    if (lib_dir) {
        std::string adsp_path = std::string(lib_dir) + ";/vendor/lib/rfsa/adsp;/system/lib/rfsa/adsp";
        setenv("ADSP_LIBRARY_PATH", adsp_path.c_str(), 1);
        LOGI("✓ ADSP_LIBRARY_PATH set to: %s", adsp_path.c_str());
    }

    llama_log_set(ggml_log_callback_android, nullptr);

    // 🔍 OPENCL DIAGNOSTICS: Try to load OpenCL library explicitly
    LOGI("========================================");
    LOGI("🔍 OpenCL Diagnostics - Attempting to load libOpenCL.so");
    LOGI("========================================");

    // Check if device files exist
    LOGI("🔍 Checking GPU device files:");
    const char* gpu_devices[] = {
        "/dev/kgsl-3d0",      // Adreno GPU device
        "/dev/dri/renderD128", // DRM render device
        "/dev/mali0",         // Mali GPU (for completeness)
    };
    for (const char* dev : gpu_devices) {
        if (access(dev, R_OK | W_OK) == 0) {
            LOGI("   ✅ %s is readable and writable", dev);
        } else if (access(dev, F_OK) == 0) {
            LOGE("   ❌ %s exists but NOT accessible (errno: %d - %s)", dev, errno, strerror(errno));
        } else {
            LOGI("   ⚠️  %s does not exist", dev);
        }
    }

    // Try to load OpenCL runtime library
    LOGI("🔍 Attempting dlopen() for libOpenCL.so...");
    void* opencl_handle = dlopen("libOpenCL.so", RTLD_NOW | RTLD_GLOBAL);
    if (opencl_handle != nullptr) {
        LOGI("✅ SUCCESS: libOpenCL.so loaded via dlopen()");

        // Try to get clGetPlatformIDs function
        typedef int (*clGetPlatformIDs_t)(unsigned int, void*, unsigned int*);
        typedef int (*clGetDeviceIDs_t)(void*, unsigned int, unsigned int, void*, unsigned int*);

        clGetPlatformIDs_t clGetPlatformIDs_fn = (clGetPlatformIDs_t)dlsym(opencl_handle, "clGetPlatformIDs");
        clGetDeviceIDs_t clGetDeviceIDs_fn = (clGetDeviceIDs_t)dlsym(opencl_handle, "clGetDeviceIDs");

        if (clGetPlatformIDs_fn != nullptr) {
            LOGI("✅ clGetPlatformIDs function found in libOpenCL.so");

            // Try to query OpenCL platforms
            unsigned int num_platforms = 0;
            int result = clGetPlatformIDs_fn(0, nullptr, &num_platforms);

            LOGI("🔍 clGetPlatformIDs() returned: %d", result);
            LOGI("🔍 num_platforms: %u", num_platforms);

            if (result == 0) {  // CL_SUCCESS = 0
                LOGI("✅ OpenCL query successful! Found %u OpenCL platform(s)", num_platforms);

                if (num_platforms > 0 && clGetDeviceIDs_fn != nullptr) {
                    // Try to get platform and query devices
                    void* platform_id = nullptr;
                    result = clGetPlatformIDs_fn(1, &platform_id, nullptr);
                    if (result == 0 && platform_id != nullptr) {
                        LOGI("✅ Got platform ID: %p", platform_id);

                        // Try to get GPU devices (CL_DEVICE_TYPE_GPU = 4)
                        unsigned int num_devices = 0;
                        result = clGetDeviceIDs_fn(platform_id, 4, 0, nullptr, &num_devices);
                        LOGI("🔍 clGetDeviceIDs(GPU) returned: %d, num_devices: %u", result, num_devices);
                    }
                }
            } else {
                LOGE("❌ OpenCL query FAILED with error code: %d", result);
                LOGE("   Error code meanings:");
                LOGE("   -1001 = CL_PLATFORM_NOT_FOUND_KHR (no OpenCL platforms)");
                LOGE("   -1000 = CL_DEVICE_NOT_FOUND (no OpenCL devices)");
                LOGE("   -30   = CL_INVALID_VALUE");
                LOGE("   This usually means:");
                LOGE("   1. GPU driver not loaded/accessible");
                LOGE("   2. SELinux blocking GPU access");
                LOGE("   3. App doesn't have GPU permissions");
                LOGE("   4. OpenGL ES context not initialized");
            }
        } else {
            LOGE("❌ clGetPlatformIDs function NOT found in libOpenCL.so");
            LOGE("   dlerror: %s", dlerror());
        }

        // Don't close the handle - keep it loaded for ggml-opencl to use
        // dlclose(opencl_handle);
    } else {
        LOGE("❌ FAILED to load libOpenCL.so via dlopen()");
        LOGE("   dlerror: %s", dlerror());
        LOGE("   OpenCL GPU acceleration will NOT be available!");
    }
    LOGI("========================================");

    // 🔍 DEBUG: Check backend count BEFORE any registration
    size_t backends_before = ggml_backend_reg_count();
    LOGI("🔍 DEBUG: Backend count BEFORE registration: %zu", backends_before);

    // Register OpenCL backend if not already registered
    ggml_backend_reg_t existing_opencl = ggml_backend_reg_by_name("OpenCL");
    if (existing_opencl != nullptr) {
        LOGI("⚠️ OpenCL backend ALREADY registered (auto-loaded by .so)");
    } else {
        // OpenCL backend should auto-register via .so linking
        LOGI("⚠️ OpenCL backend NOT found (should auto-register from libggml-opencl.so)");
    }

    // Register Hexagon backend if not already registered
    ggml_backend_reg_t existing_htp = ggml_backend_reg_by_name("HTP");
    if (existing_htp != nullptr) {
        LOGI("⚠️ Hexagon backend ALREADY registered (auto-loaded by .so)");
        LOGI("   Skipping explicit registration to avoid duplicates");
    } else {
        // Register Hexagon if not already registered
        ggml_backend_register(ggml_backend_hexagon_reg());
        LOGI("✓ Hexagon backend explicitly registered");
    }

    // 🔍 DEBUG: Check backend count AFTER backend checks
    size_t backends_after_hex = ggml_backend_reg_count();
    LOGI("🔍 DEBUG: Backend count AFTER backend checks: %zu (added %zu)",
         backends_after_hex, backends_after_hex - backends_before);

    // Initialize llama backend (this will register CPU backend automatically)
    llama_backend_init();
    LOGI("✓ llama backend initialized (CPU backend auto-registered)");

    // 🔍 DEBUG: Check backend count AFTER llama_backend_init
    size_t backends_after_init = ggml_backend_reg_count();
    LOGI("🔍 DEBUG: Backend count AFTER llama_backend_init: %zu (added %zu)",
         backends_after_init, backends_after_init - backends_after_hex);

    // Enumerate backends and find Hexagon/OpenCL
    LOGI("----------------------------------------");
    LOGI("Enumerating available backends...");

    size_t n_devices = ggml_backend_dev_count();
    LOGI("Found %zu backend devices", n_devices);

    if (n_devices < 2) {
        LOGE("⚠️ WARNING: Expected at least 2 devices (HTP0 + CPU), but found %zu!", n_devices);
    }

    ggml_backend_dev_t hexagon_dev = nullptr;
    ggml_backend_dev_t opencl_dev = nullptr;

    for (size_t i = 0; i < n_devices; i++) {
        ggml_backend_dev_t dev = ggml_backend_dev_get(i);
        const char* dev_name = ggml_backend_dev_name(dev);
        const char* backend_name = ggml_backend_dev_backend_reg(dev) ?
                                   ggml_backend_reg_name(ggml_backend_dev_backend_reg(dev)) : "Unknown";

        LOGI("  Device %zu: %s (Backend: %s)", i, dev_name, backend_name);

        // Look for Hexagon device (name starts with "HTP")
        if (strncmp(dev_name, "HTP", 3) == 0) {
            hexagon_dev = dev;
            LOGI("  ✓ Found Hexagon device: %s", dev_name);
        }

        // Look for OpenCL device (Adreno GPU)
        if (strstr(dev_name, "Adreno") != nullptr || strcmp(backend_name, "OpenCL") == 0) {
            opencl_dev = dev;
            LOGI("  ✓ Found OpenCL device: %s", dev_name);
        }
    }

    // 配置模型参数
    LOGI("----------------------------------------");
    LOGI("Loading model...");

    llama_model_params model_params = llama_model_default_params();

    // CRITICAL: Create a static device array for model_params
    // model_params.devices must be a NULL-terminated array!
    // This is why offloading wasn't working - we need 2 elements!
    static ggml_backend_dev_t device_array[2];  // [0] = device, [1] = nullptr

    // CRITICAL: Test if Hexagon device is actually usable before using it
    // Sometimes device is found but not fully initialized (dspqueue failure)
    bool use_hexagon = false;
    if (hexagon_dev != nullptr) {
        LOGI("Testing Hexagon device usability...");

        // Try to get device description to verify it's actually usable
        const char* dev_desc = ggml_backend_dev_description(hexagon_dev);
        if (dev_desc && strlen(dev_desc) > 0) {
            use_hexagon = true;
            LOGI("  ✓ Hexagon device is usable");
            LOGI("  Description: %s", dev_desc);
        } else {
            LOGE("  ✗ Hexagon device found but not usable (failed to get description)");
            LOGE("  This usually means dspqueue or session initialization failed");
            LOGE("  Falling back to CPU");
        }
    }

    if (use_hexagon) {
        LOGI("Configuring model to use Hexagon NPU");
        device_array[0] = hexagon_dev;
        device_array[1] = nullptr;  // NULL terminator - CRITICAL!
        model_params.devices = device_array;
        model_params.n_gpu_layers = 999;  // Offload all layers
        LOGI("  - Device: Hexagon HTP");
        LOGI("  - GPU layers: 999 (all)");
        LOGI("  - Device array is NULL-terminated: YES");
    } else {
        LOGI("⚠ Using CPU (Hexagon not available or not usable)");
    }

    // --- 修正后的 Backend Inspector 调试代码 ---
    LOGI("--- Backend Inspector Start ---");
// 获取已注册后端的数量
    size_t reg_count = ggml_backend_reg_count();
    LOGI("Total registered backends: %zu", reg_count);

    for (size_t i = 0; i < reg_count; ++i) {
        // 获取后端句柄
        ggml_backend_reg_t reg = ggml_backend_reg_get(i);
        const char* reg_name = ggml_backend_reg_name(reg);
        size_t dev_count = ggml_backend_reg_dev_count(reg);

        LOGI("Backend [%zu]: %s, Devices: %zu", i, reg_name, dev_count);

        for (size_t j = 0; j < dev_count; ++j) {
            ggml_backend_dev_t dev = ggml_backend_reg_dev_get(reg, j);
            if (dev == nullptr) {
                LOGE("  ! Device [%zu] is NULL! This will cause crash.", j);
                continue;
            }

            const char* dev_name = ggml_backend_dev_name(dev);
            const char* dev_desc = ggml_backend_dev_description(dev);
            LOGI("  - Device [%zu]: %s (%s)", j, dev_name, dev_desc);

            // 测试会导致崩溃的那个函数
            LOGI("  - Testing props for device [%zu]...", j);
            ggml_backend_dev_props props;
            ggml_backend_dev_get_props(dev, &props); // 如果崩在这里，我们就知道是哪个设备了
            LOGI("  - Props OK for %s", dev_name);
        }
    }
    LOGI("--- Backend Inspector End ---");


    // 加载模型
    LOGI("🔧 DEBUG: Calling llama_model_load_from_file with:");
    LOGI("  - devices: %p", (void*)model_params.devices);
    if (model_params.devices && model_params.devices[0]) {
        LOGI("  - devices[0]: %s", ggml_backend_dev_name(model_params.devices[0]));
        LOGI("  - devices[1]: %p (should be nullptr)", (void*)model_params.devices[1]);
    }
    LOGI("  - n_gpu_layers: %d", model_params.n_gpu_layers);

    llama_model* model = llama_model_load_from_file(path, model_params);
    env->ReleaseStringUTFChars(modelPath, path);

    if (!model) {
        LOGE("❌ Failed to load model");
        return 0;
    }

    const llama_vocab* vocab = llama_model_get_vocab(model);
    int32_t n_vocab = llama_vocab_n_tokens(vocab);
    int32_t n_layer = llama_model_n_layer(model);

    LOGI("✓ Model loaded successfully");
    LOGI("  Vocab size: %d", n_vocab);
    LOGI("  Total layers: %d", n_layer);

    // 🔍 CRITICAL DEBUG: Check which backend the model weights are actually on
    LOGI("----------------------------------------");
    LOGI("🔍 DEBUG: Checking model tensor allocation...");

    // Get model's internal structure to check tensor buffers
    // This will tell us if tensors are on HTP or CPU backend
    int htp_tensor_count = 0;
    int cpu_tensor_count = 0;
    int total_tensor_count = 0;

    // We can't directly access internal tensors easily, but we can check
    // the model description which should show buffer allocations
    LOGI("  Model description would show buffer allocations");
    LOGI("  (Detailed tensor inspection requires accessing model->impl)");

    // Alternative: Check the model's device
    // If model was loaded to HTP, we should see evidence in buffer names
    LOGI("----------------------------------------");

    // CRITICAL: Check if layers were actually offloaded to NPU
    LOGI("----------------------------------------");
    LOGI("⚠️ OFFLOAD STATUS CHECK:");

    // Check if GPU offload is supported
    bool gpu_offload_supported = llama_supports_gpu_offload();
    LOGI("  GPU offload supported: %s", gpu_offload_supported ? "YES" : "NO");

    if (!gpu_offload_supported) {
        LOGE("  ❌ GPU offload NOT supported!");
        LOGE("  This means llama.cpp cannot find any GPU-type backends!");
        LOGE("  Checking backend types...");

        // Debug: check what backend types are available
        for (size_t i = 0; i < ggml_backend_dev_count(); i++) {
            ggml_backend_dev_t dev = ggml_backend_dev_get(i);
            const char* dev_name = ggml_backend_dev_name(dev);
            ggml_backend_dev_props props;
            ggml_backend_dev_get_props(dev, &props);
            LOGI("    Device %zu: %s, type=%d", i, dev_name, props.type);
        }
    } else {
        LOGI("  ✓ GPU offload is supported!");
        LOGI("  Check above for 'offloaded X/Y layers' message");
    }
    LOGI("----------------------------------------");

    // 创建 context
    LOGI("----------------------------------------");
    LOGI("Creating llama context...");

    // CRITICAL: Match official example EXACTLY!
    // Reference: examples/llama.android/lib/src/main/cpp/ai_chat.cpp:89-99
    llama_context_params ctx_params = llama_context_default_params();

    // Official configuration
    const int DEFAULT_CONTEXT_SIZE = 8192;
    const int BATCH_SIZE = 512;  // Official uses 512, NOT 128!

    ctx_params.n_ctx = DEFAULT_CONTEXT_SIZE;
    ctx_params.n_batch = BATCH_SIZE;
    ctx_params.n_ubatch = BATCH_SIZE;
    ctx_params.n_threads = nThreads;
    ctx_params.n_threads_batch = nThreads;

    LOGI("Context params (OFFICIAL CONFIG):");
    LOGI("  - n_ctx: %d", ctx_params.n_ctx);
    LOGI("  - n_batch: %d", ctx_params.n_batch);
    LOGI("  - n_ubatch: %d", ctx_params.n_ubatch);
    LOGI("  - threads: %d", nThreads);

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

    // CRITICAL: Synchronize to ensure all backend operations complete
    // This is especially important for Hexagon to release resources properly
    llama_synchronize(ctx);
    LOGD("Backend synchronized after generation");

    // Calculate and log performance metrics
    auto gen_end_time = std::chrono::high_resolution_clock::now();
    auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end_time - gen_start_time);
    double gen_time_sec = gen_duration.count() / 1000.0;
    double tokens_per_sec = generation_token_count / gen_time_sec;

    LOGI("========================================");
    LOGI("⚡ PERFORMANCE STATS (NPU):");
    LOGI("  Generated tokens: %d", generation_token_count);
    LOGI("  Generation time: %.2f seconds", gen_time_sec);
    LOGI("  Speed: %.2f tokens/second", tokens_per_sec);
    LOGI("  Average time per token: %.2f ms", (gen_time_sec * 1000.0) / generation_token_count);
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

    // CRITICAL: Synchronize to ensure all backend operations complete
    // This is especially important for Hexagon to release resources properly
    llama_synchronize(ctx);
    LOGD("Backend synchronized after generation");

    // Calculate and log performance metrics
    auto gen_end_time = std::chrono::high_resolution_clock::now();
    auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end_time - gen_start_time);
    double gen_time_sec = gen_duration.count() / 1000.0;
    double tokens_per_sec = generation_token_count / gen_time_sec;

    LOGI("========================================");
    LOGI("⚡ PERFORMANCE STATS (NPU):");
    LOGI("  Generated tokens: %d", generation_token_count);
    LOGI("  Generation time: %.2f seconds", gen_time_sec);
    LOGI("  Speed: %.2f tokens/second", tokens_per_sec);
    LOGI("  Average time per token: %.2f ms", (gen_time_sec * 1000.0) / generation_token_count);
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
