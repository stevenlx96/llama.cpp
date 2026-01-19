# JNI CPP 文件最近 15 次改动详解

**目的：** 分析为什么性能从 30 token/s 下降

---

## 版本 01 (最新) - 07e7132
**文件：** version_01_07e7132.cpp
**标题：** Fix build error: Change async_copy to async

### 改动说明
- **问题：** 编译错误，`ggml_backend_dev_caps` 结构体没有 `async_copy` 字段
- **修复：** 将字段名从 `async_copy` 改为 `async`
- **影响：** 仅修复编译错误，不影响性能

---

## 版本 02 - f6358e3
**文件：** version_02_f6358e3.cpp
**标题：** Add comprehensive backend diagnostic logging

### 改动说明
- **问题：** 性能卡在 ~12 tokens/s，无法验证 OpenCL GPU 是否真正被使用
- **改动内容：**
  1. 启用层分配日志（临时）- 显示哪些层分配给 OpenCL/Hexagon/CPU
  2. 添加详细的设备能力日志 - 打印设备类型、异步/host_buffer 能力
  3. 添加每次推理的后端使用统计 - 使用 `llama_perf_context()` 显示真实性能
  4. 添加慢性能诊断提示
- **影响：** 增加了大量诊断日志，可能略微影响性能，但帮助调试

---

## 版本 03 - 7fb2ea7
**文件：** version_03_7fb2ea7.cpp
**标题：** CRITICAL FIX: Enable dual accelerators with LAYER split mode

### 改动说明
- **根本原因分析：**
  1. OpenCL 后端缺少异步操作支持（event_record/wait = NULL）
  2. 多后端调度器依赖异步事件进行同步
  3. 没有异步操作，OpenCL + Hexagon 并发会导致死锁
- **解决方案：** 使用 `LLAMA_SPLIT_MODE_LAYER`
  - 将模型层分配到不同设备（部分在 Hexagon，部分在 OpenCL）
  - 避免频繁的跨设备同步（会触发死锁）
  - 调度器独立为每个设备分配整层
  - 只在层边界同步，而非每个操作
- **配置：**
  - device_array[0] = Hexagon NPU（优先用于计算密集型操作）
  - device_array[1] = OpenCL GPU（次要加速器）
  - split_mode = LLAMA_SPLIT_MODE_LAYER
  - n_gpu_layers = 999
- **影响：** **可能的性能影响点** - LAYER 分割模式可能导致两个加速器使用不均衡

---

## 版本 04 - 239743c
**文件：** version_04_239743c.cpp
**标题：** CRITICAL FIX: Disable OpenCL to prevent inference deadlock

### 改动说明
- **根本原因：** 同时使用 OpenCL GPU + Hexagon NPU 导致推理挂起/崩溃
- **症状：**
  - 两个后端都初始化成功（OpenCL + Hexagon）
  - 模型加载成功
  - 但推理在第一个 token 生成时挂起（"> Hello" → 冻结）
- **修复：** 禁用 OpenCL，**仅使用 Hexagon NPU**（匹配官方工具）
  - 官方工具使用：`--device HTP0`（仅 Hexagon，不用 OpenCL）
  - device_array 现在包含：[Hexagon, nullptr]
- **影响：** **关键转折点** - 从这个版本开始只用 Hexagon，不用 GPU

---

## 版本 05 - 0f7ee5d
**文件：** version_05_0f7ee5d.cpp
**标题：** CRITICAL: Enable dual accelerators (OpenCL GPU + Hexagon NPU)

### 改动说明
- **目标：** 匹配官方 llama-completion 工具配置，同时使用 OpenCL (Adreno GPU) 和 Hexagon (NPU)
- **关键改动：**
  1. 移除 `(void)opencl_dev` 抑制 - OpenCL 现在被主动使用
  2. device_array 从 [2] 扩展到 [3] 以支持双加速器 + NULL
  3. 添加 OpenCL 设备可用性测试（类似 Hexagon）
  4. 设备顺序匹配官方工具：OpenCL 第一，Hexagon 第二
  5. 根据可用性动态构建设备数组
- **配置：**
  - device_array[0] = OpenCL (Adreno 830 GPU)
  - device_array[1] = Hexagon (HTP NPU)
  - device_array[2] = nullptr
- **影响：** 尝试启用双加速器以提升性能

---

## 版本 06 - c993fa8
**文件：** version_06_c993fa8.cpp
**标题：** CRITICAL: Add official tool params to match 30 tokens/s configuration

### 改动说明
- **添加了两个关键参数以匹配官方工具：**
  1. `model_params.use_mmap = false` (匹配 `--no-mmap` 标志)
  2. `ctx_params.flash_attn_type = ENABLED` (匹配 `-fa on` 标志)
- **官方工具命令：**
  ```
  llama-completion --no-mmap -fa on --batch-size 128 -ngl 99 --device HTP0
  ```
- **当前配置现在完全匹配官方工具：**
  - batch_size: 128 ✅
  - use_mmap: false ✅
  - flash_attn: enabled ✅
  - n_gpu_layers: 999 ✅
  - devices: [hexagon, nullptr] ✅
- **理论：** 之前的 30 tokens/s 成功可能就是使用了这些参数
- **影响：** **关键参数点** - 这两个参数对性能有重大影响

---

## 版本 07 - a9db1ec
**文件：** version_07_a9db1ec.cpp
**标题：** Revert threadpool CPU affinity implementation to fix ABI crash

### 改动说明
- **问题：**
  - App 启动时立即崩溃，在 llama_context 构造函数中 SIGSEGV
  - 用户使用官方/干净的 .so 文件，没有 threadpool API
  - JNI 代码调用了 ggml_threadpool_new/llama_attach_threadpool
  - 导致 ABI 不匹配和段错误
- **解决方案：**
  - 移除所有 threadpool 相关代码
  - 回退到简单的 llama_context_params 配置
  - 移除 ggml.h 和 ggml-cpu.h 包含
  - 让 llama.cpp 使用默认线程行为
- **影响：** 移除了线程池优化，可能影响多线程性能

---

## 版本 08 - 5060494
**文件：** version_08_5060494.cpp
**标题：** CRITICAL FIX: Replace process-level CPU affinity with threadpool API

### 改动说明
- **问题：**
  - 之前的实现使用 `sched_setaffinity()` 限制了**整个进程**
  - 强制所有线程（包括 Hexagon RPC、UI 等）到 cores 2-7
  - 创建了严重的竞争和 NPU 干扰
  - 性能**恶化**：从 11.96 → 4.89 tokens/s（适得其反！）
- **根本原因：**
  - 官方工具使用 `--cpu-mask 0xfc --cpu-strict 1`（llama.cpp 内部 API）
  - 这只影响 llama.cpp 的工作线程，而非整个进程
  - 允许 Hexagon NPU 和其他线程在所有核心上自由运行
- **解决方案：**
  - 使用 ggml_threadpool API 创建带 CPU 亲和性的自定义线程池
  - 设置 cpumask[2-7] = true, strict_cpu = true, poll = 1000
  - 通过 llama_attach_threadpool() 附加到 llama_context
  - 仅 llama.cpp 工作线程被限制到性能核心
- **影响：** **可能的性能影响点** - 线程池 API 可能有 bug 或配置不当

---

## 版本 09 - d34d207
**文件：** version_09_d34d207.cpp
**标题：** Temporarily disable EGL init (causes crash)

### 改动说明
- **问题：**
  - EGL 上下文初始化导致 llama_context 构造函数中 SIGSEGV
  - 崩溃地址：0xfffe0017000000d4
  - 崩溃位置：llama_init_from_model → llama_context::llama_context
- **疑似根本原因：**
  - EGL 初始化干扰后端注册
  - 在后端 init 之前创建 OpenGL ES 上下文破坏了某些东西
  - 可能需要在后端注册**之后**初始化 EGL
- **当前方法：**
  - 暂时禁用 EGL init
  - 保留 CPU 亲和性（cores 2-7）应该仍能提供 2x 加速
- **影响：** **OpenCL 被禁用** - 这导致无法使用 GPU 加速

---

## 版本 10 - e9af68b
**文件：** version_10_e9af68b.cpp
**标题：** Fix compilation errors and warnings

### 改动说明
- **修复的错误：**
  1. `pthread_setaffinity_np` → `sched_setaffinity`
     - Android 使用 sched_setaffinity 而不是 pthread_setaffinity_np
  2. 'batch' 变量作用域错误
     - batch 在 prompt 处理循环中声明
     - 在需要的生成循环中重新声明
- **修复的警告：**
  1. 未使用的变量 'opencl_dev' - 标记为 (void)
  2. 未使用的变量 htp/cpu/total_tensor_count - 注释掉
  3. JNI 函数中未使用的参数 'thiz' - 标记为 (void)
  4. nativeFree 中未使用的参数 'env' - 标记为 (void)
- **影响：** 仅修复编译问题，无逻辑改动

---

## 版本 11 - cf55fe8
**文件：** version_11_cf55fe8.cpp
**标题：** CRITICAL: Initialize EGL context to enable OpenCL on Android

### 改动说明
- **问题：**
  - OpenCL 返回 -1001 (CL_PLATFORM_NOT_FOUND_KHR)
  - 官方工具（adb shell）可以访问 OpenCL/GPU
  - Android app 无法访问 OpenCL/GPU
- **根本原因：**
  - 高通 Adreno GPU 在 OpenCL 之前需要 EGL 上下文
  - 这是高通特定要求，OpenCL 与 OpenGL ES 共享资源
  - adb shell 有不同权限，app 需要显式初始化
- **解决方案：**
  - 添加 `init_egl_for_opencl()` 函数
  - 创建离屏 EGL 上下文（pbuffer 1x1）
  - 使用 OpenGL ES 3.0 (EGL_OPENGL_ES3_BIT)
  - 在 OpenCL 初始化之前使上下文生效
  - 在 CMakeLists.txt 中添加 EGL 和 GLESv3 库链接
- **技术细节：**
  1. 获取默认 EGL 显示
  2. 初始化 EGL
  3. 选择支持 OpenGL ES 3.0 的配置
  4. 创建 pbuffer 表面（离屏，无需窗口）
  5. 创建 OpenGL ES 3.0 上下文
  6. 使上下文生效
- **影响：** **尝试启用 OpenCL** - 但后续版本又因崩溃禁用了

---

## 版本 12 - 38031b2
**文件：** version_12_38031b2.cpp
**标题：** Add CPU affinity to performance cores (match official tool)

### 改动说明
- **问题：**
  - App 慢（9.85 tokens/s）vs 官方工具（34.68 tokens/s）
  - 官方工具使用：`--cpu-mask 0xfc --cpu-strict 1`
  - 0xfc = cores 2-7（Snapdragon 8 Elite 上的性能核心）
  - 没有 CPU 亲和性，Android 可能将线程调度到慢核心
- **解决方案：**
  - 添加 `set_cpu_affinity_performance_cores()` 函数
  - 使用 `pthread_setaffinity_np()` 将线程固定到 cores 2-7
  - 避免 cores 0-1（效率核心）
  - 在任何模型加载之前的 init 期间调用
- **预期影响：**
  - 2-3x 性能提升
  - 线程保证在快核心上运行
  - 匹配官方工具行为
- **技术细节：**
  - Cores 0-1: 效率核心 (~2.0 GHz)
  - Cores 2-7: 性能核心 (~3.5+ GHz)
  - CPU mask 0xfc = binary 11111100 = cores 2-7
- **影响：** CPU 亲和性优化，应该提升性能

---

## 版本 13 - 554fb93
**文件：** version_13_554fb93.cpp
**标题：** CRITICAL FIX: Add batch processing for long prompts

### 改动说明
- **问题：**
  - 355 个 token 的 prompt > n_batch (128) → 立即崩溃
  - 错误：llama_decode 中 SIGABRT (ggml_abort)
  - 官方工具使用 3 token prompts，无问题
- **根本原因：**
  - 代码尝试在单个 batch 中解码整个 prompt（355 tokens）
  - 超过 n_batch 限制 (128) → assertion failure → abort
- **解决方案：**
  - 将 prompt 分成多个 batch（每个 128 tokens）
  - 处理：128 + 128 + 99 = 355 tokens（3 个 batches）
  - 匹配标准 llama.cpp prompt 处理
- **影响：** 修复长 prompt 崩溃，但不应影响短 prompt 性能

---

## 版本 14 - eabec8f
**文件：** version_14_eabec8f.cpp
**标题：** CRITICAL FIX: Correct batch size to match official tool (128)

### 改动说明
- **之前的设置：**
  - n_batch = 512（错误 - 导致过多的计算缓冲区分配）
  - n_ubatch = 512
- **正确的设置（来自官方 adb 工具日志）：**
  - n_batch = 128
  - n_ubatch = 128
- **影响：**
  - 减少 HTP0 计算缓冲区：23 MiB → ~3 MiB
  - 减少 CPU 计算缓冲区：296.75 MiB → ~74 MiB
  - **预期性能提升：2-3x 更快**
- **参考：** 官方工具显示 34.68 tokens/s，batch_size=128
- **影响：** **关键性能点** - batch size 从 512 改为 128

---

## 版本 15 (最旧) - b625631
**文件：** version_15_b625631.cpp
**标题：** Optimize log output: filter repetitive verbose messages

### 改动说明
- **改动内容：**
  - 过滤 28+ 行的层分配刷屏
  - 过滤 28+ 行的 KV cache 层消息
  - 过滤详细的模型元数据转储
  - 过滤重复的控制 token 警告
  - 过滤图预留细节
  - 过滤后端枚举刷屏
  - 保留关键信息：缓冲区大小、offload 统计、警告、错误
- **影响：** 减少 ~80% 日志输出，不影响性能

---

## 🔍 性能问题分析总结

### 关键转折点：

1. **版本 14 (eabec8f) - batch size 512 → 128**
   - 这是一个关键改动！batch size 从 512 降到 128
   - 虽然减少了内存，但可能**降低了吞吐量**
   - 官方工具确实用 128，但你之前 30 tokens/s 时用的是什么？

2. **版本 06 (c993fa8) - 添加 use_mmap=false 和 flash_attn**
   - 这两个参数对性能有重大影响
   - use_mmap=false 可能会增加内存使用但提高速度

3. **版本 04 (239743c) - 禁用 OpenCL，仅用 Hexagon**
   - 从这个版本开始不再使用 GPU
   - 仅依赖 Hexagon NPU
   - 这可能是性能下降的原因之一

4. **版本 08 (5060494) - 线程池 CPU 亲和性**
   - 使用 threadpool API 限制 CPU 核心
   - 这个改动实际上导致性能**下降** (11.96 → 4.89 tokens/s)
   - 版本 07 (a9db1ec) 又回退了这个改动

5. **版本 09 (d34d207) - 禁用 EGL init**
   - 因为崩溃禁用了 EGL
   - 导致无法使用 OpenCL GPU

### 建议检查点：

1. **你之前 30 tokens/s 时的 batch size 是多少？**
   - 如果是 512，那么版本 14 的改动（512→128）可能是罪魁祸首

2. **你之前 30 tokens/s 时是否使用了 OpenCL + Hexagon 双加速器？**
   - 如果是，那么版本 04 禁用 OpenCL 导致了性能下降

3. **你之前 30 tokens/s 时的 use_mmap 和 flash_attn 设置是什么？**
   - 版本 06 添加了这些参数，可能影响性能

### 建议尝试：

1. 尝试回退到版本 14 之前（恢复 batch_size = 512）
2. 尝试启用 OpenCL + Hexagon 双加速器（版本 05 的配置）
3. 检查 use_mmap 和 flash_attn 的最佳组合

---

**文件位置：**
所有 15 个版本的 CPP 文件都在 `jni_history_versions/` 文件夹中，编号从 01 (最新) 到 15 (最旧)。
