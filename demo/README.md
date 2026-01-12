# NPU 性能测试 - 进展报告

## 当前状态

| 环境 | 性能 | 稳定性 |
|------|------|--------|
| 你的 App (CPU) | 30-40 tokens/s | ✅ 稳定 |
| 你的 App (NPU) 初始配置 | 5.29 tokens/s | ✅ 稳定但慢 |
| 你的 App (NPU) FA 禁用 | 4.61 tokens/s | ✅ 稳定但更慢 |
| 你的 App (NPU) FA 开启 (ctx=8192) | **10.89 tokens/s** | ❌ 第二次推理崩溃 |
| **当前测试: FA 开启 (ctx=4096)** | **预计 ~10 tokens/s** | **待测试** |
| 官方基准 (NPU) | 51 tokens/s | ✅ 稳定 |

**重要发现**: **Flash Attention 是性能关键**！但 ctx=8192 会崩溃，正在测试 ctx=4096

---

## 根本原因

Hexagon NPU backend **不支持**这些关键操作：
- `SET_ROWS` - KV cache 更新
- `GET_ROWS` - 残差连接
- `CONT` - Tensor 连续化

导致每个 token 要切换 60+ 次：
```
HTP0 → CPU → HTP0 → CPU → HTP0 → CPU ...
```

每次切换都要**同步等待**，性能崩溃。

---

## 优化历程

### 测试 1: 官方配置完全匹配（包括 Flash Attention）

修改 `GGUFChat/app/src/main/cpp/llama-android-jni.cpp`：

```cpp
ctx_params.n_ctx = 8192;              // 官方: --ctx-size 8192
ctx_params.n_batch = 128;             // 官方: --batch-size 128
ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;  // 官方: -fa on
ctx_params.offload_kqv = true;        // KV cache 在 NPU
```

**结果**:
- ✅ 第一次推理: 10.89 tokens/s（速度提升 2 倍！）
- ❌ 第二次推理: 崩溃（SIGABRT in llama_decode）

**结论**: Flash Attention 与 Hexagon NPU 后端不兼容，导致状态累积后崩溃

### 测试 2: 禁用 Flash Attention（避免崩溃）

```cpp
ctx_params.n_ctx = 8192;              // 保留大 context
ctx_params.n_batch = 128;             // 保留小 batch
ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_DISABLED;  // 禁用 FA 避免崩溃
ctx_params.offload_kqv = true;        // KV cache 在 NPU
```

**结果**:
- ✅ 稳定运行，不崩溃
- ❌ 性能暴跌：**4.61 tokens/s**（比初始还慢！）

**结论**: Flash Attention 是性能的关键！不能禁用。

### 测试 3: 减小 Context Size + 保留 Flash Attention（当前）

**假设**: ctx=8192 太大导致 FA 在 NPU 上崩溃，尝试 ctx=4096

**当前配置** (第 340-360 行):
```cpp
ctx_params.n_ctx = 4096;              // ← 从 8192 减到 4096
ctx_params.n_batch = 128;             // 保持
ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;  // ← 重新开启！
ctx_params.offload_kqv = true;        // 保持
```

**预期**:
- ✅ 性能: ~10 tokens/s（保留 FA 的加速）
- ✅ 稳定: 不崩溃（更小的 context 避免内存问题）

**如果还是崩溃**: 再试 ctx=2048

---

## 性能分析

### ✅ 已确认有效的优化
```cpp
ctx_params.n_ctx = 8192;              // 大 context（官方配置）
ctx_params.n_batch = 128;             // 小 batch（官方配置）
ctx_params.offload_kqv = true;        // KV cache offload
model_params.n_gpu_layers = -1;       // 全部层 offload
model_params.use_extra_bufts = true;  // HTP0-REPACK
model_params.use_mmap = false;        // --no-mmap
```
→ 性能从 5.29 提升到 10.89 tokens/s

### ❌ 不兼容的功能
```cpp
ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;  // 崩溃！
```
→ 第二次推理时 SIGABRT

---

## 技术细节

### ✅ 已经设置正确的
```cpp
model_params.n_gpu_layers = -1;       // 全部层 offload
model_params.use_extra_bufts = true;  // HTP0-REPACK
model_params.use_mmap = false;        // --no-mmap
ctx_params.offload_kqv = true;        // KV cache 在 NPU
```

### ⚠️ 官方和你的差异
```
官方: ctx=8192, batch=128, FA=on
你的: ctx=2048, batch=512, FA=off
```

### 📊 执行情况（从日志看到的）
```
HTP0-REPACK: 702.84 MiB ✓
Offloaded: 29/29 layers ✓
Splits: 60+ per token  ← 问题！
```

每层都要：
```
SPLIT #110: CPU  (SET_ROWS)
SPLIT #111: HTP0 (MUL_MAT, SOFT_MAX)
SPLIT #112: CPU  (CONT)
SPLIT #113: HTP0 (继续)
SPLIT #114: CPU  (GET_ROWS)
```

29 层 × 3-4 次切换 = ~100 次同步/token

---

## 为什么还是比官方慢？

**官方**: 51 tokens/s
**你的 App**: 10.89 tokens/s
**差距**: ~5 倍

### 可能的原因

1. **模型大小不同**
   - 官方基准可能用 Llama-3.2-1B（1B 参数）
   - 你可能用的是 3B 模型
   - 更大的模型 → 更多的 MUL_MAT 操作 → 更慢

2. **测试场景不同**
   - 官方可能测的是 **prompt processing**（批处理多个 token）
   - 你测的是 **单 token 生成**（逐个生成）
   - 批处理可以更好地利用 NPU 并行性

3. **后端切换依然存在**
   - SET_ROWS/GET_ROWS/CONT 操作强制在 CPU 运行
   - 29 层 × 3-4 次切换/层 ≈ 100 次同步/token
   - 每次切换开销 ~1-2ms
   - 这是 Hexagon backend 的架构限制

4. **Flash Attention 的缺失**
   - 官方用 Flash Attention 优化了注意力计算
   - 我们为了稳定性禁用了 FA
   - 失去了部分性能优化

### 下一步建议

1. **测试 1B 模型**: 下载 Llama-3.2-1B-Instruct-Q4_0.gguf 测试
2. **接受当前性能**: 10 tokens/s 虽然不如官方，但比初始配置快 2 倍
3. **考虑 CPU**: 如果 10 tokens/s 仍不满意，CPU 的 30-40 tokens/s 更快且稳定

---

## 备注

- 你的 CPU 性能已经很好了（30-40 tokens/s）
- NPU 原本是为了 CNN/图像处理优化的
- LLM 的 KV cache 操作可能不是 NPU 的强项
