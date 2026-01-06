# NPU 性能测试指南

## 当前状态

| 环境 | 性能 | 问题 |
|------|------|------|
| 你的 App (CPU) | 30-40 tokens/s | ✅ 正常 |
| 你的 App (NPU) | **5.29 tokens/s** | ❌ 比CPU慢6倍！|
| 官方基准 | 51 tokens/s | 目标 |

---

## 根本问题

Hexagon NPU backend **不支持**这些操作：
- `SET_ROWS` - KV cache 更新
- `GET_ROWS` - 残差连接
- `CONT` - Tensor 连续化

导致每个token要经过 60+ 次后端切换：
```
HTP0 计算 → CPU(SET_ROWS) → HTP0 → CPU(CONT) → HTP0 → CPU(GET_ROWS) → ...
```

虽然显示 "# 0 inputs"（无数据传输），但每次切换都要**同步等待**，性能崩溃。

---

## 测试方案：对比官方工具

### 方法1：用官方 llama-cli 测试（推荐）

**如果你有编译好的 llama-cli**（在 `build-xxx/bin/` 或 `pkg-xxx/bin/`）：

```bash
# 1. 下载测试模型
wget https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-Q4_0.gguf

# 2. Push 到手机
adb push llama-cli /data/local/tmp/
adb push *.so /data/local/tmp/lib/  # 你的 SO 文件
adb push Llama-3.2-1B-Instruct-Q4_0.gguf /data/local/tmp/

# 3. 运行官方配置测试
adb shell "
    cd /data/local/tmp
    LD_LIBRARY_PATH=/data/local/tmp/lib \
    ADSP_LIBRARY_PATH=/data/local/tmp/lib \
    GGML_HEXAGON_EXPERIMENTAL=1 \
    ./llama-cli --no-mmap -m Llama-3.2-1B-Instruct-Q4_0.gguf \
        -t 6 --cpu-mask 0xfc --cpu-strict 1 \
        --ctx-size 8192 --batch-size 128 -fa on \
        -ngl 99 --device HTP0 \
        -p '你好' -n 50
"
```

看输出里的：
```
eval time = XXXX ms / XX runs (XX.XX ms per token, XX.XX tokens per second)
```

---

### 方法2：改你的 App 配置匹配官方

如果没有 llama-cli，直接改你 App 的参数：

**修改这些参数**（在 `llama-android-jni.cpp`）：

```cpp
// 当前配置
ctx_params.n_ctx = 2048;
ctx_params.n_batch = 512;
ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_DISABLED;

// 改成官方配置
ctx_params.n_ctx = 8192;              // ← 改这个
ctx_params.n_batch = 128;             // ← 改这个
ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;  // ← 改这个
```

重新编译测试，看性能有没有变化。

---

## 判断结果

### 如果官方配置也慢（~5-10 tokens/s）
**→ NPU 对这个场景就是不行**

原因：
- Hexagon backend 缺少关键操作支持
- 频繁的后端切换导致同步开销巨大
- 这是 llama.cpp Hexagon backend 的设计限制

**建议**：
- 放弃 NPU
- 试试 Vulkan GPU（如果你之前搞过）
- 或者就用 CPU（30-40 tokens/s 也不慢）

### 如果官方配置快（~40-50 tokens/s）
**→ 配置问题，可以修**

说明：
- NPU 硬件是好的
- 是你当前的参数配置不对
- 继续调优参数可以达到目标性能

---

## 已验证的配置

### ✅ 正确的配置（已设置）
```cpp
model_params.n_gpu_layers = -1;           // 全部层 offload
model_params.use_extra_bufts = true;       // HTP0-REPACK
model_params.use_mmap = false;             // --no-mmap
ctx_params.offload_kqv = true;             // KV cache 在 NPU
```

### ⚠️ 可能需要调整的
```cpp
ctx_params.n_ctx = 2048;        // 官方用 8192
ctx_params.n_batch = 512;       // 官方用 128
ctx_params.flash_attn = OFF;    // 官方用 ON
```

---

## 技术细节

### HTP0-REPACK 已成功
```
load_tensors: HTP0-REPACK model buffer size = 702.84 MiB  ✓
load_tensors: offloaded 29/29 layers to GPU               ✓
```

### 但是执行时切换太多
```
SPLIT #110: CPU  # 0 inputs (SET_ROWS x2)
SPLIT #111: HTP0 # 0 inputs (MUL_MAT, SOFT_MAX)
SPLIT #112: CPU  # 0 inputs (CONT)
SPLIT #113: HTP0 # 1 inputs (MUL_MAT)
SPLIT #114: CPU  # 0 inputs (GET_ROWS x2)
... 每层重复 ...
```

每个 token = 29层 × 3-4次切换 = **~100次同步**

---

## 下一步

1. **先测试**：看官方配置或官方工具的性能
2. **告诉我结果**：多少 tokens/s
3. **做决定**：
   - 慢 → 放弃 NPU
   - 快 → 继续调参数

---

## 备注

- 使用 1B 模型测试（小，快）
- 如果想测 3B：`Llama-3.2-3B-Instruct-Q4_0.gguf`
- CPU mask `0xfc` = 核心 2-7（避开系统核心 0-1）
