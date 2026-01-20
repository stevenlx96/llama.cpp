# JNI CPP 文件版本 31-45 改动详解

**目的：** 追溯更早期的开发历史，了解 Hexagon NPU 集成的初始阶段

---

## 版本 31 - 4a43490
**文件：** version_31_4a43490.cpp
**标题：** Fix device pointer issue causing crash in ggml_backend_dev_get_props

### 改动说明
- **问题：** 设备指针处理不当导致在 `ggml_backend_dev_get_props` 中崩溃
- **症状：** 访问设备属性时出现段错误
- **修复：**
  - 正确处理设备指针的生命周期
  - 添加空指针检查
  - 确保设备在使用前已正确初始化
- **影响：** 修复崩溃问题，提高稳定性，不直接影响性能

---

## 版本 32 - 71d1b6c
**文件：** version_32_71d1b6c.cpp
**标题：** Explicitly specify Hexagon device for model loading

### 改动说明
- **问题：** 模型加载时没有明确指定使用哪个设备
- **改动内容：**
  1. 在模型加载参数中显式指定 Hexagon 设备
  2. 不依赖自动设备选择
  3. 确保模型加载到正确的 NPU 设备上
- **技术细节：**
  - 使用 `llama_model_params.devices` 数组明确指定设备
  - 设置 Hexagon 为主要设备
- **影响：** 确保模型正确加载到 NPU，避免意外使用 CPU

---

## 版本 33 - 011d373
**文件：** version_33_011d373.cpp
**标题：** Fix Hexagon session failure and filter verbose logs

### 改动说明
- **问题：**
  1. Hexagon 会话初始化失败
  2. 大量详细日志影响调试
- **改动内容：**
  1. **会话修复：**
     - 修复 Hexagon 会话初始化流程
     - 添加会话失败的错误处理
     - 确保会话在使用前已就绪
  2. **日志过滤：**
     - 过滤重复的详细日志
     - 保留关键错误和警告信息
     - 减少日志输出，提高可读性
- **影响：** 提高 Hexagon 会话稳定性，改善日志可读性

---

## 版本 34 - e23ed91
**文件：** version_34_e23ed91.cpp
**标题：** Fix FastRPC file:// URI format for absolute paths

### 改动说明
- **问题：** FastRPC 的文件 URI 格式不正确
- **根本原因：**
  - Android 上的 FastRPC 需要特定的 `file://` URI 格式
  - 绝对路径处理不当导致无法加载共享库
- **解决方案：**
  - 修正 `file://` URI 格式
  - 确保使用绝对路径：`file:///data/local/tmp/...`
  - 处理路径中的特殊字符
- **技术细节：**
  - 正确的格式：`file://` + 绝对路径
  - 错误的格式：`file://` + 相对路径 或缺少斜杠
- **影响：** **关键修复** - 修复 FastRPC 库加载失败问题

---

## 版本 35 - 59b4d23
**文件：** version_35_59b4d23.cpp
**标题：** Add compilation error solution guide

### 改动说明
- **改动内容：**
  - 添加注释和文档，说明常见编译错误的解决方案
  - 包括头文件缺失、链接错误等问题
  - 提供快速参考指南
- **包含的问题和解决方案：**
  1. 缺少 llama.h、ggml.h 等头文件
  2. 链接器错误 - 缺少库
  3. ABI 不匹配问题
  4. NDK 版本兼容性
- **影响：** 文档改进，不影响性能或功能

---

## 版本 36 - 25428ca
**文件：** version_36_25428ca.cpp
**标题：** Fix Hexagon NPU to work in Android APK without root

### 改动说明
- **问题：**
  - 之前的实现需要 root 权限
  - 在非 root Android 设备上 Hexagon NPU 无法工作
  - APK 无法直接使用 NPU 加速
- **根本原因：**
  1. FastRPC 库路径需要在应用的私有目录
  2. 权限问题 - 无法访问 `/data/local/tmp`
  3. SELinux 限制
- **解决方案：**
  1. 使用应用私有目录存放 FastRPC 库
  2. 使用 `context.getFilesDir()` 获取可写路径
  3. 从 assets 复制库到应用目录
  4. 修改 FastRPC 加载路径
- **技术细节：**
  - 从：`/data/local/tmp/libhexagon_interface.so`
  - 到：`/data/data/<package>/files/libhexagon_interface.so`
- **影响：** **重大改进** - 使 NPU 能在普通（非 root）设备上工作

---

## 版本 37 - a77b1b9
**文件：** version_37_a77b1b9.cpp
**标题：** Add comprehensive Hexagon NPU solution analysis for Android APK

### 改动说明
- **改动内容：**
  - 添加详细的架构分析注释
  - 记录 Hexagon NPU 在 Android APK 中的完整解决方案
  - 包括所有发现的问题和解决方法
- **文档内容：**
  1. **FastRPC 架构：**
     - 客户端-服务器模型
     - 用户空间到内核空间通信
     - DSP/NPU 卸载机制
  2. **权限和路径要求：**
     - SELinux 上下文
     - 文件系统权限
     - 应用沙箱限制
  3. **性能优化建议：**
     - 内存对齐
     - 缓冲区管理
     - 异步操作
- **影响：** 文档和架构改进，为后续开发提供指导

---

## 版本 38 - 0cb8761
**文件：** version_38_0cb8761.cpp
**标题：** Add llama.cpp native libraries for arm64-v8a

### 改动说明
- **改动内容：**
  - 添加预编译的 llama.cpp 原生库
  - 支持 arm64-v8a 架构（64位 ARM）
  - 包括所有必需的 .so 文件
- **包含的库：**
  1. `libllama.so` - 核心 llama.cpp 库
  2. `libggml-base.so` - GGML 基础库
  3. `libggml-cpu.so` - CPU 后端
  4. `libggml-hexagon.so` - Hexagon NPU 后端
- **构建配置：**
  - 架构：arm64-v8a
  - API 级别：Android 28+
  - 启用 Hexagon 支持
- **影响：** 简化构建流程，但使用预编译库可能不是最优

---

## 版本 39 - b332b53
**文件：** version_39_b332b53.cpp
**标题：** Merge pull request #3 from stevenlx96/claude/add-npu-inference-wCm3z

### 改动说明
- **改动内容：**
  - 合并 NPU 推理功能分支
  - 整合所有 NPU 相关改动
- **合并的功能：**
  1. Hexagon NPU 后端集成
  2. FastRPC 支持
  3. Android APK 兼容性修复
  4. 性能优化
- **影响：** 里程碑版本 - NPU 功能正式集成到主分支

---

## 版本 40 - d7ebc61
**文件：** version_40_d7ebc61.cpp
**标题：** Disable llama-android module - only build app module

### 改动说明
- **问题：**
  - 构建系统中的 `llama-android` 模块导致冲突
  - 重复的依赖和链接问题
- **解决方案：**
  - 禁用 `llama-android` 模块
  - 仅构建主应用模块
  - 简化构建配置
- **改动内容：**
  - 修改 `settings.gradle`
  - 移除 llama-android 模块引用
  - 直接在 app 模块中集成所有代码
- **影响：** 简化构建，减少潜在冲突

---

## 版本 41 - ab99dcb
**文件：** version_41_ab99dcb.cpp
**标题：** CRITICAL FIX: Use prebuilt .so with MATCHING Docker compilation flags

### 改动说明
- **问题：**
  - 本地编译的 .so 文件与运行时环境不兼容
  - ABI 不匹配导致崩溃
  - 符号未定义错误
- **根本原因：**
  1. 编译器标志不一致
  2. NDK 版本差异
  3. 优化级别不同
  4. 硬件特性标志（NEON、FP16 等）不匹配
- **解决方案：**
  - 使用 Docker 容器确保编译环境一致
  - 匹配官方示例的编译标志
  - 使用相同的 NDK 版本
  - 启用相同的硬件特性
- **关键编译标志：**
  ```cmake
  -DGGML_HEXAGON=ON
  -DANDROID_ABI=arm64-v8a
  -DANDROID_PLATFORM=android-28
  -DCMAKE_BUILD_TYPE=Release
  ```
- **影响：** **关键修复** - 解决 ABI 不匹配问题

---

## 版本 42 - 03fb916
**文件：** version_42_03fb916.cpp
**标题：** CRITICAL FIX: Build from source like official example (not prebuilt .so)

### 改动说明
- **问题：**
  - 预编译的 .so 文件仍然有兼容性问题
  - 难以调试和定制
  - 版本不匹配
- **改变策略：**
  - **从预编译 → 从源代码构建**
  - 模仿官方 llama.cpp Android 示例
  - 完全控制构建过程
- **改动内容：**
  1. 添加 llama.cpp 作为子模块或源代码
  2. 修改 CMakeLists.txt 从源代码构建
  3. 确保所有依赖都从源代码编译
  4. 使用与官方示例相同的 CMake 配置
- **优势：**
  - 100% 兼容性
  - 可调试
  - 易于更新
  - 可自定义优化
- **影响：** **架构改变** - 从预编译切换到源代码构建

---

## 版本 43 - 725cea8
**文件：** version_43_725cea8.cpp
**标题：** SIMPLIFIED: Match official example EXACTLY - use default params

### 改动说明
- **问题：**
  - 过度优化和自定义导致问题
  - 与官方示例行为不一致
  - 难以排查问题根源
- **解决方案：**
  - **极简方案 - 完全匹配官方示例**
  - 移除所有自定义配置
  - 使用默认参数
- **恢复为默认的参数：**
  1. `n_threads` = 默认（自动检测）
  2. `n_batch` = 512（默认）
  3. `n_ctx` = 512（默认）
  4. 移除所有自定义优化
  5. 不强制设备选择
- **改动内容：**
  - 移除 CPU 亲和性代码
  - 移除自定义 batch size
  - 移除 flash attention 强制启用
  - 移除 mmap 配置
  - 让 llama.cpp 使用内部默认值
- **理念：** 先让它工作，再优化
- **影响：** **策略转变** - 可能性能不是最优，但稳定性提高

---

## 版本 44 - 8ecdb9e
**文件：** version_44_8ecdb9e.cpp
**标题：** CRITICAL FIX: Fix batch size > context size crash + disable repack logs

### 改动说明
- **问题：**
  1. **崩溃问题：** batch size (512) > context size (512) 导致崩溃
  2. **日志刷屏：** "repack" 日志重复出现
- **根本原因：**
  - 当 n_batch == n_ctx 时，边界条件处理不当
  - llama_decode 的 batch 大小验证失败
  - KV cache 大小计算错误
- **解决方案 1 - 批处理大小：**
  - 确保 `n_batch < n_ctx` 或者正确处理相等情况
  - 设置 `n_batch = 256`，`n_ctx = 512`
  - 添加批处理大小验证
- **解决方案 2 - 日志过滤：**
  - 过滤 "repack" 相关的详细日志
  - 保留重要的调试信息
- **修复代码：**
  ```cpp
  ctx_params.n_ctx = 512;
  ctx_params.n_batch = 256;  // 确保 < n_ctx

  // 验证
  if (n_batch >= n_ctx) {
      // 调整或报错
  }
  ```
- **影响：** **关键稳定性修复** - 防止边界条件崩溃

---

## 版本 45 (最旧) - 302a44d
**文件：** version_45_302a44d.cpp
**标题：** CRITICAL FIX: Load backend plugins before initialization

### 改动说明
- **问题：**
  - Hexagon 后端未被识别
  - 后端初始化顺序错误
  - 插件加载失败
- **根本原因：**
  - llama.cpp 使用插件系统加载后端
  - 必须在初始化模型**之前**加载插件
  - 错误的顺序：
    ```
    错误：llama_model_load() → llama_backend_init() → 加载插件
    正确：llama_backend_init() → 加载插件 → llama_model_load()
    ```
- **解决方案：**
  1. 在最开始调用 `llama_backend_init()`
  2. 显式加载 Hexagon 后端插件
  3. 验证后端已注册
  4. 然后加载模型
- **修复代码顺序：**
  ```cpp
  // 1. 初始化后端系统
  llama_backend_init();

  // 2. 加载 Hexagon 插件
  ggml_backend_load("libggml-hexagon.so");

  // 3. 注册后端
  ggml_backend_register(...);

  // 4. 加载模型
  llama_model * model = llama_load_model_from_file(...);
  ```
- **影响：** **最基础的修复** - 这是使 Hexagon NPU 工作的第一步

---

## 🔍 版本 31-45 发展历程总结

### 阶段 1：初始集成 (版本 45-42)
**目标：** 让 Hexagon NPU 基本工作起来

1. **版本 45 (302a44d)** - 最基础的修复
   - 正确加载后端插件
   - 建立基本的 NPU 支持框架

2. **版本 44 (8ecdb9e)** - 稳定性修复
   - 修复 batch size 导致的崩溃
   - 确保基本参数正确

3. **版本 43 (725cea8)** - 简化策略
   - 移除所有自定义，使用默认参数
   - 先求稳定，再求性能

4. **版本 42 (03fb916)** - 架构决策
   - 从预编译切换到源代码构建
   - 获得完全控制权

### 阶段 2：兼容性解决 (版本 41-37)
**目标：** 解决 Android APK 环境下的各种问题

1. **版本 41 (ab99dcb)** - ABI 匹配
   - 使用 Docker 确保编译一致性
   - 解决符号未定义问题

2. **版本 40 (d7ebc61)** - 构建简化
   - 移除冲突的模块
   - 简化依赖关系

3. **版本 39 (b332b53)** - 功能合并
   - NPU 功能正式集成
   - 里程碑版本

4. **版本 38 (0cb8761)** - 库集成
   - 添加所有必需的原生库
   - 支持 arm64-v8a

5. **版本 37 (a77b1b9)** - 架构文档
   - 记录完整的解决方案
   - 为后续开发提供指导

### 阶段 3：Android 适配 (版本 36-34)
**目标：** 在非 root Android 设备上工作

1. **版本 36 (25428ca)** - **重大突破**
   - 使 NPU 在非 root 设备上工作
   - 解决权限和路径问题
   - 这是 APK 可用性的关键

2. **版本 35 (59b4d23)** - 文档改进
   - 添加编译错误解决指南
   - 帮助开发者快速解决问题

3. **版本 34 (e23ed91)** - FastRPC 修复
   - 修正文件 URI 格式
   - 解决共享库加载问题

### 阶段 4：稳定性提升 (版本 33-31)
**目标：** 提高会话稳定性和可靠性

1. **版本 33 (011d373)** - 会话管理
   - 修复 Hexagon 会话失败
   - 改善日志可读性

2. **版本 32 (71d1b6c)** - 设备控制
   - 明确指定使用 Hexagon 设备
   - 避免自动选择导致的问题

3. **版本 31 (4a43490)** - 指针安全
   - 修复设备指针崩溃
   - 提高代码健壮性

---

## 📊 关键转折点分析

### 1. **版本 45 (302a44d) - 起点**
- **意义：** NPU 支持的开始
- **后果：** 后续所有优化都基于此

### 2. **版本 42 (03fb916) - 架构决策**
- **决定：** 从预编译切换到源代码构建
- **影响：**
  - ✅ 优势：完全控制、易调试、可定制
  - ❌ 劣势：编译时间更长、维护更复杂

### 3. **版本 36 (25428ca) - 可用性突破**
- **成就：** 在非 root 设备上工作
- **意义：** 从开发工具变成可发布的应用
- **关键：** 解决 Android 安全沙箱限制

### 4. **版本 43 (725cea8) - 策略转变**
- **决定：** 简化配置，使用默认参数
- **理念：** "Make it work, then make it fast"
- **后果：** 稳定性提高，但可能牺牲了性能
- **与后续版本的关系：**
  - 版本 14 (eabec8f) 又开始调整 batch size
  - 版本 06 (c993fa8) 又添加了优化参数
  - 这表明从版本 43 到版本 01 是一个**逐步优化**的过程

---

## 🎯 对性能下降的启示

### 可能的罪魁祸首：

1. **版本 43 的简化策略**
   - 移除了所有优化可能是一个转折点
   - 之后虽然逐步添加优化，但可能没有找到最佳组合

2. **版本 42 的构建方式改变**
   - 从源代码构建可能带来了不同的编译优化
   - 需要检查编译标志是否最优

3. **版本 36 的路径改变**
   - 从 `/data/local/tmp` 到应用私有目录
   - 可能影响 FastRPC 性能

### 建议尝试：

1. **检查编译优化标志**
   - 版本 42 引入的从源代码构建可能需要特定的编译优化
   - 对比官方预编译库的编译标志

2. **测试 FastRPC 路径性能**
   - 版本 36 改变了库路径
   - 可以尝试在 root 设备上用回 `/data/local/tmp` 测试性能

3. **恢复某些优化参数**
   - 版本 43 移除的优化中，可能有些是有益的
   - 需要单独测试每个优化的影响

---

**文件位置：**
所有 15 个版本的 CPP 文件都在 `jni_history_versions_31_45/` 文件夹中，编号从 31 到 45。

**与前期版本的关系：**
- 版本 1-15：在 `jni_history_versions/` 目录
- 版本 16-30：在 `jni_history_versions_16_30/` 目录
- 版本 31-45：在 `jni_history_versions_31_45/` 目录（本文档）

版本号越小越新，版本 45 是最早的 NPU 集成尝试。
