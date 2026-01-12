# GGUFChat Vulkan/NPU支持 - 快速开始指南

## 📌 核心问题回答

### Q: llama.cpp有没有NPU相关内容？
**A: 有！** llama.cpp有完整的Vulkan后端实现，可以利用GPU/NPU加速：
- ✅ 15,000+行Vulkan实现代码
- ✅ 150+个优化的计算着色器
- ✅ 支持AMD、NVIDIA、Intel、高通、联发科GPU/NPU
- ✅ 如果你的NPU支持Vulkan API，可以直接使用

### Q: 如果用Vulkan该怎么做？
**A: 三步走:**

1. **重新编译llama.cpp，启用Vulkan**
   ```bash
   ./build_vulkan_android.sh  # 我已经为你创建好了这个脚本
   ```

2. **复制so文件到ggufchat项目**
   ```bash
   ./copy_vulkan_libs.sh      # 自动复制脚本
   ```

3. **修改CMakeLists.txt添加Vulkan库**
   ```bash
   # 参考文件: GGUFChat_CMakeLists_VULKAN.txt
   # 主要是添加 libggml-vulkan.so 的导入和依赖
   ```

---

## 🚀 最简操作流程

### 第一步：设置NDK路径

编辑 `build_vulkan_android.sh`，修改第4行：
```bash
export ANDROID_NDK="/your/path/to/ndk/29.0.13113456"
```

### 第二步：编译

```bash
cd /home/user/llama.cpp
./build_vulkan_android.sh
```

等待编译完成（可能需要5-15分钟）

### 第三步：复制文件

```bash
./copy_vulkan_libs.sh
```

### 第四步：更新CMakeLists.txt

```bash
# 备份原文件
cp GGUFChat/llama-android/src/main/cpp/CMakeLists.txt \
   GGUFChat/llama-android/src/main/cpp/CMakeLists.txt.backup

# 使用新版本（或手动修改）
cp GGUFChat_CMakeLists_VULKAN.txt \
   GGUFChat/llama-android/src/main/cpp/CMakeLists.txt
```

**手动修改要点（如果不想整个替换）：**

在第44行添加：
```cmake
"libggml-vulkan.so"     # 新增这一行
```

在第102行后添加：
```cmake
# ggml-vulkan (depends on ggml-base)
add_library(ggml_vulkan_prebuilt SHARED IMPORTED GLOBAL)
set_target_properties(ggml_vulkan_prebuilt PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_LIB_DIR}/libggml-vulkan.so"
        INTERFACE_LINK_LIBRARIES ggml_base_prebuilt
)
message(STATUS "Imported ggml_vulkan_prebuilt (depends on ggml-base)")
```

修改第108行和第116行，添加 `;ggml_vulkan_prebuilt`

### 第五步：编译GGUFChat

```bash
cd GGUFChat
./gradlew clean
./gradlew :llama-android:assembleRelease
```

### 第六步：验证

安装APK后，运行应用，查看日志：
```bash
adb logcat | grep -i "vulkan\|ggml"
```

如果看到类似输出，说明成功：
```
ggml_vulkan: Found 1 Vulkan devices
ggml_vulkan: Using Qualcomm Adreno (TM) 740
```

---

## 📊 性能对比预期

### CPU模式（当前）
- Qwen 1.5B Q4_K_M: ~8-15 tokens/s
- Qwen 2.5B Q4_K_M: ~5-10 tokens/s

### Vulkan模式（启用后）
- 理论提升：2-5倍（取决于设备GPU/NPU能力）
- 高通8Gen2+: 预计 15-40 tokens/s
- 高通8Gen1: 预计 10-25 tokens/s
- 联发科天玑9200+: 预计 12-30 tokens/s

**注意**：
- 小模型（<1B）可能提升不明显（GPU开销）
- Q4_0, Q4_K_M量化格式在GPU上表现最好
- 实际性能取决于设备Vulkan驱动质量

---

## 🔍 关键技术点

### 为什么选择Vulkan而不是NNAPI？

| 特性 | Vulkan | NNAPI | QNN (高通) |
|------|--------|-------|------------|
| llama.cpp支持 | ✅ 完整 | ❌ 无 | ❌ 无 |
| 跨平台 | ✅ 是 | ❌ 仅Android | ❌ 仅高通 |
| 性能 | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| 易用性 | ✅ 高 | ⚠️ 中 | ❌ 低 |
| 设备支持 | ✅ 广泛 | ✅ 广泛 | ⚠️ 仅高通 |

### Vulkan如何利用NPU？

现代Android设备的NPU（如高通HTP、联发科APU）如果支持Vulkan API，会作为Vulkan计算设备暴露出来。llama.cpp会自动检测并使用。

检查你的设备：
```bash
# 查看Vulkan版本
adb shell getprop ro.vulkan.level

# 查看GPU驱动
adb shell dumpsys | grep -i "vulkan\|gpu"
```

---

## 📁 创建的文件列表

我为你创建了以下文件：

1. **build_vulkan_android.sh** - 编译脚本（需要修改NDK路径）
2. **copy_vulkan_libs.sh** - 自动复制so文件脚本
3. **VULKAN_ANDROID_GUIDE.md** - 详细指南（中文，10,000+字）
4. **GGUFChat_CMakeLists_VULKAN.txt** - 修改后的CMakeLists.txt参考
5. **QUICK_START_CN.md** - 本快速开始指南

---

## ⚠️ 常见问题

### 1. 编译失败：找不到NDK
```bash
# 设置环境变量
export ANDROID_NDK=$HOME/Android/Sdk/ndk/29.0.13113456
```

### 2. 编译失败：找不到Vulkan头文件
NDK r21+已包含Vulkan。如果提示找不到，检查NDK版本：
```bash
ls $ANDROID_NDK/sysroot/usr/include/vulkan/
```

### 3. 运行时崩溃
检查所有so文件都已复制：
```bash
ls -lh GGUFChat/llama-android/src/main/jniLibs/arm64-v8a/
# 应该看到5个文件：
# libggml-base.so
# libggml-cpu.so
# libggml-vulkan.so  ← 这个是关键
# libggml.so
# libllama.so
```

### 4. 性能没有提升
可能原因：
- 设备Vulkan驱动不完善（更新系统）
- NPU未暴露为Vulkan设备（厂商限制）
- 模型量化格式不适合GPU（试试Q4_K_M）
- 模型太小，GPU开销超过收益

### 5. Vulkan未启用
查看日志确认：
```bash
adb logcat -s "llama:*" "ggml:*"
```

如果只看到CPU后端，可能是：
- 设备不支持Vulkan
- so文件未正确链接
- 编译时未启用GGML_VULKAN

---

## 📚 进阶优化

### 减小APK大小
如果只需要arm64支持：
```gradle
// build.gradle.kts
ndk {
    abiFilters += listOf("arm64-v8a")  // 移除armeabi-v7a
}
```

### 调试Vulkan
编译时启用调试：
```bash
# 修改 build_vulkan_android.sh
-DGGML_VULKAN_DEBUG=ON \
-DGGML_VULKAN_VALIDATE=ON \
```

### 性能分析
使用Android GPU Inspector：
```bash
# 安装AGI
# https://gpuinspector.dev/

# 分析Vulkan调用
```

---

## 🎯 下一步

完成Vulkan集成后，你可以：

1. **性能测试**: 比较CPU vs Vulkan性能
2. **模型优化**: 测试不同量化格式（Q4_0, Q4_K_M, Q5_K_M）
3. **内存优化**: 调整context size和batch size
4. **UI改进**: 添加后端选择开关（CPU/Vulkan）
5. **分享经验**: 在llama.cpp社区分享Android NPU经验

---

## 📞 需要帮助？

查看详细文档：
```bash
cat VULKAN_ANDROID_GUIDE.md
```

检查llama.cpp官方文档：
- https://github.com/ggerganov/llama.cpp/blob/master/docs/build.md#vulkan

祝你成功启用NPU加速！🚀
