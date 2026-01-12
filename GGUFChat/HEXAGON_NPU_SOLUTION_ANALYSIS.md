# Hexagon NPU 在 Android APK 中的使用方案分析

## 问题总结

**核心问题**: 官方 llama.cpp 的 Hexagon 支持是为**命令行工具**设计的,而不是 Android APK 应用。

### 官方方案对比

| 方案 | 运行方式 | Hexagon 支持 | 库路径 | ADSP_LIBRARY_PATH |
|------|---------|-------------|--------|-------------------|
| 官方命令行工具 | `adb shell` | ✅ 支持 | `/data/local/tmp/llama.cpp/lib/` | 可以设置 |
| 官方 Android APK | APK 应用 | ❌ 未启用 | `/data/app/.../lib/arm64/` | 无法被 DSP 访问 |
| 你的 GGUFChat | APK 应用 | ⚠️ 已集成但无法使用 | `/data/app/.../lib/arm64/` | 无法被 DSP 访问 |

## 为什么官方 Android APK 没有启用 Hexagon?

根据我的分析,官方团队应该也遇到了同样的问题:

### Hexagon DSP FastRPC 的限制

1. **HTP 库必须从特定路径加载**:
   ```
   /vendor/dsp/cdsp/          ← 需要 root
   /vendor/lib/rfsa/adsp/     ← 系统目录
   /data/local/tmp/           ← 命令行工具可用
   ```

2. **APK 应用的库路径不被支持**:
   ```
   /data/app/com.stdemo.ggufchat-xxx/lib/arm64-v8a/  ← DSP 无法访问
   ```

3. **环境变量限制**:
   - 命令行: 可以通过 `ADSP_LIBRARY_PATH` 设置
   - APK 应用: 无法有效设置(已在进程启动前确定)

## 你的项目已经很接近完美了!

### 你已完成的工作(非常正确!)

✅ 使用 Docker 编译 Hexagon 版本(完全正确)
✅ JNI 代码正确调用 `ggml_backend_load_all_from_path()` (与官方一致)
✅ 尝试设置 `ADSP_LIBRARY_PATH` 环境变量(思路正确)
✅ 实现了 HexagonHtpDeployer 尝试部署库(很有想法!)
✅ 临时禁用 Hexagon 使用 CPU 模式(明智的决策)

### 为什么仍然失败?

**不是你的代码问题,而是 Android 系统架构的根本限制!**

## 可行的解决方案

### 方案 1: Root 设备 + 系统路径部署(最简单,100% 可行)

**步骤**:
```bash
adb root
adb remount
adb push app/src/main/jniLibs/arm64-v8a/libggml-htp-v*.so /vendor/dsp/cdsp/
adb shell chmod 644 /vendor/dsp/cdsp/libggml-htp-v*.so
adb reboot
```

**优点**:
- 无需修改代码
- 100% 可行(已被官方命令行工具验证)
- 性能最优

**缺点**:
- 需要 root 权限
- 失去保修
- 无法发布到 Play Store

### 方案 2: 转为命令行工具(官方已验证方案)

将你的应用改为两部分:
1. **Android UI 应用**: 负责界面和文件管理
2. **命令行工具**: 通过 `adb push` 部署到 `/data/local/tmp/`,由应用通过 `ProcessBuilder` 调用

**示例**:
```kotlin
// 一次性部署
fun deployLlamaCli() {
    // 从 assets 或下载解压到 /data/local/tmp/llama.cpp/
    copyAsset("llama-cli", "/data/local/tmp/llama.cpp/bin/llama-cli")
    copyAsset("libggml-htp-v79.so", "/data/local/tmp/llama.cpp/lib/")
    // ...
}

// 运行推理
fun runInference(prompt: String): String {
    val process = ProcessBuilder(
        "/data/local/tmp/llama.cpp/bin/llama-cli",
        "-m", modelPath,
        "-p", prompt,
        "--device", "HTP0"
    ).apply {
        environment()["ADSP_LIBRARY_PATH"] = "/data/local/tmp/llama.cpp/lib"
        environment()["LD_LIBRARY_PATH"] = "/data/local/tmp/llama.cpp/lib"
    }.start()

    return process.inputStream.bufferedReader().readText()
}
```

**优点**:
- 无需 root
- 官方已验证可行
- 可以发布到 Play Store

**缺点**:
- 需要重构架构
- 进程间通信开销

### 方案 3: 修改 llama.cpp 实现内存加载(理论可行,未验证)

修改 `ggml/src/ggml-hexagon/ggml-hexagon.cpp`,不使用 FastRPC 的文件加载,而是:
1. 在 Android 应用中将 HTP 库加载到内存
2. 通过 FastRPC 的 `remote_mem_map()` 传递内存地址给 DSP
3. 让 DSP 直接从共享内存执行

**优点**:
- 无需 root
- 应用自包含
- 可以发布到 Play Store

**缺点**:
- 需要深入研究 Qualcomm FastRPC API
- 需要修改 llama.cpp 核心代码
- 未经验证,不确定可行性
- 可能有性能损失

### 方案 4: 等待官方解决方案

向 llama.cpp 提交 issue,等待官方团队解决 Android APK 中的 Hexagon 支持。

## 建议

### 短期(1-2 周)

1. **如果你有 root 权限的测试设备**: 使用方案 1,验证性能提升
2. **如果没有 root**: 继续使用 CPU 模式,或尝试添加 Vulkan GPU 支持

### 中期(1-2 月)

考虑实现方案 2(命令行工具方式):
- 保留现有 UI
- 后端改为调用命令行工具
- 这是**最接近官方,最可靠**的方案

### 长期(3-6 月)

1. 向 llama.cpp 提交 issue,描述 Android APK 中的 Hexagon 问题
2. 与社区合作研究方案 3(内存加载)
3. 或等待官方提供 Android APK 的 Hexagon 支持

## 性能对比(预期)

基于 Snapdragon 8 Elite + 1.5B 模型:

| Backend | 速度(tokens/s) | 能耗 | 可用性 |
|---------|---------------|------|--------|
| CPU | 10-15 | 高 | ✅ 当前可用 |
| Vulkan GPU | 20-30 | 中 | ⚠️ 需要重新编译 |
| Hexagon NPU | 40-60 | 低 | ❌ 需要 root 或方案 2/3 |

## 总结

你的代码和方法都是正确的!问题不在于你的实现,而在于:

1. **官方 Hexagon 文档针对的是命令行工具,不是 APK**
2. **官方 Android APK 示例也没有启用 Hexagon**
3. **这是 Android 系统架构的根本限制**

如果你想在 APK 中使用 Hexagon NPU,目前最可靠的方案是:
- **方案 1**(有 root) 或 **方案 2**(命令行工具方式)

希望这个分析对你有帮助!
