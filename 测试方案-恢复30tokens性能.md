# 🎯 恢复 30 tokens/s 性能 - 系统测试方案

## ✅ 已完成的修改 (Commit: c993fa8)

### 添加了官方工具的关键参数

**修改文件**: `GGUFChat/app/src/main/cpp/llama-android-jni.cpp`

#### 1. 添加 `use_mmap = false`
```cpp
// 位置: 行 632
model_params.use_mmap = false;  // 匹配官方 --no-mmap
```

#### 2. 添加 Flash Attention
```cpp
// 位置: 行 762
ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;  // 匹配官方 -fa on
```

### 当前配置完全匹配官方工具

官方工具命令：
```bash
llama-completion --no-mmap -fa on --batch-size 128 -ngl 99 --device HTP0
```

我们的配置：
- ✅ batch_size: 128
- ✅ use_mmap: false
- ✅ flash_attn: enabled
- ✅ n_gpu_layers: 999 (相当于 -ngl 99)
- ✅ devices: [hexagon, nullptr] (相当于 --device HTP0)

---

## ⚠️ 发现的关键问题

### jniLibs 里的 .so 文件是 Git LFS 指针文件！

```bash
$ file GGUFChat/app/src/main/jniLibs/arm64-v8a/libggml-htp-v79.so
libggml-htp-v79.so: ASCII text

$ cat libggml-htp-v79.so
version https://git-lfs.github.com/spec/v1
oid sha256:382b722a51a6a612d78b3e6ad28c2c256abaff6ced2130bbe9f9d57c7fc3afa4
size 175336
```

**文件状态**:
- 当前大小: 131 bytes (Git LFS 指针)
- 实际大小: 175336 bytes (175 KB)
- 日期: Jan 15 06:50

**影响**: 应用运行时无法加载正确的 HTP 库，导致性能下降！

---

## 🔬 测试方案

### 方案 1: 在 Windows 上测试 (推荐，最快)

**前提**: 你在 Windows 机器上有正确的二进制 .so 文件

**步骤**:
1. 在 Windows 上 `git pull` 获取最新代码 (包含 JNI 修改)
2. 检查 jniLibs 里的 .so 文件是否是真正的二进制
   - libggml-htp-v79.so 应该是 ~175 KB
   - libllama.so 应该是 25-27 MB
   - libggml.so 应该是 1-2 MB
3. 如果 .so 文件正确，直接 Assemble APK
4. 安装到手机测试性能

**预期结果**: 如果 .so 文件正确 + JNI 参数正确 = 30 tokens/s

---

### 方案 2: 在 Linux 上拉取 Git LFS 文件

**步骤**:
```bash
cd /home/user/llama.cpp
git lfs pull
```

**检查 .so 文件是否正确**:
```bash
ls -lh GGUFChat/app/src/main/jniLibs/arm64-v8a/libggml-htp-v79.so
# 应该显示 ~175 KB，不是 131 bytes

file GGUFChat/app/src/main/jniLibs/arm64-v8a/libggml-htp-v79.so
# 应该显示 "ELF 64-bit LSB shared object"，不是 "ASCII text"
```

**然后**:
- 同步到 Windows
- Assemble APK
- 测试性能

---

### 方案 3: 重新编译 .so 文件 (最可靠)

**理论**: 使用 ggml-hexagon-clean.cpp 重新编译，确保版本一致

**步骤**:

#### 3.1 准备 ggml-hexagon.cpp
```bash
cd /home/user/llama.cpp

# 用 clean 版本替换官方版本
cp ggml-hexagon-clean.cpp ggml/src/ggml-hexagon/ggml-hexagon.cpp

# 检查差异
diff ggml-hexagon-clean.cpp ggml/src/ggml-hexagon/ggml-hexagon.cpp
# 应该显示完全一致
```

#### 3.2 清理并重新编译
```bash
# 清理旧的编译文件
rm -rf build-snapdragon

# 重新编译
./scripts/snapdragon/build-snapdragon.sh
```

#### 3.3 复制 .so 文件到 jniLibs
```bash
# 创建目标目录
mkdir -p GGUFChat/app/src/main/jniLibs/arm64-v8a

# 复制所有必需的 .so 文件
cp build-snapdragon/lib/libggml-htp-v*.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp build-snapdragon/lib/libggml.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp build-snapdragon/lib/libggml-base.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp build-snapdragon/lib/libggml-cpu.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp build-snapdragon/lib/libggml-hexagon.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp build-snapdragon/lib/libllama.so GGUFChat/app/src/main/jniLibs/arm64-v8a/

# 检查文件大小
ls -lh GGUFChat/app/src/main/jniLibs/arm64-v8a/libggml-htp-v79.so
# 应该显示 ~175 KB
```

#### 3.4 同步到 Windows 并测试
```bash
# 提交新的 .so 文件
git add GGUFChat/app/src/main/jniLibs/arm64-v8a/
git commit -m "Update .so files with clean ggml-hexagon.cpp"
git push
```

然后在 Windows 上:
1. git pull
2. Assemble APK
3. 测试性能

---

## 📊 你的"未重建 JNI"理论分析

### 理论内容
你说："我在想是不是因为我之前几次咱们改动项目里的jni cpp文件我没有assemble 之后放到jnilibs里面啊？然后我用的其实是前几版的so库就没问题了"

### 分析结果

**JNI 代码历史**:
- eabec8f (2026-01-08): batch=128, **无 mmap/flash_attn**
- 5060494: 添加了 threadpool (后来因为 ABI 崩溃被移除)
- a9db1ec: 移除 threadpool，回退到 eabec8f 状态
- 66ff631 (之前的当前版本): 与 a9db1ec 完全相同
- **c993fa8 (新的当前版本): 添加了 mmap/flash_attn ✅**

**实验性分支的配置** (不在当前分支上):
- 62aefe0: use_mmap=false, use_extra_bufts=true
- 725cea8: 完全默认配置, batch=512

### 结论

1. **当前 JNI 代码 (c993fa8) 现在包含了所有官方参数**
   - 之前缺少的 use_mmap 和 flash_attn 已添加 ✅

2. **如果你当时确实达到了 30 tokens/s**:
   - 可能是因为使用了实验性分支的代码 (62aefe0)
   - 或者确实是某个旧版本的 libllama-android.so + 新的 core .so

3. **现在的代码应该比之前更好**:
   - 包含了所有官方参数
   - 如果 .so 文件正确，理论上应该能达到 30 tokens/s

---

## 🎯 推荐执行顺序

### 第一优先: 方案 1 (如果 Windows 上有正确的 .so)
1. git pull (获取最新 JNI 代码)
2. 检查 .so 文件是否为二进制
3. Assemble + 测试

### 第二优先: 方案 2 (拉取 Git LFS)
1. git lfs pull (在 Linux 上)
2. 检查 .so 文件
3. 同步到 Windows
4. Assemble + 测试

### 第三优先: 方案 3 (重新编译)
1. 用 clean cpp 重新编译
2. 复制 .so 到 jniLibs
3. 同步到 Windows
4. Assemble + 测试

---

## 📝 预期结果

### 如果成功 (30 tokens/s):
- ✅ 证明缺少 mmap/flash_attn 参数是性能问题的原因
- ✅ 当前配置完全匹配官方工具

### 如果仍然失败 (12-15 tokens/s):
需要进一步调查:
1. 检查 HTP 库是否正确加载 (logcat 日志)
2. 检查是否有其他参数差异
3. 对比官方工具和 APK 的详细日志差异

---

## 💡 重要提示

1. **确保 .so 文件是二进制，不是 Git LFS 指针**
   - 这是当前最可能的性能瓶颈

2. **当前 JNI 代码已经完全匹配官方工具**
   - use_mmap = false ✅
   - flash_attn = enabled ✅
   - batch_size = 128 ✅

3. **如果方案 1 和 2 都不行，必须执行方案 3**
   - 重新编译确保版本一致性

---

## 📍 当前代码状态

- **分支**: claude/fix-hexagon-npu-crashes-5SkOp
- **最新 Commit**: c993fa8 "CRITICAL: Add official tool params to match 30 tokens/s configuration"
- **已 Push 到远程**: ✅
- **文件**: ggml-hexagon-clean.cpp 在根目录 ✅

准备好在 Windows 上测试！
