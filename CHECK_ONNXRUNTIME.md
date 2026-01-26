# 检查 ONNX Runtime 库文件

## 问题诊断

根据编译错误，链接器找不到 `OrtGetApiBase` 符号，这通常意味着：

1. **libonnxruntime.so 是 Git LFS 指针文件**（只有 ~130 字节）
2. 或者库文件版本/ABI 不匹配

## Windows 端检查步骤

### 1. 检查文件大小

```powershell
cd E:\MyGithub\llama.cpp\GGUFChat\app\src\main\jniLibs\arm64-v8a
dir libonnxruntime.so
```

**预期结果**：
- ✅ **正确**：文件大小应该是 **几十MB**（例如 20-50 MB）
- ❌ **错误**：如果只有 **100-130 字节**，这是 Git LFS 指针文件

### 2. 如果是 Git LFS 指针文件

你需要下载真正的文件：

#### 方法 A：手动下载并放置（推荐）

1. 下载 ONNX Runtime Android AAR：
   ```
   https://repo1.maven.org/maven2/com/microsoft/onnxruntime/onnxruntime-android/1.17.0/onnxruntime-android-1.17.0.aar
   ```

2. 使用 7-Zip 或 WinRAR 解压 AAR 文件（它是个 ZIP 文件）

3. 提取：
   ```
   jni/arm64-v8a/libonnxruntime.so
   ```

4. 复制到：
   ```
   E:\MyGithub\llama.cpp\GGUFChat\app\src\main\jniLibs\arm64-v8a\libonnxruntime.so
   ```

5. 确认文件大小（应该是 20-50 MB）

#### 方法 B：使用 Git LFS

```powershell
cd E:\MyGithub\llama.cpp
git lfs install
git lfs pull
```

### 3. 验证文件内容

在 PowerShell 中运行：

```powershell
# 检查文件头（应该是 ELF 二进制文件）
Format-Hex E:\MyGithub\llama.cpp\GGUFChat\app\src\main\jniLibs\arm64-v8a\libonnxruntime.so -Count 16
```

**预期输出**（前 4 个字节应该是 `7F 45 4C 46`，这是 ELF 文件的魔数）：
```
00000000   7F 45 4C 46 02 01 01 00 00 00 00 00 00 00 00 00
```

**如果看到的是文本**（例如 `version https://git-lfs...`），说明是 Git LFS 指针文件。

## 快速检查命令（PowerShell）

```powershell
$file = "E:\MyGithub\llama.cpp\GGUFChat\app\src\main\jniLibs\arm64-v8a\libonnxruntime.so"

if (Test-Path $file) {
    $size = (Get-Item $file).Length
    Write-Host "File exists: $file"
    Write-Host "Size: $size bytes ($([math]::Round($size/1MB, 2)) MB)"

    if ($size -lt 1000) {
        Write-Host "❌ ERROR: File is too small - this is likely a Git LFS pointer file"
        Write-Host "Action: Download the real library file (see Method A above)"
    } elseif ($size -gt 1000000) {
        Write-Host "✅ OK: File size looks correct"
        $header = Get-Content $file -Encoding Byte -TotalCount 4
        if ($header[0] -eq 0x7F -and $header[1] -eq 0x45 -and $header[2] -eq 0x4C -and $header[3] -eq 0x46) {
            Write-Host "✅ OK: File is a valid ELF binary"
        } else {
            Write-Host "❌ ERROR: File header is invalid"
        }
    }
} else {
    Write-Host "❌ ERROR: File not found: $file"
    Write-Host "Action: Download and place the library file (see Method A above)"
}
```

## 下载链接汇总

### ONNX Runtime Android AAR v1.17.0
- Maven Central: https://repo1.maven.org/maven2/com/microsoft/onnxruntime/onnxruntime-android/1.17.0/onnxruntime-android-1.17.0.aar
- 文件大小：约 20 MB
- 解压后提取：`jni/arm64-v8a/libonnxruntime.so`

### 其他 ABI（如果需要）
- armeabi-v7a: `jni/armeabi-v7a/libonnxruntime.so`
- x86_64: `jni/x86_64/libonnxruntime.so`
- x86: `jni/x86/libonnxruntime.so`

## 修复后重新编译

1. 确认 libonnxruntime.so 文件正确（大小 > 10 MB）
2. 在 Android Studio 中：
   - Build → Clean Project
   - Build → Rebuild Project

3. 你应该看到编译日志：
   ```
   ✓ ONNX Runtime found - enabling intent recognition
   Intent recognition feature: ENABLED
   ```

而不是：
   ```
   ⚠ ONNX Runtime not found - intent recognition disabled
   ```
