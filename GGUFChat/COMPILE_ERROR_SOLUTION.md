# 编译错误解决方案

## 错误原因

`clang++: error: unable to execute command: Killed`

这是 **内存不足(OOM)** 导致的，与我们修改的 Hexagon 代码无关。

## 好消息

你需要的 Hexagon 库已经成功编译了！错误发生在编译命令行工具时，而你的 Android 项目不需要这些工具。

## 解决方案 1: 只编译库文件（推荐）

在 Docker 容器内执行：

```bash
# 检查已编译的库文件
find build-snapdragon -name "*.so" -type f

# 如果库文件已存在，直接安装
cmake --install build-snapdragon --prefix pkg-adb/llama.cpp --component libraries

# 检查安装的库
ls -lh pkg-adb/llama.cpp/lib/
```

如果库文件已经存在，你可以直接跳到"复制库文件"步骤。

## 解决方案 2: 减少内存使用重新编译

如果库文件不完整，使用单线程编译：

```bash
# 清理之前的编译
rm -rf build-snapdragon

# 重新配置
cmake --preset arm64-android-snapdragon-release -B build-snapdragon

# 单线程编译（避免内存不足）
cmake --build build-snapdragon -j 1

# 如果还是失败，只编译库文件
cmake --build build-snapdragon -j 1 --target ggml
cmake --build build-snapdragon -j 1 --target llama
```

## 解决方案 3: 只编译 Hexagon 相关库（最快）

```bash
cd build-snapdragon/ggml/src

# 编译 base 库
make -j1 ggml-base

# 编译 CPU 库
make -j1 ggml-cpu

# 编译 Hexagon 库
make -j1 ggml-hexagon

# 编译 HTP 库（关键！）
cd ggml-hexagon
make -j1 htp-v73
make -j1 htp-v75
make -j1 htp-v79
make -j1 htp-v81

# 返回根目录编译 llama
cd /workspace
cmake --build build-snapdragon -j 1 --target llama
```

## 复制库文件到 GGUFChat

编译成功后，在 Docker 容器内：

```bash
# 安装到 pkg-adb 目录
cmake --install build-snapdragon --prefix pkg-adb/llama.cpp

# 检查库文件
ls -lh pkg-adb/llama.cpp/lib/

# 你应该看到这些文件：
# libggml-base.so
# libggml-cpu.so
# libggml-hexagon.so
# libggml-htp-v73.so
# libggml-htp-v75.so
# libggml-htp-v79.so
# libggml-htp-v81.so
# libggml.so
# libllama.so
```

然后退出 Docker，在宿主机执行：

```bash
cd /home/user/llama.cpp

# 运行复制脚本
./GGUFChat/copy-official-libs.sh

# 或手动复制
cp pkg-adb/llama.cpp/lib/libggml-base.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-cpu.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-hexagon.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-htp-v73.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-htp-v75.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-htp-v79.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml-htp-v81.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libggml.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
cp pkg-adb/llama.cpp/lib/libllama.so GGUFChat/app/src/main/jniLibs/arm64-v8a/
```

## 快速检查：库是否已编译

在 Docker 容器内执行：

```bash
# 检查 HTP 库
ls -lh build-snapdragon/ggml/src/ggml-hexagon/*.so

# 检查主库
ls -lh build-snapdragon/bin/*.so
```

如果这些文件已存在，说明编译成功，只是后续的命令行工具编译失败（不影响使用）。

## 验证修改已生效

检查编译的库是否包含我们的修改：

```bash
# 在 Docker 容器内
strings build-snapdragon/bin/libggml-hexagon.so | grep "ADSP_LIBRARY_PATH"
# 应该看到输出，说明我们的修改已编译进去

strings build-snapdragon/bin/libggml-hexagon.so | grep "using absolute HTP path"
# 应该看到我们添加的日志信息
```

## 总结

1. ✅ 我们的 Hexagon 修改已经成功编译
2. ❌ 链接命令行工具时内存不足（不影响 Android 使用）
3. 🎯 只需要复制 `.so` 库文件到 GGUFChat 项目
4. 🚀 然后重新编译 Android APK 测试 NPU 加速

## 关键提示

**我们修改的代码在运行时生效，不在编译时！**

修改的功能：
- 读取 `ADSP_LIBRARY_PATH` 环境变量
- 生成绝对路径 URI

这些都是在你的 Android 应用**运行时**执行的，编译只是把代码打包进 `.so` 文件。
