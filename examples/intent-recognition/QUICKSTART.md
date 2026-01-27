# Quick Start Guide

快速上手指南，帮助你在5分钟内运行意图识别示例。

## 前提条件

1. **已安装的依赖**:
   - CMake (>= 3.14)
   - C++17 编译器 (GCC, Clang, or MSVC)
   - ONNX Runtime
   - nlohmann/json

2. **已准备的模型文件**:
   - `joint_model_quantized.onnx` (或 `joint_model.onnx`)
   - `intent_label.txt`
   - `slot_label.txt`
   - `vocab.txt`
   - `android_config.json`

## 步骤 1: 安装依赖

### Ubuntu/Debian

```bash
# 安装 ONNX Runtime
sudo apt install libonnxruntime-dev

# 安装 JSON 库
sudo apt install nlohmann-json3-dev
```

### 或手动下载 ONNX Runtime

```bash
# 下载 ONNX Runtime
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.0/onnxruntime-linux-x64-1.17.0.tgz
tar -xzf onnxruntime-linux-x64-1.17.0.tgz

# 设置环境变量
export ONNXRUNTIME_ROOT_DIR=$(pwd)/onnxruntime-linux-x64-1.17.0
```

## 步骤 2: 准备模型

使用提供的Python脚本准备模型:

```bash
cd examples/intent-recognition/models

# 基本用法 (不量化)
python3 prepare_model.py \
  --model_path /path/to/your/joint_model.onnx \
  --output_dir ../../../data/file/models/intend \
  --intent_labels /path/to/intent_label.txt \
  --slot_labels /path/to/slot_label.txt \
  --vocab /path/to/vocab.txt

# 或者使用量化 (推荐,减小模型大小)
python3 prepare_model.py \
  --model_path /path/to/your/joint_model.onnx \
  --output_dir ../../../data/file/models/intend \
  --intent_labels /path/to/intent_label.txt \
  --slot_labels /path/to/slot_label.txt \
  --vocab /path/to/vocab.txt \
  --quantize
```

或者手动复制文件:

```bash
# 创建模型目录
mkdir -p data/file/models/intend

# 复制模型文件
cp /path/to/joint_model_quantized.onnx data/file/models/intend/
cp /path/to/intent_label.txt data/file/models/intend/
cp /path/to/slot_label.txt data/file/models/intend/
cp /path/to/vocab.txt data/file/models/intend/

# 创建配置文件
cat > data/file/models/intend/android_config.json << EOF
{
  "max_seq_len": 64
}
EOF
```

## 步骤 3: 构建

```bash
cd examples/intent-recognition
mkdir build && cd build

# 如果 ONNX Runtime 已系统安装
cmake ..

# 或指定 ONNX Runtime 路径
cmake -DONNXRUNTIME_ROOT_DIR=/path/to/onnxruntime ..

# 编译
make -j$(nproc)
```

## 步骤 4: 运行

### 运行测试用例

```bash
./intent-recognition --model_dir ../../../data/file/models/intend
```

### 单次预测

```bash
./intent-recognition --text "今天北京天气怎么样"
```

输出示例:
```
==================================================
Input: 今天北京天气怎么样
--------------------------------------------------
Intent: weather_query (confidence: 98.45%)
--------------------------------------------------
Slots:
  - time: 今天
  - location: 北京
--------------------------------------------------
BIO Tags:
  Chars: 今 天 北 京 天 气 怎 么 样
  Tags:  B-time I-time B-location I-location O O O O O
==================================================
```

### 交互模式

```bash
./intent-recognition --interactive
```

然后输入文本进行实时预测:
```
Input: 播放周杰伦的歌
==================================================
Input: 播放周杰伦的歌
--------------------------------------------------
Intent: music_play (confidence: 95.32%)
--------------------------------------------------
Slots:
  - artist: 周杰伦
==================================================

Input: quit
Bye!
```

## 常见问题

### 1. ONNX Runtime 未找到

**错误信息:**
```
CMake Error: Could not find ONNX Runtime
```

**解决方案:**
- 安装 ONNX Runtime: `sudo apt install libonnxruntime-dev`
- 或指定路径: `cmake -DONNXRUNTIME_ROOT_DIR=/path/to/onnxruntime ..`

### 2. nlohmann/json 未找到

**错误信息:**
```
fatal error: nlohmann/json.hpp: No such file or directory
```

**解决方案:**
```bash
sudo apt install nlohmann-json3-dev
```

或手动下载:
```bash
sudo mkdir -p /usr/local/include/nlohmann
sudo wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp \
  -O /usr/local/include/nlohmann/json.hpp
```

### 3. 模型文件未找到

**错误信息:**
```
Failed to open label file: intent_label.txt
```

**解决方案:**
确保所有模型文件都在正确的目录中:
```bash
ls -la data/file/models/intend/
# 应该看到:
# joint_model_quantized.onnx (or joint_model.onnx)
# intent_label.txt
# slot_label.txt
# vocab.txt
# android_config.json
```

### 4. 推理速度慢

**优化建议:**

1. **使用量化模型** (INT8):
   ```bash
   python3 models/prepare_model.py ... --quantize
   ```

2. **增加线程数**:
   ```bash
   ./intent-recognition --threads 8
   ```

3. **减小 max_seq_len**:
   编辑 `android_config.json`:
   ```json
   {
     "max_seq_len": 32
   }
   ```

## Android 集成

如果要在 Android 上使用,请参考 [Android README](android/README.md)。

快速步骤:
1. 下载 ONNX Runtime Android AAR
2. 复制 Kotlin 和 JNI 文件到你的项目
3. 配置 build.gradle
4. 使用 `IntentRecognizer` 类

示例代码:
```kotlin
val recognizer = IntentRecognizer()
if (recognizer.initialize("${filesDir}/models/intend", numThreads = 4)) {
    val result = recognizer.predict("今天北京天气怎么样")
    println(result.format())
    recognizer.release()
}
```

## 下一步

- 查看完整文档: [README.md](README.md)
- Android 集成: [android/README.md](android/README.md)
- 训练自己的模型: 参考 [JointBERT](https://github.com/monologg/JointBERT)
- 性能优化: 使用模型量化、剪枝、蒸馏

## 获取帮助

如果遇到问题:
1. 检查 [README.md](README.md) 中的故障排除部分
2. 查看完整的错误日志
3. 在 llama.cpp GitHub 仓库提交 issue
