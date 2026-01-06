# Installing Vulkan SDK on Windows

## Current Issue
CMake found Vulkan headers (version 1.3.275) but is missing `glslc`, the GLSL shader compiler needed to compile Vulkan shaders.

## Solution: Install Vulkan SDK

### Step 1: Download Vulkan SDK

1. Go to: https://vulkan.lunarg.com/sdk/home#windows
2. Download the latest Vulkan SDK installer for Windows (approximately 500MB)
3. Or use this direct link: https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe

### Step 2: Install Vulkan SDK

1. Run the downloaded `vulkan-sdk.exe`
2. **IMPORTANT**: During installation, make sure to select:
   - ✅ **Shader Toolchain Debug Libraries** (includes glslc)
   - ✅ **Core SDK Components**
   - ✅ **Debuggable Shader API Libraries**

3. The default installation path is typically:
   ```
   C:\VulkanSDK\1.3.xxx.x
   ```

4. The installer will automatically add Vulkan to your PATH environment variable

### Step 3: Verify Installation

After installation, open a **NEW** Git Bash or PowerShell window and run:

```bash
glslc --version
```

You should see output like:
```
glslc 1.3.xxx.x
Target: SPIR-V 1.0
```

### Step 4: Continue Build

Once glslc is installed and working:

```bash
cd /e/MyGithub/llama.cpp/examples/llama.android
./gradlew clean
./gradlew :lib:assembleRelease
```

## Alternative: Manual PATH Configuration

If glslc is not found after installation, manually add to PATH:

1. Open System Environment Variables
2. Edit PATH variable
3. Add: `C:\VulkanSDK\<version>\Bin`
4. Restart Git Bash/terminal

## Troubleshooting

**Q: glslc still not found after installation?**
- Close ALL terminal windows and open a new one
- Check PATH: `echo $PATH | grep -i vulkan`
- Verify file exists: `ls "C:/VulkanSDK/1.3.*/Bin/glslc.exe"`

**Q: Do I need to uninstall existing Vulkan?**
- No, you can install over existing installation
- Make sure to select shader toolchain components

**Q: Can I use Android NDK's Vulkan instead?**
- No, Android NDK includes Vulkan headers for runtime but not shader compilation tools like glslc
