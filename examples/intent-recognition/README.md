# Intent Recognition with ONNX

This example demonstrates how to integrate an ONNX-based intent recognition model (with slot filling) into llama.cpp.

## Overview

The intent recognizer performs joint **intent classification** and **slot filling** on input text:

- **Intent Classification**: Identifies the user's intent (e.g., "weather_query", "music_play")
- **Slot Filling**: Extracts key information using BIO tagging (e.g., location="北京", time="明天")

## Features

- ✅ C++ implementation using ONNX Runtime
- ✅ Support for Chinese text (UTF-8)
- ✅ Character-level tokenization for BERT models
- ✅ Multi-threaded CPU inference
- ✅ Android JNI support
- ✅ Interactive and batch testing modes

## Prerequisites

### 1. ONNX Runtime

Download and install ONNX Runtime from [GitHub Releases](https://github.com/microsoft/onnxruntime/releases):

**Linux/Mac:**
```bash
# Download the release
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.0/onnxruntime-linux-x64-1.17.0.tgz
tar -xzf onnxruntime-linux-x64-1.17.0.tgz

# Set environment variable
export ONNXRUNTIME_ROOT_DIR=$(pwd)/onnxruntime-linux-x64-1.17.0
```

**Or install system-wide (Ubuntu/Debian):**
```bash
sudo apt install libonnxruntime-dev
```

### 2. nlohmann/json

Install the JSON library:

**Ubuntu/Debian:**
```bash
sudo apt install nlohmann-json3-dev
```

**Or download single header:**
```bash
mkdir -p /usr/local/include/nlohmann
wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -O /usr/local/include/nlohmann/json.hpp
```

## Model Setup

### Directory Structure

Place your ONNX model files in `data/file/models/intend/`:

```
data/file/models/intend/
├── joint_model_quantized.onnx  (or joint_model.onnx)
├── intent_label.txt             # List of intent labels
├── slot_label.txt               # List of slot labels (BIO tags)
├── vocab.txt                    # BERT vocabulary
└── android_config.json          # Model configuration
```

### Required Files

1. **joint_model_quantized.onnx** or **joint_model.onnx**
   - ONNX model with two outputs: intent logits and slot logits

2. **intent_label.txt**
   - One intent label per line
   - Example:
     ```
     weather_query
     music_play
     greeting
     ```

3. **slot_label.txt**
   - One slot label per line (BIO format)
   - Example:
     ```
     O
     B-location
     I-location
     B-time
     I-time
     B-artist
     I-artist
     ```

4. **vocab.txt**
   - BERT vocabulary file
   - One token per line

5. **android_config.json**
   - Model configuration
   - Example:
     ```json
     {
       "max_seq_len": 64
     }
     ```

## Building

### Desktop (Linux/Mac)

```bash
cd examples/intent-recognition
mkdir build && cd build

# If ONNX Runtime is installed system-wide
cmake ..

# Or specify ONNX Runtime location
cmake -DONNXRUNTIME_ROOT_DIR=/path/to/onnxruntime ..

make
```

### Android

See [Android Build Instructions](android/README.md)

## Usage

### Command Line Options

```bash
./intent-recognition [OPTIONS]

Options:
  --model_dir PATH    Directory containing ONNX model and assets
                      (default: ./data/file/models/intend)
  --interactive, -i   Run in interactive mode
  --text TEXT         Single text to predict
  --threads N         Number of CPU threads (default: 4)
  --help, -h          Show help message
```

### Examples

**Run predefined test cases:**
```bash
./intent-recognition --model_dir ../../../data/file/models/intend
```

**Single prediction:**
```bash
./intent-recognition --text "今天北京天气怎么样"
```

**Interactive mode:**
```bash
./intent-recognition --interactive
```

Output example:
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

## Android Integration

### Kotlin Usage

```kotlin
import com.llama.cpp.IntentRecognizer

// Initialize
val recognizer = IntentRecognizer()
val modelDir = "${getFilesDir()}/models/intend"

if (recognizer.initialize(modelDir, numThreads = 4)) {
    // Predict
    val result = recognizer.predict("今天北京天气怎么样")

    println("Intent: ${result.intent} (${result.confidence * 100}%)")
    result.slots.forEach { slot ->
        println("  ${slot.slotType}: ${slot.slotValue}")
    }

    // Or use formatted output
    println(result.format())

    // Release when done
    recognizer.release()
}
```

### Adding to Your Android Project

1. Copy `android/IntentRecognizer.kt` to your Kotlin source directory
2. Copy `android/jni/` to your JNI directory
3. Update your `build.gradle` to include the native library
4. Place model files in app assets or internal storage

See [Android README](android/README.md) for detailed instructions.

## Project Structure

```
examples/intent-recognition/
├── include/
│   └── intent_recognizer.h      # Main C++ API header
├── src/
│   ├── intent_recognizer.cpp    # C++ implementation
│   └── main.cpp                 # Command-line example
├── android/
│   ├── IntentRecognizer.kt      # Kotlin/Java API
│   └── jni/
│       ├── intent_jni.cpp       # JNI bridge
│       └── CMakeLists.txt       # Android build
├── CMakeLists.txt               # Desktop build
└── README.md                    # This file
```

## Model Training

The Python training code for JointBERT models can be found in various repositories:

- [JointBERT](https://github.com/monologg/JointBERT)
- [BERT-NLU](https://github.com/sz128/BERT-NLU)

After training, export to ONNX:

```python
import torch
from transformers import BertModel

# Load your trained model
model = YourJointBertModel()
model.eval()

# Create dummy input
dummy_input = {
    'input_ids': torch.randint(0, 21128, (1, 64)),
    'attention_mask': torch.ones(1, 64, dtype=torch.long)
}

# Export
torch.onnx.export(
    model,
    (dummy_input['input_ids'], dummy_input['attention_mask']),
    'joint_model.onnx',
    input_names=['input_ids', 'attention_mask'],
    output_names=['intent_logits', 'slot_logits'],
    dynamic_axes={
        'input_ids': {0: 'batch_size'},
        'attention_mask': {0: 'batch_size'},
        'intent_logits': {0: 'batch_size'},
        'slot_logits': {0: 'batch_size'}
    }
)
```

## Performance

### Inference Speed

On a typical CPU (4 threads):
- **Desktop**: ~50-100ms per query (depending on seq_len)
- **Android**: ~100-200ms per query

### Model Size

- **Original**: ~400MB (BERT-base)
- **Quantized (INT8)**: ~100MB
- **Further optimized**: Use ONNX quantization or model distillation

## Troubleshooting

### ONNX Runtime not found

```
CMake Error: Could not find ONNX Runtime
```

**Solution**: Install ONNX Runtime or specify path:
```bash
cmake -DONNXRUNTIME_ROOT_DIR=/path/to/onnxruntime ..
```

### Missing nlohmann/json

```
fatal error: nlohmann/json.hpp: No such file or directory
```

**Solution**: Install nlohmann-json3-dev or download json.hpp

### Model files not found

```
Failed to open label file: intent_label.txt
```

**Solution**: Ensure all model files are in the correct directory with correct names

### Android ONNX Runtime

For Android builds, you need the Android AAR:
1. Download from [ONNX Runtime Releases](https://github.com/microsoft/onnxruntime/releases)
2. Extract and place in `android/libs/onnxruntime/`

## License

This example follows the llama.cpp license. ONNX Runtime has its own license (MIT).

## References

- [ONNX Runtime](https://github.com/microsoft/onnxruntime)
- [JointBERT](https://github.com/monologg/JointBERT)
- [llama.cpp](https://github.com/ggerganov/llama.cpp)
