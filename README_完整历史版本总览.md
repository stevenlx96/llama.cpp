# JNI CPP 完整历史版本总览（1-30）

## 📊 所有 30 个版本的完整清单

### 📁 版本 1-15（最新）
位置：`jni_history_versions/`
详细分析：`README_改动详解.md`

| 版本 | Commit | 描述 | 关键改动 |
|------|--------|------|---------|
| **version_01** | 07e7132 | Fix build error: Change async_copy to async | 修复编译错误 |
| **version_02** | f6358e3 | Add comprehensive backend diagnostic logging | 添加诊断日志 |
| **version_03** | 7fb2ea7 | LAYER split mode with dual accelerators | 启用双加速器（LAYER 模式）|
| **version_04** | 239743c | CRITICAL: Disable OpenCL to prevent deadlock | ⚠️ 禁用 OpenCL |
| **version_05** | 0f7ee5d | Enable dual accelerators (OpenCL + Hexagon) | 启用双加速器 |
| **version_06** | c993fa8 | Add use_mmap=false and flash_attn | ⚠️ 添加关键参数 |
| **version_07** | a9db1ec | Revert thread pool CPU affinity | 回退 CPU 亲和性 |
| **version_08** | 5060494 | Implement thread pool CPU affinity | 实现 CPU 亲和性 |
| **version_09** | d34d207 | Disable EGL init (crash) | 禁用 EGL 初始化 |
| **version_10** | e9af68b | Fix compilation errors and warnings | 修复编译错误 |
| **version_11** | cf55fe8 | Initialize EGL context to enable OpenCL | 初始化 EGL 上下文 |
| **version_12** | 38031b2 | Add CPU affinity to performance cores | 添加 CPU 亲和性 |
| **version_13** | 554fb93 | Add batch handling for long prompts | 添加长 prompt batch 处理 |
| **version_14** | eabec8f | batch_size 512→128 | ⚠️⚠️ 降低 batch size |
| **version_15** | b625631 | Optimize log output | 优化日志输出 |

---

### 📁 版本 16-30（更早）
位置：`jni_history_versions_16_30/`
详细分析：`README_版本16-30改动详解.md`

| 版本 | Commit | 描述 | 关键改动 |
|------|--------|------|---------|
| **version_16** | df8af3d | Remove load_tensors log filter | 移除日志过滤器 |
| **version_17** | 4e0546c | Fix compilation error headers | 修复编译错误 |
| **version_18** | 3a1f779 | Add tensor allocation debug | 添加 tensor 分配调试 |
| **version_19** | 3af70fe | Add OpenCL diagnostics | ⚠️⚠️ 详细 OpenCL 诊断 |
| **version_20** | 523823e | Add OpenCL backend support | ⚠️ 首次添加 OpenCL |
| **version_21** | ee8d150 | Add tensor allocation debug | 添加 tensor 调试占位符 |
| **version_22** | 2dc17d1 | Fix duplicate Hexagon registration | 修复重复注册 |
| **version_23** | cd830ba | Add backend registration tracking | 添加注册跟踪 |
| **version_24** | f1f44c9 | Remove duplicate registration | 移除重复注册 |
| **version_25** | eee8487 | Remove duplicate Hexagon registration | 移除重复注册 |
| **version_26** | 9bb7ca4 | Add NULL terminator to device array | ⚠️ CRITICAL 修复 |
| **version_27** | 4e02129 | Fix compilation error llama_get_buf | 修复编译错误 |
| **version_28** | 066268c | Add backend synchronization | 添加 backend 同步 |
| **version_29** | 9cfa7ec | Update jni so | 更新 JNI .so |
| **version_30** | 62b561d | Test Hexagon device usability | 测试设备可用性 |

---

## 🔍 关键版本分析

### 🔴 最关键的 5 个版本

#### 1. **Version 19 (3af70fe) - Add OpenCL diagnostics** ⭐⭐⭐
**最重要的诊断版本**
- 添加了完整的 OpenCL 诊断代码
- 使用 `dlopen()` 动态加载 libOpenCL.so
- 使用 `clGetPlatformIDs()` 查询 OpenCL 平台
- **用户日志显示：OpenCL 库加载成功，但返回 -1001 错误**

#### 2. **Version 14 (eabec8f) - batch_size 512→128** ⭐⭐⭐
**可能导致性能下降**
- batch_size 从 512 降低到 128
- 可能导致吞吐量下降
- **这可能是 30 tokens/s 性能下降的主要原因之一**

#### 3. **Version 04 (239743c) - Disable OpenCL** ⭐⭐⭐
**完全禁用 GPU 加速**
- 为了防止死锁而禁用 OpenCL
- 失去了 Adreno GPU 加速
- **直接影响推理速度**

#### 4. **Version 26 (9bb7ca4) - Add NULL terminator** ⭐⭐
**CRITICAL 崩溃修复**
- device_list 数组添加 NULL 终结符
- 防止 `ggml_backend_sched_new()` 崩溃
- 影响 NPU offloading 的稳定性

#### 5. **Version 11 (cf55fe8) - Initialize EGL context** ⭐⭐
**尝试启用 OpenCL**
- 初始化 EGL 上下文以启用 OpenCL
- 但在 Version 09 中被禁用（导致崩溃）

---

## 📈 性能相关改动时间线

```
Version 30 (62b561d) ← 最早
        ↓
Version 20 (523823e) ← 首次添加 OpenCL backend 支持
        ↓
Version 19 (3af70fe) ← 添加详细 OpenCL 诊断（发现 -1001 错误）
        ↓
Version 16 (df8af3d) ← 移除日志过滤器
        ↓
Version 14 (eabec8f) ← ⚠️ batch_size 512→128（性能影响）
        ↓
Version 11 (cf55fe8) ← 初始化 EGL 上下文
        ↓
Version 09 (d34d207) ← 禁用 EGL（崩溃）
        ↓
Version 06 (c993fa8) ← 添加 use_mmap=false, flash_attn
        ↓
Version 05 (0f7ee5d) ← 启用双加速器（OpenCL + Hexagon）
        ↓
Version 04 (239743c) ← ⚠️ 禁用 OpenCL（防止死锁）
        ↓
Version 03 (7fb2ea7) ← LAYER 模式启用双加速器
        ↓
Version 01 (07e7132) ← 最新（修复 async_copy 编译错误）
```

---

## 🎯 关键发现总结

### 1. OpenCL 问题的演进

| 阶段 | 版本 | 状态 | 说明 |
|------|------|------|------|
| 初始添加 | v20 | OpenCL 首次添加 | 基本支持，无诊断 |
| 诊断阶段 | v19 | 发现 -1001 错误 | 详细诊断，发现 GPU 不可访问 |
| 移除诊断 | v16 | 移除诊断代码 | 问题变得更难调试 |
| 尝试修复 | v11 | 初始化 EGL | 尝试通过 EGL 启用 OpenCL |
| 崩溃修复 | v09 | 禁用 EGL | EGL 导致崩溃，回退 |
| 双加速器 | v05 | OpenCL + Hexagon | 尝试同时使用 |
| 防止死锁 | v04 | 禁用 OpenCL | **当前状态：OpenCL 被禁用** |

### 2. 性能下降的可能原因

根据历史版本分析，性能从可能的更高值下降到 30 tokens/s 的原因可能是：

1. **batch_size 降低** (v14: 512→128)
   - 直接影响吞吐量
   - 优先级：⭐⭐⭐

2. **OpenCL 被禁用** (v04)
   - 失去 Adreno GPU 加速
   - 只能使用 Hexagon NPU
   - 优先级：⭐⭐⭐

3. **use_mmap=false** (v06)
   - 可能影响内存访问性能
   - 优先级：⭐⭐

4. **EGL 初始化失败** (v11→v09)
   - OpenCL 无法正确初始化
   - 优先级：⭐⭐

### 3. OpenCL -1001 错误的根源

根据 Version 19 的诊断输出：
```
✅ libOpenCL.so loaded successfully
✅ clGetPlatformIDs function found
❌ clGetPlatformIDs() returned: -1001 (CL_PLATFORM_NOT_FOUND_KHR)
```

**可能的原因：**
- OpenGL ES 上下文未初始化（Android OpenCL 通常需要 OpenGL ES）
- GPU 驱动未加载或不可访问
- OpenCL 查询时机过早（在 GPU 初始化之前）
- SELinux 权限问题

---

## 🔧 建议的调试步骤

### 1. 恢复 Version 19 的诊断代码
将 Version 19 的完整 OpenCL 诊断代码恢复到当前版本，以便：
- 确认 OpenCL 库是否可以加载
- 检查 clGetPlatformIDs() 的返回值
- 获取详细的错误信息

### 2. 尝试恢复 batch_size=512
将 batch_size 从 128 恢复到 512（Version 14 之前的值），测试性能影响。

### 3. 尝试重新启用 OpenCL
在确保不会死锁的情况下，尝试重新启用 OpenCL：
- 参考 Version 05 的双加速器配置
- 添加 EGL 初始化（如 Version 11）
- 在 OpenGL ES 上下文创建后再初始化 OpenCL

### 4. 查找"成功版本"
用户提到有一个"成功"的版本，需要确定：
- 这个版本的 commit hash
- 这个版本是否在当前分支上
- 这个版本的完整日志输出

### 5. 检查其他分支
可能存在其他分支包含性能更好的版本：
```bash
git branch -a | grep -i claude
git log --all --oneline --graph
```

---

## 📚 相关文档

### 详细分析文档：
1. **README_改动详解.md** - 版本 1-15 详细分析
2. **README_版本16-30改动详解.md** - 版本 16-30 详细分析
3. **CRITICAL_OpenCL_诊断代码对比.md** - OpenCL 诊断代码详细对比

### 差异对比文件：
- **version_19_vs_20_diff.txt** - Version 19 vs 20 完整差异

### 代码文件：
- **jni_history_versions/** - 版本 1-15 的完整代码
- **jni_history_versions_16_30/** - 版本 16-30 的完整代码

---

## 🚀 下一步行动

1. ✅ **已完成**：提取所有 30 个历史版本
2. ✅ **已完成**：详细分析 OpenCL 相关改动
3. ✅ **已完成**：生成详细的对比文档

4. ⏳ **待完成**：
   - 找到用户的"成功版本"
   - 恢复 Version 19 的 OpenCL 诊断代码
   - 测试不同 batch_size 的性能影响
   - 尝试重新启用 OpenCL（如果可能）
   - 对比更早的版本（Version 31+，如果需要）

---

## 📞 需要用户提供的信息

为了找到性能下降的根本原因，需要用户提供：

1. **"成功版本"的 commit hash**
   - 性能达到预期的那个版本
   - 可以通过 `git log --oneline` 查找

2. **"成功版本"的完整日志**
   - 特别是初始化阶段的日志
   - OpenCL/Hexagon 相关的日志

3. **性能测试数据**
   - "成功版本"的 tokens/s
   - 当前版本的 tokens/s
   - 测试条件（模型、prompt 长度等）

4. **是否有本地未提交的改动**
   - `git status` 的输出
   - `git stash list` 的输出

---

**生成时间：** 2026-01-19
**总版本数：** 30 个
**文件总数：** 19 个（30 个 .cpp + 6 个文档 + 1 个脚本 + 2 个差异文件）
