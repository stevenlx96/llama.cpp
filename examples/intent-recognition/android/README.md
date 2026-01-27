# Android Integration Guide

This guide explains how to integrate the Intent Recognition library into your Android project.

## Prerequisites

1. Android Studio
2. NDK installed (version 21 or higher)
3. ONNX Runtime Android AAR

## Download ONNX Runtime for Android

1. Go to [ONNX Runtime Releases](https://github.com/microsoft/onnxruntime/releases)
2. Download the Android AAR (e.g., `onnxruntime-android-1.17.0.aar`)
3. Extract the AAR file:
   ```bash
   unzip onnxruntime-android-1.17.0.aar -d onnxruntime-android
   ```
4. The structure should be:
   ```
   onnxruntime-android/
   ├── jni/
   │   ├── arm64-v8a/
   │   │   └── libonnxruntime.so
   │   ├── armeabi-v7a/
   │   │   └── libonnxruntime.so
   │   └── x86_64/
   │       └── libonnxruntime.so
   └── headers/
       └── onnxruntime/
           └── *.h
   ```

## Integration Steps

### 1. Copy Files to Your Project

**Copy Kotlin source:**
```bash
cp android/IntentRecognizer.kt app/src/main/java/com/yourpackage/
```

**Copy JNI source:**
```bash
mkdir -p app/src/main/cpp/intent
cp android/jni/* app/src/main/cpp/intent/
cp -r ../../include app/src/main/cpp/intent/
cp -r ../../src app/src/main/cpp/intent/
```

### 2. Update build.gradle (app level)

Add CMake configuration:

```gradle
android {
    ...

    defaultConfig {
        ...

        externalNativeBuild {
            cmake {
                cppFlags "-std=c++17"
                arguments "-DONNXRUNTIME_ROOT_DIR=${project.rootDir}/libs/onnxruntime"
            }
        }

        ndk {
            abiFilters 'arm64-v8a', 'armeabi-v7a', 'x86_64'
        }
    }

    externalNativeBuild {
        cmake {
            path "src/main/cpp/intent/jni/CMakeLists.txt"
            version "3.18.1"
        }
    }
}

dependencies {
    // If using the full AAR
    implementation files('libs/onnxruntime-android-1.17.0.aar')
}
```

### 3. Place ONNX Runtime Libraries

**Option A: Use AAR dependency (recommended)**
```bash
mkdir -p app/libs
cp onnxruntime-android-1.17.0.aar app/libs/
```

**Option B: Extract and use native libraries**
```bash
mkdir -p app/libs/onnxruntime
# Copy headers
cp -r onnxruntime-android/headers/* app/libs/onnxruntime/include/

# Copy libraries for each ABI
cp -r onnxruntime-android/jni/* app/src/main/jniLibs/
```

### 4. Place Model Files

Put your model files in the assets folder:

```bash
mkdir -p app/src/main/assets/models/intend
cp your_model/* app/src/main/assets/models/intend/
```

Or copy to internal storage at runtime:

```kotlin
private fun copyModelToInternalStorage(context: Context) {
    val assetManager = context.assets
    val modelDir = File(context.filesDir, "models/intend")
    modelDir.mkdirs()

    val files = listOf(
        "joint_model_quantized.onnx",
        "intent_label.txt",
        "slot_label.txt",
        "vocab.txt",
        "android_config.json"
    )

    files.forEach { filename ->
        assetManager.open("models/intend/$filename").use { input ->
            File(modelDir, filename).outputStream().use { output ->
                input.copyTo(output)
            }
        }
    }
}
```

### 5. Use in Your Code

#### Kotlin Example

```kotlin
import com.llama.cpp.IntentRecognizer
import com.llama.cpp.format

class MainActivity : AppCompatActivity() {
    private lateinit var recognizer: IntentRecognizer

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Copy model files (first time only)
        copyModelToInternalStorage(this)

        // Initialize recognizer
        recognizer = IntentRecognizer()
        val modelDir = "${filesDir}/models/intend"

        lifecycleScope.launch(Dispatchers.IO) {
            val success = recognizer.initialize(modelDir, numThreads = 4)

            withContext(Dispatchers.Main) {
                if (success) {
                    Log.d(TAG, "Intent recognizer initialized")
                    setupUI()
                } else {
                    Log.e(TAG, "Failed to initialize recognizer")
                    showError()
                }
            }
        }
    }

    private fun predictIntent(text: String) {
        lifecycleScope.launch(Dispatchers.IO) {
            val result = recognizer.predict(text)

            withContext(Dispatchers.Main) {
                // Update UI with result
                textViewIntent.text = result.intent
                textViewConfidence.text = "${(result.confidence * 100).toInt()}%"

                // Display slots
                val slotsText = result.slots.joinToString("\n") { slot ->
                    "${slot.slotType}: ${slot.slotValue}"
                }
                textViewSlots.text = slotsText

                // Or print formatted result
                Log.d(TAG, result.format())
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        recognizer.release()
    }

    companion object {
        private const val TAG = "MainActivity"
    }
}
```

#### Handling Permissions

Add to AndroidManifest.xml (if loading from external storage):

```xml
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
```

## Complete Example Project Structure

```
app/
├── src/main/
│   ├── java/com/yourpackage/
│   │   ├── MainActivity.kt
│   │   └── IntentRecognizer.kt
│   ├── cpp/intent/
│   │   ├── jni/
│   │   │   ├── CMakeLists.txt
│   │   │   └── intent_jni.cpp
│   │   ├── include/
│   │   │   └── intent_recognizer.h
│   │   └── src/
│   │       └── intent_recognizer.cpp
│   ├── assets/models/intend/
│   │   ├── joint_model_quantized.onnx
│   │   ├── intent_label.txt
│   │   ├── slot_label.txt
│   │   ├── vocab.txt
│   │   └── android_config.json
│   └── jniLibs/
│       ├── arm64-v8a/
│       │   └── libonnxruntime.so
│       ├── armeabi-v7a/
│       │   └── libonnxruntime.so
│       └── x86_64/
│           └── libonnxruntime.so
├── libs/
│   └── onnxruntime-android-1.17.0.aar (optional)
└── build.gradle
```

## Troubleshooting

### UnsatisfiedLinkError

```
java.lang.UnsatisfiedLinkError: dlopen failed: library "libintent_jni.so" not found
```

**Solutions:**
1. Check that CMake build succeeded
2. Verify ndk.abiFilters in build.gradle
3. Check that ONNX Runtime libraries are in jniLibs

### ONNX Runtime Not Found

```
CMake Error: ONNX Runtime not found for Android
```

**Solution:** Set ONNXRUNTIME_ROOT_DIR in build.gradle or place in expected location

### Model Not Loading

```
Failed to initialize recognizer
```

**Solutions:**
1. Check model files are copied correctly
2. Verify file permissions
3. Check logcat for detailed error messages:
   ```bash
   adb logcat | grep IntentJNI
   ```

### Out of Memory

For large models on low-end devices:
1. Use quantized model (INT8)
2. Reduce max_seq_len
3. Use model pruning or distillation

## Performance Optimization

### 1. Use Quantized Model

Convert your model to INT8:
```python
import onnxruntime.quantization as quantization

quantization.quantize_dynamic(
    'joint_model.onnx',
    'joint_model_quantized.onnx',
    weight_type=quantization.QuantType.QInt8
)
```

### 2. Enable NNAPI (Android Neural Networks API)

Add to session options in C++:
```cpp
session_options.AddConfigEntry("session.use_nnapi", "1");
```

### 3. Thread Configuration

For mobile devices, use 2-4 threads:
```kotlin
recognizer.initialize(modelDir, numThreads = 2)
```

### 4. Asynchronous Inference

Always run inference on background thread:
```kotlin
lifecycleScope.launch(Dispatchers.IO) {
    val result = recognizer.predict(text)
    // Update UI on main thread
}
```

## Example App

A complete example Android app can be found in `examples/intent-recognition/android/app/` (TODO)

## License

This follows the llama.cpp license. ONNX Runtime has its own license (MIT).
