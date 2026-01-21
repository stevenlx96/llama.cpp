# Hexagon NPU性能诊断手册

## 问题：测试了45个JNI CPP版本，都没有达到30 token/s

## 根本原因分析

### 编译流程澄清

```
┌─────────────────────────────────────────────┐
│  预编译核心库 (libggml*.so, libllama.so)    │
│  - 包含Hexagon NPU后端实现                   │
│  - 从官方release或自己编译llama.cpp获取      │
│  - 这些库决定了是否支持NPU以及性能如何       │
└──────────────────┬──────────────────────────┘
                   │ 被依赖
                   ↓
┌─────────────────────────────────────────────┐
│  JNI包装层 (llama-android-jni.cpp)          │
│  - 只是Java和C++之间的接口翻译               │
│  - 负责参数传递、错误处理、UTF-8验证等       │
│  - 不包含推理逻辑或后端实现                  │
│  - 编译成libllama-android.so                │
└─────────────────────────────────────────────┘
```

**关键认知：**
- ✅ 修改JNI CPP可以改变：参数配置、错误处理、日志输出
- ❌ 修改JNI CPP无法改变：Hexagon后端实现、推理性能、后端bug

## 诊断步骤

### 步骤1：验证.so文件是否真正包含Hexagon支持

```bash
# 1. 下载真正的.so文件（如果是Git LFS指针）
cd /path/to/GGUFChat/app/src/main/jniLibs/arm64-v8a/
file libggml-hexagon.so

# 如果输出是 "ASCII text"，说明是Git LFS指针，需要下载真实文件：
git lfs pull

# 2. 检查.so文件大小
ls -lh libggml-hexagon.so
# 应该是几MB，不是130字节

# 3. 检查符号表
nm -D libggml-hexagon.so | grep -i "ggml_backend_hexagon"
# 应该看到hexagon相关的函数符号

# 4. 检查所有Hexagon相关库
ls -lh libggml-htp-*.so
# 应该都是真实的二进制文件，不是指针
```

### 步骤2：确认当前使用的.so文件来源

**问题：** 你说"从官方编译拿到了so文件"，但是：

1. **官方release的.so文件**
   - 位置：GitHub releases页面
   - 优点：经过测试，应该支持Hexagon
   - 缺点：版本可能不是最新

2. **自己编译的.so文件**
   - 位置：本地llama.cpp编译产物
   - 优点：可以控制版本和编译选项
   - 缺点：需要正确配置Hexagon SDK和编译参数

**你现在用的是哪一个？**

### 步骤3：检查当前JNI CPP版本

```bash
cd /path/to/GGUFChat
head -50 llama-android/src/main/cpp/llama-android-jni.cpp
```

关键检查点：
- [ ] use_mmap = false
- [ ] flash_attn = ENABLED
- [ ] batch_size = 128
- [ ] n_gpu_layers = 999
- [ ] device = 仅Hexagon（不用OpenCL）

### 步骤4：查看运行时日志

关键日志标记：
```
✅ 好的迹象：
- "HTP0 available"
- "offload 28/28 layers to HTP0"
- "compute buffer: HTP0 X MiB"

❌ 坏的迹象：
- "HTP0 not available"
- "offload 0/28 layers"
- "Failed to load backend"
- "fallback to CPU"
```

## 可能的根本原因

### 原因1：预编译.so文件版本不匹配

**症状：**
- 版本36开始DSP路径有问题
- 测试到30也没有达到30 token/s

**解释：**
- 预编译.so文件可能是旧版本，不支持最新的Hexagon API
- 或者编译时没有启用Hexagon支持
- JNI CPP调用了.so中不存在的函数

**解决方案：**
1. 确认.so文件的编译日期和llama.cpp版本
2. 重新从官方release下载最新的Android .so文件
3. 或者自己编译llama.cpp，确保启用Hexagon

### 原因2：.so文件和JNI CPP的API不匹配

**症状：**
- 程序崩溃或挂起
- 日志中有"undefined symbol"错误

**解释：**
- JNI CPP使用了新API（如async_copy），但.so文件中没有
- 需要同步更新

**解决方案：**
1. 确保.so文件和JNI CPP来自同一个llama.cpp版本
2. 检查头文件（include/llama.h）是否匹配.so文件

### 原因3：Hexagon权限或环境问题

**症状：**
- .so文件正确，但运行时Hexagon不可用
- 日志显示"HTP0 not available"

**可能原因：**
- Android权限不足
- Hexagon DSP驱动未加载
- 设备不支持或已禁用Hexagon

**解决方案：**
1. 检查AndroidManifest.xml权限
2. 运行adb shell命令测试Hexagon可用性
3. 使用官方llama-cli工具对比

### 原因4：配置参数不是最优

**官方30+ token/s配置：**
```cpp
// 模型参数
model_params.use_mmap = false;     // 关键！

// 上下文参数
ctx_params.n_batch = 128;          // 不是512！
ctx_params.n_ubatch = 128;
ctx_params.flash_attn = ENABLED;   // 关键！
ctx_params.n_ctx = 8192;

// 设备配置
device_array[0] = hexagon_dev;     // 仅Hexagon
device_array[1] = nullptr;         // 不用OpenCL！

// offload
ctx_params.n_gpu_layers = 999;     // 全部offload
```

## 建议的测试策略

### 策略A：从已知的"好版本"开始

根据commit历史，**版本06 (c993fa8)** 添加了关键参数以匹配30 token/s配置。

```bash
# 1. 检出版本06的JNI CPP
cp jni_history_versions/version_06_c993fa8.cpp \
   GGUFChat/llama-android/src/main/cpp/llama-android-jni.cpp

# 2. 确保使用匹配的.so文件（从官方release下载）

# 3. 编译并测试
cd GGUFChat
./gradlew assembleDebug

# 4. 安装并运行，检查日志
```

### 策略B：逐步二分查找

如果有多个"可疑版本"，使用二分法：

```
测试版本顺序：
1. 版本15 (最早)
2. 版本23 (中间)
3. 根据结果选择上半部或下半部
4. 继续二分...
```

### 策略C：对比官方工具

```bash
# 1. 使用官方llama-cli工具测试（作为baseline）
adb push llama-cli /data/local/tmp/
adb push model.gguf /data/local/tmp/
adb shell
cd /data/local/tmp
./llama-cli --no-mmap -fa on --batch-size 128 -ngl 99 --device HTP0 \
  -m model.gguf -p "Hello" -n 100

# 记录性能：X tokens/s

# 2. 运行你的app，对比性能
# 如果官方工具也只有10 tokens/s → 设备/驱动问题
# 如果官方工具有30 tokens/s → .so文件或配置问题
```

## 最可能的解决方案

基于你的描述（"到36开始DSP路径有问题"），**最可能的原因是：**

### 🎯 预编译.so文件不支持或有bug

**立即检查：**

```bash
# 1. 确认当前.so文件是否真实存在
cd GGUFChat/app/src/main/jniLibs/arm64-v8a/
ls -lh *.so

# 如果都是130字节 → 需要下载真实文件！
# 如果是正常大小 → 检查编译时间和版本

# 2. 获取官方编译的.so文件
# 方法A：从GitHub release下载
# https://github.com/ggerganov/llama.cpp/releases

# 方法B：使用Git LFS下载
git lfs install
git lfs pull

# 方法C：自己编译llama.cpp（确保启用Hexagon）
cd /path/to/llama.cpp
mkdir build-android && cd build-android
cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DGGML_HEXAGON=ON \
  -DGGML_OPENCL=ON
make -j8

# 编译产物在 build-android/ggml/src/ 和 build-android/src/
```

## 总结

### ✅ 你的编译流程是正确的

你理解了JNI包装层需要重新编译。这是对的！

### ❌ 但是你忽略了.so文件本身

**关键认知：**
- 修改JNI CPP只能改变"接口层"
- 核心性能由预编译.so文件决定
- 如果.so文件有bug或不支持Hexagon，改多少次JNI CPP都没用

### 🔍 下一步

1. **立即检查：** 你的.so文件是130字节的Git LFS指针，还是真正的二进制文件？
2. **如果是指针：** 运行`git lfs pull`或从release下载真实文件
3. **如果是真实文件：** 确认编译时间和llama.cpp版本，确保支持Hexagon
4. **测试baseline：** 使用官方llama-cli工具测试，对比性能

### 🎯 最终目标

找到一组**匹配的**组合：
- ✅ 支持Hexagon的.so文件（libggml-hexagon.so等）
- ✅ 正确配置的JNI CPP（use_mmap=false, flash_attn=on, batch=128）
- ✅ 正确的设备配置（仅Hexagon，不用OpenCL）
- ✅ 正确的Android环境（权限、驱动）

只有这四者**同时满足**，才能达到30 token/s！
