# NPU 性能测试 Demo

## 目的

对比官方 llama.cpp 和你的 GGUFChat App 的 NPU 性能，找出为什么慢。

---

## 当前性能对比

| 环境 | 性能 | 备注 |
|------|------|------|
| **官方基准测试** | 51 tokens/s | Llama-3.2-1B Q4_0 |
| **你的 GGUFChat (CPU)** | 30-40 tokens/s | 正常 |
| **你的 GGUFChat (NPU)** | **5.29 tokens/s** | ❌ 太慢！|

---

## 测试步骤：复现官方基准

### 1. 在 Docker 中编译完整包

```bash
cd /home/user/llama.cpp

# 复制 CMake 预设
cp docs/backend/hexagon/CMakeUserPresets.json .

# 用 Docker 编译（包含所有工具）
docker run --rm \
    --volume "$(pwd):/workspace" \
    --platform linux/amd64 \
    ghcr.io/snapdragon-toolchain/arm64-android:v0.3 \
    bash -c "
        cd /workspace
        cmake --preset arm64-android-snapdragon-release -B build-snapdragon
        cmake --build build-snapdragon -j\$(nproc)
        cmake --install build-snapdragon --prefix pkg-adb/llama.cpp
    "
```

### 2. 下载测试模型（和官方一样）

```bash
# 下载 1B 模型（小，快）
wget https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-Q4_0.gguf
```

### 3. Push 到手机

```bash
# Push 编译好的工具和库
adb push pkg-adb/llama.cpp /data/local/tmp/

# Push 模型
adb shell mkdir -p /data/local/tmp/gguf
adb push Llama-3.2-1B-Instruct-Q4_0.gguf /data/local/tmp/gguf/
```

### 4. 运行官方测试命令（完全相同的参数）

```bash
adb shell "
    cd /data/local/tmp/llama.cpp
    LD_LIBRARY_PATH=/data/local/tmp/llama.cpp/lib \
    ADSP_LIBRARY_PATH=/data/local/tmp/llama.cpp/lib \
    GGML_HEXAGON_EXPERIMENTAL=1 \
    ./bin/llama-cli --no-mmap -m /data/local/tmp/gguf/Llama-3.2-1B-Instruct-Q4_0.gguf \
        --poll 1000 -t 6 --cpu-mask 0xfc --cpu-strict 1 \
        --ctx-size 8192 --batch-size 128 -fa on \
        -ngl 99 -no-cnv --device HTP0 \
        -p '你好，请介绍一下你自己。' -n 50
"
```

### 5. 看结果

找这行：
```
llama_perf_context_print: eval time = XXXX ms / XX runs (XX.XX ms per token, XX.XX tokens per second)
```

**关键数字**: `XX.XX tokens per second`

---

## 判断结果

### 情况A: 官方工具也慢（~5-10 tokens/s）
**结论**: NPU 本身就不适合这个场景
- 可能是 Hexagon backend 的 SET_ROWS/GET_ROWS/CONT 操作强制在 CPU 导致频繁切换
- 可能是 S25 NPU 硬件限制
- **建议**: 放弃 NPU，用 Vulkan GPU 或者就用 CPU

### 情况B: 官方工具快（~40-50 tokens/s）
**结论**: 你的 JNI wrapper 或 App 配置有问题
- 需要检查 JNI 调用是否有额外同步点
- 需要对比官方 C++ 代码和你的 Android 实现的差异
- **建议**: 继续 debug，有希望修复

---

## 已知问题汇总

### NPU 确实在工作，但是慢：
- ✅ HTP0-REPACK 成功（702.84 MiB）
- ✅ 29 层全部 offload 到 NPU
- ✅ `offload_kqv = true` 设置
- ❌ SET_ROWS/GET_ROWS/CONT 操作被强制到 CPU
- ❌ 导致每个 token 有 60+ 个 split（HTP0 → CPU → HTP0...）

### 根本原因：
Hexagon NPU backend **不支持** 这些关键操作：
- `GGML_OP_SET_ROWS` - KV cache 更新
- `GGML_OP_GET_ROWS` - 残差连接
- `GGML_OP_CONT` - Tensor 连续化

虽然这些操作显示 "# 0 inputs"（无数据传输），但每次切换都需要**同步**，导致性能崩溃。

---

## 下一步

1. **先运行上面的测试**，看官方工具的性能
2. 把结果告诉我
3. 根据结果决定是继续 debug 还是放弃 NPU

---

## 备注

- 测试用的是 1B 模型，比你 App 可能用的 3B 模型小
- 如果想测试 3B：下载 `Llama-3.2-3B-Instruct-Q4_0.gguf`
- 官方脚本在 `scripts/snapdragon/adb/run-completion.sh`
