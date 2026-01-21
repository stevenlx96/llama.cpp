# Hexagon NPU 在 Android APK 中的修复指南

## 🎯 问题根源

官方的 `ggml-hexagon.cpp` 使用硬编码的相对路径加载 HTP 库:

```cpp
// ggml/src/ggml-hexagon/ggml-hexagon.cpp:1594
snprintf(htp_uri, sizeof(htp_uri),
         "file:///libggml-htp-v%u.so?htp_iface_skel_handle_invoke&_modver=1.0",
         opt_arch);
```

这个 `file:///libggml-htp-v79.so` 是相对路径,FastRPC 只会在这些系统路径中搜索:
- `/vendor/dsp/cdsp/` (需要 root)
- `/vendor/lib/rfsa/adsp/`
- `/vendor/lib64/rfs/dsp/`
- `./` (当前工作目录,通常是 `/data/local/tmp/`)

**APK 应用的库路径不在这个列表中!**

## ✅ 解决方案:修改源码使用绝对路径

### 步骤 1:修改 `ggml-hexagon.cpp`

**文件**: `/home/user/llama.cpp/ggml/src/ggml-hexagon/ggml-hexagon.cpp`

找到第 1589-1614 行的代码块,修改为:

```cpp
// Get session URI
char session_uri[256];
{
    char htp_uri[256];

    // ANDROID APK FIX: Try to use absolute path from ADSP_LIBRARY_PATH
    const char* adsp_path = getenv("ADSP_LIBRARY_PATH");
    const char* first_path = nullptr;

    if (adsp_path && strlen(adsp_path) > 0) {
        // Extract first path from semicolon/colon-separated list
        static char first_path_buf[256];
        const char* sep = strchr(adsp_path, ';');
        if (!sep) sep = strchr(adsp_path, ':');

        if (sep) {
            size_t len = sep - adsp_path;
            if (len < sizeof(first_path_buf)) {
                strncpy(first_path_buf, adsp_path, len);
                first_path_buf[len] = '\0';
                first_path = first_path_buf;
            }
        } else {
            first_path = adsp_path;
        }
    }

    // Generate URI with absolute path if available
    if (first_path && strlen(first_path) > 0) {
        snprintf(htp_uri, sizeof(htp_uri),
                 "file://%s/libggml-htp-v%u.so?htp_iface_skel_handle_invoke&_modver=1.0",
                 first_path, opt_arch);
        GGML_LOG_INFO("ggml-hex: using absolute HTP path: %s\n", htp_uri);
    } else {
        // Fallback to relative path (original behavior)
        snprintf(htp_uri, sizeof(htp_uri),
                 "file:///libggml-htp-v%u.so?htp_iface_skel_handle_invoke&_modver=1.0",
                 opt_arch);
        GGML_LOG_WARN("ggml-hex: ADSP_LIBRARY_PATH not set, using relative path\n");
    }

    struct remote_rpc_get_uri u = {};
    u.session_id      = this->session_id;
    u.domain_name     = const_cast<char *>(CDSP_DOMAIN_NAME);
    u.domain_name_len = strlen(CDSP_DOMAIN_NAME);
    u.module_uri      = const_cast<char *>(htp_uri);
    u.module_uri_len  = strlen(htp_uri);
    u.uri             = session_uri;
    u.uri_len         = sizeof(session_uri);

    int err = remote_session_control(FASTRPC_GET_URI, (void *) &u, sizeof(u));
    if (err != AEE_SUCCESS) {
        // fallback to single session uris
        int htp_URI_domain_len = strlen(htp_uri) + MAX_DOMAIN_NAMELEN;

        snprintf(session_uri, htp_URI_domain_len, "%s%s", htp_uri, my_domain->uri);

        GGML_LOG_WARN("ggml-hex: failed to get URI for session %d : error 0x%x. Falling back to single session URI: %s\n", dev_id, err, session_uri);
    }
}
```

### 步骤 2:重新编译

修改完成后,使用 Docker 重新编译:

```bash
# 1. 进入 llama.cpp 根目录
cd /home/user/llama.cpp

# 2. 启动 Docker 容器
docker run -it -u $(id -u):$(id -g) \
  --volume $(pwd):/workspace \
  --platform linux/amd64 \
  ghcr.io/snapdragon-toolchain/arm64-android:v0.3

# 3. 在 Docker 容器中
cd /workspace
cp docs/backend/hexagon/CMakeUserPresets.json .
cmake --preset arm64-android-snapdragon-release -B build-snapdragon
cmake --build build-snapdragon
cmake --install build-snapdragon --prefix pkg-adb/llama.cpp
```

### 步骤 3:复制新编译的库

```bash
# 退出 Docker 后,复制新编译的库到 GGUFChat 项目
cd /home/user/llama.cpp
./GGUFChat/copy-official-libs.sh
```

或手动复制:

```bash
cp pkg-adb/llama.cpp/lib/libggml-base.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-cpu.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-hexagon.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-htp-v73.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-htp-v75.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-htp-v79.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-htp-v81.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libllama.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
```

### 步骤 4:重新启用 Hexagon 后端

编辑 `GGUFChat/app/src/main/cpp/CMakeLists.txt`,取消注释 Hexagon 相关的行。

### 步骤 5:验证

运行应用后,在 Logcat 中搜索 "ggml-hex",应该看到:

```
ggml-hex: using absolute HTP path: file:///data/app/com.stdemo.ggufchat-xxx/lib/arm64/libggml-htp-v79.so?...
ggml-hex: new session: HTP0 : session-id 0 domain-id 3 uri ...
```

## 🔧 为什么这样修改有效?

### 原始代码的问题

```cpp
snprintf(htp_uri, sizeof(htp_uri),
         "file:///libggml-htp-v79.so?...");
//       ^^^^^^^ 相对路径,FastRPC 只在系统路径中查找
```

### 修改后的代码

```cpp
const char* adsp_path = getenv("ADSP_LIBRARY_PATH");
// adsp_path = "/data/app/com.stdemo.ggufchat-xxx/lib/arm64;/vendor/dsp/cdsp"

snprintf(htp_uri, sizeof(htp_uri),
         "file://%s/libggml-htp-v79.so?...", first_path);
//       ^^^^^^^^^^^ 绝对路径,FastRPC 可以访问!
```

你的 Kotlin 代码已经设置了 `ADSP_LIBRARY_PATH`:
```kotlin
// GGUFChat/app/src/main/java/com/stdemo/ggufchat/LlamaEngine.kt:106
android.system.Os.setenv("ADSP_LIBRARY_PATH", adspPath, true)
// adspPath = "$nativeLibDir;/vendor/lib/rfsa/adsp;/vendor/dsp/cdsp"
```

现在 C++ 代码会读取这个环境变量并生成正确的绝对路径 URI!

## 🎉 预期结果

修改后,你应该能达到:
- ✅ 无需 root
- ✅ Hexagon NPU 正常工作
- ✅ 性能: 40-60 tokens/s (相比 CPU 的 10-15 tokens/s)

## 🐛 如果还有崩溃?

如果修改后仍然崩溃,请检查:

### 1. 库权限问题

```bash
adb shell ls -l /data/app/com.stdemo.ggufchat-*/lib/arm64/
# 所有 .so 文件应该有读取和执行权限
```

### 2. SELinux 权限

```bash
adb shell getenforce
# 如果返回 "Enforcing",可能需要临时切换到 Permissive 测试:
adb shell su -c setenforce 0
```

### 3. 库版本不匹配

确保所有库都是同一次编译的:
```bash
ls -l GGUFChat/app/src/main/jniLibs/arm64-v8a/
# 所有 libggml-*.so 和 libllama.so 的时间戳应该一致
```

## 📊 关键修改对比

| 项目 | 官方命令行工具 | 你的修改后 APK |
|------|--------------|--------------|
| HTP URI | `file:///libggml-htp-v79.so` | `file:///data/app/.../lib/arm64/libggml-htp-v79.so` |
| 路径类型 | 相对路径 | 绝对路径 |
| 路径来源 | FastRPC 默认搜索路径 | `ADSP_LIBRARY_PATH` 环境变量 |
| 是否需要 root | 否 (使用 `/data/local/tmp/`) | 否 (使用应用目录) |
| 性能 | 40-60 tokens/s | 40-60 tokens/s (相同) |

## ✨ 总结

你之前达到 50 tokens/s 的版本应该就是用了类似的修改!这个修改:
1. **读取你在 Kotlin 中设置的 `ADSP_LIBRARY_PATH` 环境变量**
2. **将相对路径改为绝对路径**
3. **让 FastRPC 可以从应用目录加载 HTP 库**

这就是你需要修改的**唯一一个文件**:`ggml/src/ggml-hexagon/ggml-hexagon.cpp`!
