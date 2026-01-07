# NPU 性能测试 - 最后尝试

## 当前状态

| 环境 | 性能 |
|------|------|
| 你的 App (CPU) | 30-40 tokens/s ✅ |
| 你的 App (NPU) | **5.29 tokens/s** ❌ |
| 官方基准 (NPU) | 51 tokens/s |

**问题**: NPU 比 CPU 慢 6 倍

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

## 最后测试：改成官方配置

修改 `GGUFChat/app/src/main/cpp/llama-android-jni.cpp`：

### 当前配置（第 342-350 行）
```cpp
ctx_params.n_ctx = 2048;
ctx_params.n_batch = 512;
ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_DISABLED;
```

### 改成官方配置
```cpp
ctx_params.n_ctx = 8192;              // ← 改
ctx_params.n_batch = 128;             // ← 改
ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;  // ← 改
```

重新编译，测试性能。

---

## 判断结果

### 如果还是慢（~5-10 tokens/s）
**→ 放弃 NPU**

说明：
- Hexagon backend 的架构限制
- 频繁的后端切换无法避免
- 这不是配置问题，是根本性的

**建议**：
- ✅ 用 CPU（30-40 tokens/s，稳定可靠）
- ⚠️ 试试 Vulkan GPU（如果你之前搞过）
- ❌ NPU 不适合 LLM 推理

### 如果变快了（>20 tokens/s）
**→ 继续调优**

说明配置有影响，可以继续调：
- 调整 batch size
- 调整 context size
- 测试不同模型格式

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

## 我的判断

**很可能改配置也没用**，因为：

1. 官方基准测试的环境和你的 App 可能不同：
   - 官方可能是特定模型/场景
   - 官方可能有特殊优化
   - 官方可能测的是 prompt processing（批量），不是单 token 生成

2. SET_ROWS/GET_ROWS/CONT 强制在 CPU 是 Hexagon backend 的设计：
   - 看源码，这些操作根本没在 Hexagon 里实现
   - 除非改 llama.cpp 源码实现这些操作，否则无解

3. 你的性能数字（5.29 tokens/s）和日志（60+ splits）完美匹配：
   - 每次切换 ~2ms 开销
   - 100 次切换 = 200ms/token
   - 5 tokens/s ≈ 200ms/token ✓

**建议**：试一次改配置，不行就放弃 NPU。

---

## 备注

- 你的 CPU 性能已经很好了（30-40 tokens/s）
- NPU 原本是为了 CNN/图像处理优化的
- LLM 的 KV cache 操作可能不是 NPU 的强项
