/**
 * ============================================================================
 * llama-android-jni.cpp - LLM 推理核心文件 (JNI 桥接层)
 * ============================================================================
 *
 * 【文件作用】
 * 这个文件是 Android 应用和 llama.cpp C++ 库之间的"桥梁"。
 * JNI (Java Native Interface) 允许 Kotlin/Java 代码调用 C++ 函数。
 *
 * 【工作流程图】
 *
 *   Kotlin 代码 (LlamaEngine.kt)
 *         │
 *         ▼
 *   ┌─────────────────────────────┐
 *   │  JNI 桥接层 (本文件)          │  ◀── 你现在看的这个文件
 *   │  - 接收 Kotlin 的调用         │
 *   │  - 转换数据格式               │
 *   │  - 调用 llama.cpp 库          │
 *   │  - 把结果返回给 Kotlin        │
 *   └─────────────────────────────┘
 *         │
 *         ▼
 *   llama.cpp 库 (libllama.so)
 *         │
 *         ▼
 *   GPU/NPU 加速 (OpenCL/Hexagon)
 *
 * 【主要功能】
 * 1. nativeInit()     - 初始化模型（加载 .gguf 文件）
 * 2. nativeCompletion() - 生成回复（非流式，一次性返回）
 * 3. nativeCompletionStreaming() - 流式生成（边生成边返回）
 * 4. nativeFree()     - 释放资源
 *
 * 【给 Python 开发者的说明】
 * - C++ 中的 #include 相当于 Python 的 import
 * - extern "C" 是告诉编译器用 C 的方式处理函数名
 * - JNIEnv* env 是 JNI 环境指针，用于和 Java 交互
 * - jstring, jlong, jint 等是 Java 类型在 C++ 中的表示
 *
 * ============================================================================
 */

// ============================================================================
// 头文件引入部分 (相当于 Python 的 import)
// ============================================================================

#include <jni.h>           // JNI 核心库 - 让 C++ 能和 Java/Kotlin 通信
#include <string>          // C++ 字符串类
#include <vector>          // C++ 动态数组 (类似 Python 的 list)
#include <chrono>          // 时间测量库 (用于计算推理速度)
#include <android/log.h>   // Android 日志库 (输出到 logcat)
#include <EGL/egl.h>       // EGL 库 - OpenCL GPU 加速需要用到
#include "llama.h"         // llama.cpp 主库
#include "ggml-backend.h"  // ggml 后端管理 (CPU/GPU/NPU 切换)
#include <stdlib.h>        // 标准库 (setenv 等)

// ============================================================================
// 日志宏定义 - 简化日志输出
// ============================================================================
// 这些宏让你可以用 LOGD("消息") 代替冗长的 __android_log_print(...)
// 类似于 Python 中定义一个简单的 log 函数

#define TAG "LlamaJNI"  // 日志标签，在 logcat 中过滤用

// LOGD = Debug 级别日志 (调试信息)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

// LOGE = Error 级别日志 (错误信息)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// LOGI = Info 级别日志 (一般信息)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

// ============================================================================
// 日志过滤回调函数
// ============================================================================
/**
 * 【功能】自定义日志回调，过滤掉 llama.cpp 输出的大量冗余信息
 *
 * 【为什么需要这个？】
 * llama.cpp 库会输出非常多的调试信息（比如每一层的 KV cache 信息），
 * 这些信息会让 logcat 变得很乱，很难找到有用的信息。
 * 这个函数就像一个"过滤器"，只让重要的信息通过。
 *
 * 【参数说明】
 * @param level     - 日志级别 (DEBUG/INFO/WARN/ERROR)
 * @param text      - 日志内容
 * @param user_data - 用户数据 (这里不使用)
 */
void ggml_log_callback_android(enum ggml_log_level level, const char * text, void * user_data) {
    (void) user_data;  // 标记参数未使用，避免编译器警告

    size_t len = strlen(text);  // 获取日志文本长度

    // ============================================================
    // 过滤规则：跳过不需要的日志消息
    // ============================================================
    // strstr(a, b) 检查字符串 a 中是否包含字符串 b
    // 如果包含返回位置指针，不包含返回 nullptr

    // 过滤 1: 跳过 KV cache 层信息 (每层都会输出，太多了)
    if (strstr(text, "llama_kv_cache: layer") != nullptr &&
        strstr(text, "dev =") != nullptr) {
        return;  // 直接返回，不输出这条日志
    }

    // 过滤 3: 跳过 repack 进度信息 (模型加载时的中间过程)
    if (strstr(text, "repack:") != nullptr ||
        strstr(text, "repack tensor") != nullptr ||
        strstr(text, "create_tensor:") != nullptr) {
        return;
    }

    // 过滤 4: 跳过 control token 的单独警告 (只保留汇总信息)
    if (strstr(text, "control token:") != nullptr &&
        strstr(text, "is not marked as EOG") != nullptr) {
        return;
    }

    // 过滤 5: 跳过模型加载器的键值对详情 (太冗长)
    if (strstr(text, "llama_model_loader: - kv") != nullptr ||
        strstr(text, "llama_model_loader: - type") != nullptr) {
        return;
    }

    // 过滤 6: 跳过 graph reserve 调试信息
    if (strstr(text, "graph_reserve:") != nullptr) {
        return;
    }

    // 过滤 7: 跳过详细的模型架构参数 (n_embd, n_head 等)
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
        return;
    }

    // 过滤 8: 跳过 token 相关的冗余信息
    if (strstr(text, "EOG token        =") != nullptr ||
        strstr(text, "FIM") != nullptr ||
        strstr(text, "token to piece cache") != nullptr) {
        return;
    }

    // 过滤 9: 跳过 backend 枚举信息
    if (strstr(text, "llama_context: enumerating backends") != nullptr ||
        strstr(text, "llama_context: backend_ptrs.size()") != nullptr ||
        strstr(text, "llama_context: max_nodes") != nullptr ||
        strstr(text, "llama_context: reserving") != nullptr ||
        strstr(text, "llama_context: worst-case") != nullptr) {
        return;
    }

    // 过滤 10: 跳过异步上传信息
    if (strstr(text, "load_all_data:") != nullptr) {
        return;
    }

    // 过滤 11: 跳过进度点 (加载时显示的 "..." )
    if (len == 2 && text[0] == '.' && text[1] == '\n') {
        return;
    }
    if (len == 1 && text[0] == '.') {
        return;
    }

    // ============================================================
    // 通过过滤的日志：转换级别并输出到 Android logcat
    // ============================================================

    // 把 ggml 的日志级别映射到 Android 的日志级别
    int android_priority;
    switch (level) {
        case GGML_LOG_LEVEL_ERROR:
            android_priority = ANDROID_LOG_ERROR;   // 红色错误
            break;
        case GGML_LOG_LEVEL_WARN:
            android_priority = ANDROID_LOG_WARN;    // 黄色警告
            break;
        case GGML_LOG_LEVEL_INFO:
            android_priority = ANDROID_LOG_INFO;    // 蓝色信息
            break;
        case GGML_LOG_LEVEL_DEBUG:
            android_priority = ANDROID_LOG_DEBUG;   // 绿色调试
            break;
        default:
            android_priority = ANDROID_LOG_VERBOSE; // 默认详细
            break;
    }

    // 移除末尾的换行符 (logcat 会自动添加换行)
    if (len > 0 && text[len - 1] == '\n') {
        char * text_copy = strdup(text);     // 复制字符串
        text_copy[len - 1] = '\0';           // 替换换行符为结束符
        __android_log_write(android_priority, "llama.cpp", text_copy);
        free(text_copy);                     // 释放内存
    } else {
        __android_log_write(android_priority, "llama.cpp", text);
    }
}

// ============================================================================
// EGL 上下文初始化 - OpenCL GPU 加速的前置条件
// ============================================================================
/**
 * 【重要背景知识】
 * 在 Android 上使用 OpenCL 进行 GPU 加速时，尤其是高通 Adreno GPU，
 * 必须先初始化一个 EGL 上下文。这是高通的特殊要求。
 *
 * 【什么是 EGL？】
 * EGL 是 OpenGL ES 和原生窗口系统之间的接口。
 * 简单理解：EGL 帮助 GPU 准备好工作环境。
 *
 * 【为什么需要这个？】
 * 高通 Adreno GPU 的 OpenCL 实现和 OpenGL ES 共享资源，
 * 如果不先初始化 EGL，OpenCL 可能无法正确检测到 GPU。
 */

// 全局变量：存储 EGL 上下文信息
// static 表示这些变量只在本文件内可见
static EGLDisplay g_egl_display = EGL_NO_DISPLAY;  // EGL 显示连接
static EGLContext g_egl_context = EGL_NO_CONTEXT;  // EGL 渲染上下文
static EGLSurface g_egl_surface = EGL_NO_SURFACE;  // EGL 绘图表面

/**
 * 【函数】初始化 EGL 上下文
 *
 * 【返回值】
 * - true: 初始化成功
 * - false: 初始化失败
 */
static bool init_egl_for_opencl() {
    LOGI("========================================");
    LOGI("Initializing EGL Context for OpenCL");
    LOGI("========================================");

    // 步骤 1: 获取默认显示设备
    g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_egl_display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed: 0x%x", eglGetError());
        return false;
    }

    // 步骤 2: 初始化 EGL
    EGLint major, minor;  // 用于存储 EGL 版本号
    if (!eglInitialize(g_egl_display, &major, &minor)) {
        LOGE("eglInitialize failed: 0x%x", eglGetError());
        return false;
    }
    LOGI("EGL initialized: version %d.%d", major, minor);

    // 步骤 3: 选择 EGL 配置
    // 这些配置参数定义了我们需要的 EGL 环境特性
    const EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,  // 需要 OpenGL ES 3.0 支持
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,        // 使用离屏渲染 (不显示到屏幕)
        EGL_BLUE_SIZE, 8,                         // 蓝色通道 8 位
        EGL_GREEN_SIZE, 8,                        // 绿色通道 8 位
        EGL_RED_SIZE, 8,                          // 红色通道 8 位
        EGL_ALPHA_SIZE, 8,                        // 透明度通道 8 位
        EGL_DEPTH_SIZE, 0,                        // 不需要深度缓冲
        EGL_NONE                                  // 配置结束标记
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

    // 步骤 4: 创建 PBuffer 表面 (1x1 像素的离屏渲染表面)
    // 我们只需要一个最小的表面来让 EGL 上下文工作
    const EGLint surface_attribs[] = {
        EGL_WIDTH, 1,    // 宽度 1 像素
        EGL_HEIGHT, 1,   // 高度 1 像素
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

    // 步骤 5: 创建 OpenGL ES 3.0 上下文
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,  // OpenGL ES 版本 3.0
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

    // 步骤 6: 将上下文设为当前上下文
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

/**
 * 【函数】清理 EGL 资源
 *
 * 在应用退出或释放模型时调用，释放 EGL 占用的资源
 */
static void cleanup_egl() {
    if (g_egl_display != EGL_NO_DISPLAY) {
        // 取消当前上下文
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        // 销毁上下文
        if (g_egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(g_egl_display, g_egl_context);
        }

        // 销毁表面
        if (g_egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_egl_display, g_egl_surface);
        }

        // 终止 EGL
        eglTerminate(g_egl_display);
    }

    // 重置为初始值
    g_egl_display = EGL_NO_DISPLAY;
    g_egl_context = EGL_NO_CONTEXT;
    g_egl_surface = EGL_NO_SURFACE;
}

// ============================================================================
// LLaMA Android 上下文结构体
// ============================================================================
/**
 * 【结构体】llama_android_context
 *
 * 用于保存 LLaMA 模型和上下文的指针。
 * 这个结构体会被转换为 jlong (64位整数) 传递给 Kotlin 层，
 * Kotlin 层再传回来时，我们把它转换回指针。
 *
 * 【类比 Python】
 * 类似于 Python 中的:
 * class LlamaContext:
 *     def __init__(self):
 *         self.model = None
 *         self.ctx = None
 */
struct llama_android_context {
    llama_model* model;    // 模型指针 - 包含模型权重
    llama_context* ctx;    // 上下文指针 - 包含推理状态 (KV cache 等)
};

// ============================================================================
// 全局回调变量 (用于流式输出)
// ============================================================================
/**
 * thread_local 关键字表示这些变量是线程本地的，
 * 每个线程都有自己的一份副本，避免多线程冲突。
 */
thread_local JNIEnv* g_env = nullptr;           // JNI 环境指针
thread_local jobject g_callback_obj = nullptr;  // Java 回调对象
thread_local jmethodID g_callback_method = nullptr;  // Java 回调方法 ID

// ============================================================================
// UTF-8 编码验证和处理函数
// ============================================================================
/**
 * 【为什么需要这些函数？】
 *
 * 中文和其他非 ASCII 字符在 UTF-8 编码中可能占用 2-4 个字节。
 * LLM 生成的 token 有时候会把一个中文字符拆成多个 token，
 * 导致某个 token 只包含一个中文字符的一部分字节。
 *
 * 如果直接把这种不完整的字节序列传给 JNI 的 NewStringUTF()，
 * 会导致崩溃！
 *
 * 【UTF-8 编码规则】
 * - ASCII (英文字母、数字): 1 字节，格式 0xxxxxxx
 * - 欧洲字符等: 2 字节，格式 110xxxxx 10xxxxxx
 * - 中文等: 3 字节，格式 1110xxxx 10xxxxxx 10xxxxxx
 * - Emoji 等: 4 字节，格式 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 */

/**
 * 【函数】检查数据是否是有效的 UTF-8 编码
 *
 * @param data - 要检查的数据
 * @param len  - 数据长度
 * @return true 如果是有效的 UTF-8，false 如果不是
 */
bool is_valid_utf8(const char* data, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)data[i];

        if (c < 0x80) {
            // ASCII 字符 (0-127)，单字节
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            // 2 字节 UTF-8 序列的开头 (110xxxxx)
            if (i + 1 >= len) return false;  // 不够 2 个字节
            unsigned char c2 = (unsigned char)data[i + 1];
            if ((c2 & 0xC0) != 0x80) return false;  // 第二字节格式不对 (应该是 10xxxxxx)
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3 字节 UTF-8 序列的开头 (1110xxxx)
            if (i + 2 >= len) return false;
            unsigned char c2 = (unsigned char)data[i + 1];
            unsigned char c3 = (unsigned char)data[i + 2];
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4 字节 UTF-8 序列的开头 (11110xxx)
            if (i + 3 >= len) return false;
            unsigned char c2 = (unsigned char)data[i + 1];
            unsigned char c3 = (unsigned char)data[i + 2];
            unsigned char c4 = (unsigned char)data[i + 3];
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            // 无效的 UTF-8 起始字节
            return false;
        }
    }
    return true;
}

/**
 * 【函数】找到最后一个完整 UTF-8 字符的边界位置
 *
 * 这个函数返回可以安全截断的位置，保证截断后的字符串是有效的 UTF-8。
 *
 * 【示例】
 * 假设 "你好" 的 UTF-8 编码是:
 * 你: E4 BD A0 (3字节)
 * 好: E5 A5 BD (3字节)
 *
 * 如果 token 只给了 "E4 BD A0 E5"，
 * 这个函数会返回 3，表示只有前 3 个字节是完整的 "你"。
 *
 * @param data - 数据
 * @param len  - 数据长度
 * @return 最后一个完整 UTF-8 字符的结束位置
 */
size_t find_utf8_boundary(const char* data, size_t len) {
    if (len == 0) return 0;

    size_t boundary = 0;  // 记录最后一个完整字符的结束位置
    size_t i = 0;

    while (i < len) {
        unsigned char c = (unsigned char)data[i];
        size_t char_len = 0;

        // 根据首字节判断这个字符应该有几个字节
        if (c < 0x80) {
            char_len = 1;  // ASCII
        } else if ((c & 0xE0) == 0xC0) {
            char_len = 2;  // 2 字节字符
        } else if ((c & 0xF0) == 0xE0) {
            char_len = 3;  // 3 字节字符 (中文)
        } else if ((c & 0xF8) == 0xF0) {
            char_len = 4;  // 4 字节字符 (Emoji)
        } else {
            // 无效的起始字节，停止
            return boundary;
        }

        // 检查是否有足够的字节
        if (i + char_len > len) {
            // 字节不够，说明这个字符不完整
            return boundary;
        }

        // 检查后续字节是否都是 10xxxxxx 格式
        bool valid = true;
        for (size_t j = 1; j < char_len; j++) {
            if (((unsigned char)data[i + j] & 0xC0) != 0x80) {
                valid = false;
                break;
            }
        }

        if (valid) {
            boundary = i + char_len;  // 更新边界
            i += char_len;
        } else {
            return boundary;
        }
    }
    return boundary;
}

// ============================================================================
// Token 回调函数 (用于流式输出)
// ============================================================================
/**
 * 【函数】将生成的 token 发送给 Kotlin 回调
 *
 * 这个函数在每次生成一个 token 后被调用，
 * 把生成的文本片段发送给 Kotlin 层显示。
 *
 * @param token - 生成的 token 文本
 */
void token_callback(const std::string& token) {
    // 检查回调是否已设置
    if (g_env && g_callback_obj && g_callback_method) {
        if (token.empty()) {
            return;  // 空 token 不发送
        }

        // 创建 Java 字符串
        jstring jtoken = g_env->NewStringUTF(token.c_str());
        if (!jtoken) {
            LOGE("token_callback: Failed to create jstring");
            return;
        }

        // 调用 Java 回调方法: callback.onToken(token)
        g_env->CallVoidMethod(g_callback_obj, g_callback_method, jtoken);

        // 检查是否有异常
        if (g_env->ExceptionCheck()) {
            LOGE("token_callback: Exception in Java callback");
            g_env->ExceptionClear();
        }

        // 释放本地引用
        g_env->DeleteLocalRef(jtoken);
    } else {
        LOGE("token_callback: callback not set");
    }
}

// ============================================================================
// 注意: CPU 亲和性配置
// ============================================================================
// 官方工具使用: --cpu-mask 0xfc --cpu-strict 1 --poll 1000
// 这是由 llama.cpp 的线程池在设置 n_threads 时内部处理的。
// 我们依赖库的默认线程池行为。

// ============================================================================
// JNI 函数定义开始
// ============================================================================
/**
 * extern "C" 告诉编译器用 C 语言的方式处理这些函数名，
 * 这样 JNI 才能正确找到这些函数。
 */
extern "C" {

// ============================================================================
// 函数 1: nativeInit - 初始化模型
// ============================================================================
/**
 * 【函数】初始化 LLM 模型
 *
 * 【Kotlin 调用示例】
 * val contextPtr = nativeInit(modelPath, nThreads, libPath, dspLibPath)
 *
 * 【参数说明】
 * @param env        - JNI 环境指针 (自动传入)
 * @param thiz       - 调用此方法的 Java 对象 (自动传入)
 * @param modelPath  - 模型文件路径 (.gguf 文件)
 * @param nThreads   - 使用的 CPU 线程数
 * @param libPath    - Native 库路径 (.so 文件所在目录)
 * @param dspLibPath - DSP 库路径 (用于 Hexagon NPU)
 *
 * @return 上下文指针 (成功) 或 0 (失败)
 */
JNIEXPORT jlong JNICALL
Java_com_stdemo_ggufchat_GGUFChatEngine_nativeInit(
        JNIEnv* env, jobject thiz, jstring modelPath, jint nThreads, jstring libPath, jstring dspLibPath) {

    (void)thiz;  // 标记参数未使用

    // 将 Java 字符串转换为 C 字符串
    const char* path = env->GetStringUTFChars(modelPath, nullptr);

    LOGI("========================================");
    LOGI("GGUFChat with NPU/GPU Acceleration");
    LOGI("========================================");

    // 设置日志回调，过滤 llama.cpp 的冗余输出
    llama_log_set(ggml_log_callback_android, nullptr);

    // 【关键步骤】初始化 EGL 上下文
    // 这必须在加载后端之前完成，否则 OpenCL 可能检测不到 GPU
    if (!init_egl_for_opencl()) {
        LOGE("EGL initialization failed - OpenCL GPU acceleration may not be available");
    }

    // 获取路径字符串
    const char* nativeLibPath = env->GetStringUTFChars(libPath, nullptr);
    const char* dspPath = env->GetStringUTFChars(dspLibPath, nullptr);

    LOGI("Native library path: %s", nativeLibPath);
    LOGI("DSP library path: %s", dspPath);

    // 【Hexagon NPU 配置】
    // 设置 ADSP_LIBRARY_PATH 环境变量，让 DSP 能找到 skel 库
    // DSP 可以访问外部存储，但不能访问 app 的私有目录 /data/app/
    LOGI("Setting Hexagon DSP environment variables...");

    // 构建搜索路径：DSP 外部目录优先，然后是备用路径
    std::string dspSearchPath = std::string(dspPath) + ";" +
                                std::string(nativeLibPath) +
                                ";/vendor/dsp/cdsp;/vendor/lib/rfsa/adsp;/system/lib/rfsa/adsp;/dsp";
    setenv("ADSP_LIBRARY_PATH", dspSearchPath.c_str(), 1);
    LOGI("ADSP_LIBRARY_PATH = %s", dspSearchPath.c_str());

    // 设置 LD_LIBRARY_PATH
    setenv("LD_LIBRARY_PATH", nativeLibPath, 1);

    // 【关键步骤】从 native 库路径加载所有后端
    // 这会自动检测并加载 CPU、OpenCL、Hexagon 等后端
    ggml_backend_load_all_from_path(nativeLibPath);

    env->ReleaseStringUTFChars(libPath, nativeLibPath);
    env->ReleaseStringUTFChars(dspLibPath, dspPath);
    LOGI("Backends loaded dynamically");

    // 初始化 llama 后端
    llama_backend_init();
    LOGI("llama backend initialized");

    // ============================================================
    // 加载模型
    // ============================================================
    LOGI("Loading model: %s", path);

    // 创建模型参数
    llama_model_params model_params = llama_model_default_params();

    // 使用单设备模式，避免在 CPU/GPU/NPU 之间分割模型
    model_params.split_mode = LLAMA_SPLIT_MODE_NONE;

    // 将所有层都放到 GPU/NPU 上 (99 层足够覆盖大多数模型)
    model_params.n_gpu_layers = 99;

    LOGI("Model params: split_mode=NONE, n_gpu_layers=99 (NPU acceleration)");

    // 加载模型文件
    llama_model* model = llama_model_load_from_file(path, model_params);
    env->ReleaseStringUTFChars(modelPath, path);

    if (!model) {
        LOGE("Failed to load model");
        return 0;  // 返回 0 表示失败
    }

    // 获取模型信息
    const llama_vocab* vocab = llama_model_get_vocab(model);
    int32_t n_vocab = llama_vocab_n_tokens(vocab);
    int32_t n_layer = llama_model_n_layer(model);

    LOGI("Model loaded successfully");
    LOGI("  Vocab size: %d", n_vocab);
    LOGI("  Total layers: %d", n_layer);

    // ============================================================
    // 创建推理上下文
    // ============================================================
    LOGI("----------------------------------------");
    LOGI("Creating llama context...");

    // 创建上下文参数
    llama_context_params ctx_params = llama_context_default_params();

    // 上下文配置
    const int DEFAULT_CONTEXT_SIZE = 8192;  // 上下文窗口大小 (最多记住 8192 个 token)
    const int BATCH_SIZE = 128;             // 批处理大小 (官方使用 128)

    ctx_params.n_ctx = DEFAULT_CONTEXT_SIZE;
    ctx_params.n_batch = BATCH_SIZE;        // 每批处理的 token 数
    ctx_params.n_ubatch = BATCH_SIZE;       // 微批次大小
    ctx_params.n_threads = nThreads;        // CPU 线程数
    ctx_params.n_threads_batch = nThreads;  // 批处理线程数

    // 注意: Flash Attention 在 Hexagon NPU 上不支持，所以禁用
    // ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;

    LOGI("Context params (Hexagon NPU):");
    LOGI("  - n_ctx: %d", ctx_params.n_ctx);
    LOGI("  - n_batch: %d", ctx_params.n_batch);
    LOGI("  - n_ubatch: %d", ctx_params.n_ubatch);
    LOGI("  - threads: %d", nThreads);
    LOGI("  - flash_attn: DISABLED (not supported by Hexagon)");

    // 创建上下文
    llama_context* ctx = llama_init_from_model(model, ctx_params);

    if (!ctx) {
        LOGE("❌ Failed to create context");
        llama_model_free(model);
        return 0;
    }

    LOGI("✓ Context created successfully");

    // 创建 Android 上下文结构体
    llama_android_context* android_ctx = new llama_android_context();
    android_ctx->model = model;
    android_ctx->ctx = ctx;

    LOGI("========================================");
    LOGI("✅ Initialization complete!");
    LOGI("========================================");

    // 将指针转换为 jlong 返回给 Kotlin
    return reinterpret_cast<jlong>(android_ctx);
}

// ============================================================================
// 辅助函数: 判断是否应该停止生成
// ============================================================================
/**
 * 【函数】判断是否应该停止文本生成
 *
 * LLM 有时候会生成过长或重复的内容，这个函数用于提前终止生成。
 *
 * @param generated_text - 已生成的文本
 * @param token_count    - 已生成的 token 数量
 * @return true 如果应该停止，false 如果继续
 */
bool should_stop_generation(const std::string& generated_text, int token_count) {
    // 规则 1: 超过 256 个 token 就停止
    if (token_count > 256) {
        LOGD("Stopping: reached max reasonable tokens (%d)", token_count);
        return true;
    }

    // 规则 2: 找到结束标记 <|im_end|>
    if (generated_text.find("<|im_end|>") != std::string::npos) {
        LOGD("Stopping: found end token marker");
        return true;
    }

    // 规则 3: 找到新的角色标记 (说明模型开始"自问自答"了)
    size_t first_marker = generated_text.find("<|im_start|>");
    if (first_marker != std::string::npos && first_marker > 10) {
        LOGD("Stopping: found new role marker");
        return true;
    }

    // 规则 4: 检查是否是完整的短句 (中文句号 "。" 或英文标点)
    if (token_count > 50 && generated_text.length() > 0) {
        // 检查中文句号 (UTF-8 编码: E3 80 82)
        if (generated_text.length() >= 3) {
            std::string last_three = generated_text.substr(generated_text.length() - 3);
            if (last_three == "\xe3\x80\x82") {  // 中文句号 "。"
                if (token_count < 100) {
                    LOGD("Stopping: short complete response at %d tokens", token_count);
                    return true;
                }
            }
        }

        // 检查英文标点
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
// 函数 2: nativeCompletion - 非流式文本生成
// ============================================================================
/**
 * 【函数】生成文本回复 (非流式，一次性返回完整结果)
 *
 * 【Kotlin 调用示例】
 * val response = nativeCompletion(contextPtr, prompt, 256, 0.7f, 0.9f, 40)
 *
 * 【参数说明】
 * @param env         - JNI 环境指针
 * @param thiz        - Java 对象
 * @param contextPtr  - 上下文指针 (从 nativeInit 获得)
 * @param prompt      - 输入提示词
 * @param nPredict    - 最大生成 token 数
 * @param temperature - 温度参数 (0.0-2.0，越高越随机)
 * @param topP        - Top-P 采样参数 (0.0-1.0)
 * @param topK        - Top-K 采样参数 (1-100)
 *
 * @return 生成的文本
 */
JNIEXPORT jstring JNICALL
Java_com_stdemo_ggufchat_GGUFChatEngine_nativeCompletion(
        JNIEnv* env, jobject thiz,
        jlong contextPtr,
        jstring prompt,
        jint nPredict,
        jfloat temperature,
        jfloat topP,
        jint topK) {

    (void)thiz;

    // 获取上下文
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

    // ============================================================
    // 步骤 1: 清空 KV Cache
    // ============================================================
    // KV Cache 存储之前的对话上下文
    // 清空它是为了让每次对话都是独立的
    llama_memory_seq_rm(llama_get_memory(ctx), -1, 0, -1);
    LOGD("KV Cache cleared");

    // ============================================================
    // 步骤 2: 创建采样器链
    // ============================================================
    // 采样器决定了如何从模型输出的概率分布中选择下一个 token
    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());

    // Top-K 采样: 只考虑概率最高的 K 个 token
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(topK));

    // Top-P (nucleus) 采样: 只考虑累积概率达到 P 的 token
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(topP, 1));

    // 温度采样: 控制输出的随机性
    // temperature < 1: 更确定性 (选择概率高的)
    // temperature > 1: 更随机 (给低概率 token 更多机会)
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));

    // 分布采样: 根据调整后的概率分布随机选择
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    LOGD("Sampler created with temp: %.2f, top_p: %.2f, top_k: %d", temperature, topP, topK);

    // ============================================================
    // 步骤 3: 将提示词转换为 token
    // ============================================================
    // Tokenization: 把文本拆分成 token (模型能理解的最小单位)
    std::vector<llama_token> tokens;
    int max_tokens = strlen(prompt_text) + 32;  // 预留足够空间
    tokens.resize(max_tokens);

    // 调用 tokenize 函数
    // 参数: vocab, 文本, 文本长度, 输出数组, 数组大小, add_special=true, parse_special=false
    int n_tokens = llama_tokenize(
            vocab,
            prompt_text,
            strlen(prompt_text),
            tokens.data(),
            tokens.size(),
            true,   // 添加特殊 token (如 BOS)
            false   // 不解析特殊 token
    );

    // 如果 buffer 太小，重新分配
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

    // ============================================================
    // 步骤 4: 处理提示词 (Prefill 阶段)
    // ============================================================
    // 把所有提示词 token 一次性送入模型
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);

    if (llama_decode(ctx, batch) != 0) {
        LOGE("Failed to decode prompt");
        llama_sampler_free(sampler);
        return env->NewStringUTF("Error: Failed to decode prompt");
    }

    LOGD("Prompt decoded, starting static generation");

    // ============================================================
    // 步骤 5: 生成循环 (Decode 阶段)
    // ============================================================
    std::string result;
    result.reserve(nPredict * 4);  // 预留空间 (中文字符最多 4 字节)

    int generation_token_count = 0;
    const std::string end_marker = "<|im_end|>";
    bool found_end = false;

    // 记录开始时间 (用于计算速度)
    auto gen_start_time = std::chrono::high_resolution_clock::now();

    // 生成循环
    for (int i = 0; i < nPredict; i++) {
        // 从采样器获取下一个 token
        llama_token new_token = llama_sampler_sample(sampler, ctx, -1);

        // 检查是否是结束 token
        if (llama_vocab_is_eog(vocab, new_token)) {
            LOGD("End of generation at token %d", i);
            break;
        }

        // 将 token 转换为文本
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

            // 检查是否找到结束标记
            size_t marker_pos = result.find(end_marker);
            if (marker_pos != std::string::npos) {
                LOGD("Found end marker at pos %zu, truncating", marker_pos);
                result = result.substr(0, marker_pos);  // 截断到标记之前
                found_end = true;
                break;
            }
        }

        // 检查是否应该停止
        if (!found_end && should_stop_generation(result, generation_token_count)) {
            LOGD("Stopping generation early at token %d", i);
            break;
        }

        // 继续生成下一个 token
        if (!found_end) {
            batch = llama_batch_get_one(&new_token, 1);
            if (llama_decode(ctx, batch) != 0) {
                LOGE("Failed to decode token %d", i);
                break;
            }

            // 让采样器记住这个 token
            llama_sampler_accept(sampler, new_token);
        }
    }

    llama_sampler_free(sampler);

    // 同步后端操作
    llama_synchronize(ctx);
    LOGD("Backend synchronized after generation");

    // ============================================================
    // 步骤 6: 计算并输出性能统计
    // ============================================================
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

    // 获取 llama.cpp 的性能统计
    LOGI("========================================");
    LOGI("🔍 BACKEND USAGE STATISTICS:");
    LOGI("========================================");

    struct llama_perf_context_data perf = llama_perf_context(ctx);

    LOGI("📊 Context Performance:");
    LOGI("  - Prompt eval time: %.2f ms", perf.t_p_eval_ms);      // 提示词处理时间
    LOGI("  - Prompt eval count: %d", perf.n_p_eval);             // 提示词 token 数
    LOGI("  - Token eval time: %.2f ms", perf.t_eval_ms);         // 生成时间
    LOGI("  - Token eval count: %d", perf.n_eval);                // 生成的 token 数
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
// 函数 3: nativeCompletionStreaming - 流式文本生成
// ============================================================================
/**
 * 【函数】流式生成文本 (边生成边返回，用户能看到逐字输出效果)
 *
 * 【与 nativeCompletion 的区别】
 * - nativeCompletion: 等全部生成完才返回 → 用户要等很久
 * - nativeCompletionStreaming: 每生成一点就回调一次 → 用户立刻看到结果
 *
 * 【参数说明】
 * @param tokenCallback - Kotlin 回调对象，每生成一个 token 就调用 onToken(text)
 * 其他参数同 nativeCompletion
 */
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

    (void)thiz;

    llama_android_context* android_ctx = reinterpret_cast<llama_android_context*>(contextPtr);
    if (!android_ctx || !android_ctx->ctx || !android_ctx->model) {
        LOGE("Invalid context");
        return env->NewStringUTF("Error: Invalid context");
    }

    // 获取 Java 回调方法
    jclass callback_class = env->GetObjectClass(tokenCallback);
    jmethodID callback_method = env->GetMethodID(callback_class, "onToken", "(Ljava/lang/String;)V");
    if (!callback_method) {
        LOGE("Failed to find onToken method");
        env->DeleteLocalRef(callback_class);
        return env->NewStringUTF("Error: Callback method not found");
    }

    // 保存回调信息到线程局部变量
    g_env = env;
    g_callback_obj = env->NewGlobalRef(tokenCallback);  // 创建全局引用防止被 GC
    g_callback_method = callback_method;

    const char* prompt_text = env->GetStringUTFChars(prompt, nullptr);
    LOGD("Generating streaming completion for prompt (length: %zu)", strlen(prompt_text));

    llama_context* ctx = android_ctx->ctx;
    llama_model* model = android_ctx->model;
    const llama_vocab* vocab = llama_model_get_vocab(model);

    // 清空 KV Cache
    llama_memory_seq_rm(llama_get_memory(ctx), -1, 0, -1);
    LOGD("KV Cache cleared");

    // 创建采样器
    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(topK));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(topP, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    LOGD("Sampler created with temp: %.2f, top_p: %.2f, top_k: %d", temperature, topP, topK);

    // Tokenize 提示词
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

    // ============================================================
    // 分批处理提示词 (防止超过 batch size 导致崩溃)
    // ============================================================
    // 如果提示词有 355 个 token，但 batch size 只有 128，
    // 需要分 3 批 (128 + 128 + 99) 来处理
    int n_batch = llama_n_batch(ctx);
    LOGD("Processing prompt in batches (n_batch=%d, n_tokens=%d)", n_batch, n_tokens);

    for (int i = 0; i < n_tokens; i += n_batch) {
        int n_eval = std::min(n_batch, n_tokens - i);  // 这批要处理的 token 数
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

    // 用于存储完整生成结果
    std::string total_generated_text;
    total_generated_text.reserve(nPredict * 4);

    // 【关键】pending buffer: 用于累积不完整的 UTF-8 字节
    // 比如中文字符可能被拆成多个 token，需要等凑齐才能发送
    std::string pending_token_buffer;
    int generation_token_count = 0;

    auto gen_start_time = std::chrono::high_resolution_clock::now();

    const std::string end_marker = "<|im_end|>";
    bool found_end = false;

    // ============================================================
    // 流式生成循环
    // ============================================================
    for (int i = 0; i < nPredict; i++) {
        llama_token new_token = llama_sampler_sample(sampler, ctx, -1);

        // 检查结束 token
        if (llama_vocab_is_eog(vocab, new_token)) {
            LOGD("End of generation at token %d", i);
            break;
        }

        // Token 转文本
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

            // 添加到 pending buffer
            pending_token_buffer.append(token_str);
            total_generated_text.append(token_str);
            generation_token_count++;

            // --- UTF-8 和结束标记处理 ---

            // 1. 检查 buffer 中是否有结束标记
            size_t marker_pos = pending_token_buffer.find(end_marker);
            if (marker_pos != std::string::npos) {
                LOGD("Found end marker at buffer pos %zu, stopping", marker_pos);

                // 发送标记之前的内容
                std::string before_marker = pending_token_buffer.substr(0, marker_pos);
                if (!before_marker.empty()) {
                    LOGD("Sending content before marker: '%s'", before_marker.c_str());
                    token_callback(before_marker);
                }

                found_end = true;
                break;
            }

            // 2. 找到 buffer 中完整 UTF-8 序列的边界
            size_t boundary = find_utf8_boundary(pending_token_buffer.c_str(), pending_token_buffer.length());

            // 3. 发送完整的部分
            if (boundary > 0) {
                std::string complete_part = pending_token_buffer.substr(0, boundary);
                std::string tail = pending_token_buffer.substr(boundary);

                // 检查是否是结束标记的前缀 (如 "<", "<|", "<|im" 等)
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
                    // 检查整个 token 是否是标记前缀
                    for (size_t j = 1; j < end_marker.length(); j++) {
                        if (complete_part == end_marker.substr(0, complete_part.length())) {
                            is_marker_prefix = true;
                            LOGD("Full token matches %zu-char prefix of marker, holding.", j);
                            break;
                        }
                    }
                }

                if (!is_marker_prefix) {
                    // 不是标记前缀，可以安全发送
                    LOGD("Sending complete part: '%s'", complete_part.c_str());
                    token_callback(complete_part);
                    pending_token_buffer = tail;  // 保留未完成的部分
                }
            }
        }

        // 简单的停止检查: 限制 256 个 token
        if (generation_token_count >= 256) {
            LOGD("Stopping: reached 256 token limit");
            break;
        }

        // 继续生成
        if (!found_end) {
            llama_batch batch = llama_batch_get_one(&new_token, 1);
            if (llama_decode(ctx, batch) != 0) {
                LOGE("Failed to decode token %d", i);
                break;
            }

            llama_sampler_accept(sampler, new_token);
        }
    }

    // 刷新剩余的 buffer
    if (!pending_token_buffer.empty()) {
        size_t marker_pos = pending_token_buffer.find(end_marker);

        if (marker_pos != std::string::npos) {
            std::string before_marker = pending_token_buffer.substr(0, marker_pos);
            if (!before_marker.empty()) {
                LOGD("Final flush buffer before marker: '%s'", before_marker.c_str());
                token_callback(before_marker);
            }
        } else {
            LOGD("Final flush buffer: '%s'", pending_token_buffer.c_str());
            token_callback(pending_token_buffer);
        }
    }

    llama_sampler_free(sampler);

    // 同步后端
    llama_synchronize(ctx);
    LOGD("Backend synchronized after generation");

    // 性能统计
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

    LOGI("========================================");
    LOGI("🔍 BACKEND USAGE STATISTICS:");
    LOGI("========================================");

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

    // 清理回调
    env->DeleteGlobalRef(g_callback_obj);
    g_env = nullptr;
    g_callback_obj = nullptr;
    g_callback_method = nullptr;

    return env->NewStringUTF(total_generated_text.c_str());
}

// ============================================================================
// 函数 4: nativeFree - 释放资源
// ============================================================================
/**
 * 【函数】释放 LLM 占用的资源
 *
 * 【何时调用】
 * - 应用退出时
 * - 切换模型时
 * - 需要释放内存时
 *
 * 【重要】不调用这个函数会导致内存泄漏！
 */
JNIEXPORT void JNICALL
Java_com_stdemo_ggufchat_GGUFChatEngine_nativeFree(
        JNIEnv* env, jobject thiz, jlong contextPtr) {

    (void)env;
    (void)thiz;

    llama_android_context* android_ctx = reinterpret_cast<llama_android_context*>(contextPtr);
    if (android_ctx) {
        // 释放上下文
        if (android_ctx->ctx) {
            llama_free(android_ctx->ctx);
            LOGD("Context freed");
        }

        // 释放模型
        if (android_ctx->model) {
            llama_model_free(android_ctx->model);
            LOGD("Model freed");
        }

        // 释放结构体本身
        delete android_ctx;
    }

    // 释放 llama 后端
    llama_backend_free();

    // 清理 EGL 资源
    cleanup_egl();
    LOGD("EGL resources cleaned up");
}

}  // extern "C"

/**
 * ============================================================================
 * 文件结束
 * ============================================================================
 *
 * 【总结：这个文件的核心流程】
 *
 * 1. Kotlin 调用 nativeInit()
 *    ↓
 * 2. 初始化 EGL (为 GPU 加速准备)
 *    ↓
 * 3. 加载 llama.cpp 后端 (CPU/GPU/NPU)
 *    ↓
 * 4. 加载模型文件 (.gguf)
 *    ↓
 * 5. 创建推理上下文
 *    ↓
 * 6. 返回上下文指针给 Kotlin
 *
 * ---
 *
 * 7. Kotlin 调用 nativeCompletion() 或 nativeCompletionStreaming()
 *    ↓
 * 8. Tokenize 提示词
 *    ↓
 * 9. 处理提示词 (Prefill)
 *    ↓
 * 10. 循环生成 token (Decode)
 *     - 采样下一个 token
 *     - 转换为文本
 *     - (流式) 回调给 Kotlin
 *    ↓
 * 11. 返回完整结果
 *
 * ---
 *
 * 12. Kotlin 调用 nativeFree()
 *     ↓
 * 13. 释放所有资源
 *
 * ============================================================================
 */
