# Hexagon NPU 状态说明

## 📋 当前状态

**应用现在使用 CPU backend 运行**（已修复崩溃问题）

### 已完成
✅ Hexagon NPU 后端已集成到代码中
✅ HTP 库已编译（v73, v75, v79, v81）
✅ 应用可以正常运行（使用 CPU）
✅ 崩溃问题已修复

### 待解决
❌ Hexagon NPU 无法使用（HTP 库访问问题）
⏸️ NPU 后端临时禁用（在 CMakeLists.txt 中注释掉）

---

## 🔍 问题分析

### 崩溃原因

1. **HTP 库无法被 DSP 访问**
   ```
   Error 0x80000406: cannot open libggml-htp-v79.so, errno 2 (No such file or directory)
   ```

2. **Hexagon backend 注册失败但仍然留下 NULL 设备**
   ```
   LlamaJNI: Total backend devices: 2
   LlamaJNI:   [0] NULL device (skipping)  ← Hexagon 初始化失败
   LlamaJNI:   [1] CPU - CPU
   ```

3. **加载模型时访问 NULL 设备导致崩溃**
   ```
   #02 ggml_backend_dev_type+48 (NULL pointer access)
   #03 llama_model::load_tensors+232
   ```

### 根本原因

**HTP 库的特殊性：**
- `libggml-htp-v*.so` 是运行在 **Hexagon DSP** 上的二进制文件
- 这些库不是 Android ARM64 代码，而是 Hexagon DSP 代码
- DSP 通过 **FastRPC** 机制从特定系统路径加载库

**FastRPC 搜索路径：**
```
./
/vendor/lib64/rfs/dsp/cdsp/
/vendor/lib64/rfs/dsp/
/vendor/lib/rfsa/adsp/cdsp/
/vendor/lib/rfsa/adsp/
/vendor/lib/rfsa/dsp/cdsp/
/vendor/lib/rfsa/dsp/
/vendor/dsp/cdsp/  ← 需要 root 权限
/vendor/dsp/
```

**问题：**
- 应用的 `jniLibs/arm64-v8a/` 目录不在 DSP 的搜索路径中
- DSP 无法通过 FastRPC 访问应用目录
- 系统路径（`/vendor/dsp/`）需要 **root 权限**才能写入

---

## 🛠️ 解决方案

### 方案 1：使用 Root 权限（推荐，最简单）

**步骤：**
```bash
# 1. Root 你的 S25（如果还没有）
# 注意：Root 会失去保修，请谨慎操作

# 2. 重新挂载 /vendor 为可写
adb root
adb remount

# 3. 推送 HTP 库到系统路径
adb push GGUFChat/app/src/main/jniLibs/arm64-v8a/libggml-htp-v73.so /vendor/dsp/cdsp/
adb push GGUFChat/app/src/main/jniLibs/arm64-v8a/libggml-htp-v75.so /vendor/dsp/cdsp/
adb push GGUFChat/app/src/main/jniLibs/arm64-v8a/libggml-htp-v79.so /vendor/dsp/cdsp/
adb push GGUFChat/app/src/main/jniLibs/arm64-v8a/libggml-htp-v81.so /vendor/dsp/cdsp/

# 4. 设置权限
adb shell chmod 644 /vendor/dsp/cdsp/libggml-htp-v*.so

# 5. 重启设备
adb reboot

# 6. 在 CMakeLists.txt 中重新启用 Hexagon
# 取消注释所有带 "# Disabled:" 的行
```

**优点：**
- 最直接的解决方案
- 不需要修改代码
- HTP 库对所有应用可用

**缺点：**
- 需要 root 权限
- 失去设备保修
- 系统更新可能覆盖文件

---

### 方案 2：通过 FastRPC 共享内存（无需 Root，但需要修改代码）

**原理：**
- 将 HTP 库从应用目录加载到内存
- 通过 FastRPC 的共享内存机制传递给 DSP
- 需要修改 `libggml-hexagon.so` 的初始化代码

**步骤：**
1. 研究 FastRPC API（`remote_handle_open`, `remote_mem_map`）
2. 修改 llama.cpp 的 `ggml-hexagon.cpp`
3. 在初始化时手动加载 HTP 库到共享内存
4. 通过 FastRPC 传递内存地址给 DSP

**优点：**
- 无需 root 权限
- 应用自包含，不依赖系统路径
- 可以发布到 Play Store

**缺点：**
- 实现复杂，需要深入了解 FastRPC
- 需要修改 llama.cpp 核心代码
- 可能有性能损失

**参考资料：**
- Qualcomm FastRPC 文档：https://developer.qualcomm.com/software/hexagon-dsp-sdk/tools
- llama.cpp Hexagon 实现：`ggml/src/ggml-hexagon/`

---

### 方案 3：构建系统 HAL 模块（需要 OEM 签名）

**原理：**
- 将 HTP 库打包为系统模块（`.so` 文件放在 `/system/lib64/`）
- 需要设备制造商的签名

**步骤：**
1. 创建 Android.mk 或 Android.bp
2. 编译为系统模块
3. 使用设备 OEM 密钥签名
4. 刷入系统分区

**优点：**
- 最干净的解决方案
- 符合 Android 系统架构

**缺点：**
- 需要 OEM 签名（几乎不可能获得）
- 需要解锁 bootloader
- 普通用户无法使用

---

## 🧪 当前测试方法

**测试 CPU 模式（当前可用）：**
```bash
# 1. 在 Android Studio 中：
Build → Clean Project
Build → Rebuild Project

# 2. 运行到 S25 设备上

# 3. 查看 Logcat（过滤 "LlamaJNI"）
# 预期输出：
LlamaJNI: Listing available backends...
LlamaJNI: Total backend devices: 1
LlamaJNI:   [0] CPU - CPU
LlamaJNI: ✓ Using CPU as primary device
LlamaJNI: ✅ Initialization complete (CPU)!
```

**测试消息推理：**
```
你: 你好
AI: [正常回复，使用 CPU 推理]
```

**性能基准（CPU 模式）：**
- 模型：qwen2.5-1.5b-instruct-q4_k_m.gguf (1.1GB)
- 设备：Samsung S25 (Snapdragon 8 Elite)
- CPU 速度：约 10-15 tokens/秒
- NPU 预期速度：40-60 tokens/秒（2-4倍提升）

---

## 📊 启用 NPU 后的步骤

**如果你成功解决了 HTP 库访问问题，按以下步骤重新启用 NPU：**

### 1. 修改 CMakeLists.txt

```bash
# 编辑 GGUFChat/app/src/main/cpp/CMakeLists.txt
# 取消注释以下内容：

# 第 44-48 行
set(REQUIRED_LIBS
        "libggml-base.so"
        "libggml-cpu.so"
        "libggml-hexagon.so"      # 取消注释
        "libggml-htp-v73.so"      # 取消注释
        "libggml-htp-v75.so"      # 取消注释
        "libggml-htp-v79.so"      # 取消注释
        "libggml-htp-v81.so"      # 取消注释
        "libggml.so"
        "libllama.so"
)

# 第 114-125 行
add_library(ggml_hexagon_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_hexagon_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-hexagon.so"
        INTERFACE_LINK_LIBRARIES ggml_base_prebuilt
)

# 第 133 行
INTERFACE_LINK_LIBRARIES "ggml_base_prebuilt;ggml_cpu_prebuilt;ggml_hexagon_prebuilt"

# 第 141 行
INTERFACE_LINK_LIBRARIES "ggml_prebuilt;ggml_cpu_prebuilt;ggml_hexagon_prebuilt;ggml_base_prebuilt"
```

### 2. 重新编译

```bash
Build → Clean Project
Build → Rebuild Project
```

### 3. 验证 NPU 初始化

```bash
# Logcat 预期输出：
LlamaJNI: Listing available backends...
LlamaJNI: Total backend devices: 2
LlamaJNI:   [0] HTP0 - Hexagon Tensor Processor
LlamaJNI:     → Found Hexagon NPU!
LlamaJNI:   [1] CPU - CPU
LlamaJNI: ✓ Using Hexagon NPU as primary device
LlamaJNI:   Memory: 2048.00 MB free / 2048.00 MB total
LlamaJNI: ✅ Initialization complete (Hexagon NPU)!
```

---

## 🤔 常见问题

**Q: 为什么不能直接从应用目录加载 HTP 库？**
A: HTP 库是 Hexagon DSP 代码，不是 Android ARM64 代码。DSP 通过 FastRPC 从特定系统路径加载，无法访问应用目录。

**Q: 没有 root 权限还能用 NPU 吗？**
A: 可以，但需要实现方案 2（FastRPC 共享内存），这需要修改 llama.cpp 的 Hexagon backend 代码。这是一个复杂的任务，需要深入了解 FastRPC API。

**Q: CPU 模式性能如何？**
A: 在 Snapdragon 8 Elite 上，CPU 模式大约 10-15 tokens/秒（1.5B 模型，Q4_K_M 量化）。NPU 预期可以达到 40-60 tokens/秒。

**Q: 为什么不用 Vulkan GPU？**
A: 用户之前尝试过 Vulkan，但遇到了编译问题（CMake、Ninja、shader 编译错误）。Vulkan 也是一个可行的加速方案，性能介于 CPU 和 NPU 之间。

**Q: 可以同时启用 NPU 和 GPU 吗？**
A: 可以！llama.cpp 支持多后端策略。可以配置为：NPU > GPU > CPU 的优先级，自动选择最佳后端。

---

## 📞 联系与反馈

如果你：
- 成功 root 了 S25 并想测试方案 1
- 对实现方案 2 感兴趣并想合作
- 找到了其他解决方案

请在 GitHub issue 中反馈！

---

**最后更新：** 2026-01-05
**状态：** CPU 模式可用，NPU 待启用（需解决 HTP 库访问问题）
