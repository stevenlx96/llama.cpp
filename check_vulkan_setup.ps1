# 检查 Vulkan 开发环境设置
# 使用方法: .\check_vulkan_setup.ps1

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Vulkan 开发环境检查" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$allGood = $true

# 1. 检查 Vulkan SDK 环境变量
Write-Host "[1] 检查 Vulkan SDK 环境变量..." -ForegroundColor Yellow
if ($env:VULKAN_SDK) {
    Write-Host "  ✓ VULKAN_SDK = $env:VULKAN_SDK" -ForegroundColor Green

    # 检查路径是否存在
    if (Test-Path $env:VULKAN_SDK) {
        Write-Host "  ✓ 路径存在" -ForegroundColor Green
    } else {
        Write-Host "  ✗ 路径不存在!" -ForegroundColor Red
        $allGood = $false
    }
} else {
    Write-Host "  ✗ VULKAN_SDK 环境变量未设置" -ForegroundColor Red
    Write-Host "    请安装 Vulkan SDK: https://vulkan.lunarg.com/sdk/home#windows" -ForegroundColor Yellow
    $allGood = $false
}
Write-Host ""

# 2. 检查 glslc 编译器
Write-Host "[2] 检查 glslc 着色器编译器..." -ForegroundColor Yellow
try {
    $glslcVersion = & glslc --version 2>&1 | Select-Object -First 1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ glslc 已安装: $glslcVersion" -ForegroundColor Green
    } else {
        Write-Host "  ✗ glslc 未找到" -ForegroundColor Red
        $allGood = $false
    }
} catch {
    Write-Host "  ✗ glslc 未找到或无法执行" -ForegroundColor Red
    Write-Host "    请确保 Vulkan SDK 已添加到 PATH" -ForegroundColor Yellow
    $allGood = $false
}
Write-Host ""

# 3. 检查 vulkan.hpp 头文件
Write-Host "[3] 检查 vulkan.hpp C++ 头文件..." -ForegroundColor Yellow
if ($env:VULKAN_SDK) {
    $vulkanHpp = Join-Path $env:VULKAN_SDK "Include\vulkan\vulkan.hpp"
    if (Test-Path $vulkanHpp) {
        $fileSize = (Get-Item $vulkanHpp).Length / 1MB
        Write-Host "  ✓ vulkan.hpp 存在 ($([math]::Round($fileSize, 2)) MB)" -ForegroundColor Green
        Write-Host "    位置: $vulkanHpp" -ForegroundColor Gray
    } else {
        Write-Host "  ✗ vulkan.hpp 不存在" -ForegroundColor Red
        Write-Host "    预期位置: $vulkanHpp" -ForegroundColor Yellow
        $allGood = $false
    }
} else {
    Write-Host "  ⊘ 跳过（VULKAN_SDK 未设置）" -ForegroundColor Gray
}
Write-Host ""

# 4. 检查 Android SDK 环境变量
Write-Host "[4] 检查 Android SDK 环境变量..." -ForegroundColor Yellow
if ($env:ANDROID_HOME) {
    Write-Host "  ✓ ANDROID_HOME = $env:ANDROID_HOME" -ForegroundColor Green
} elseif ($env:ANDROID_SDK_ROOT) {
    Write-Host "  ✓ ANDROID_SDK_ROOT = $env:ANDROID_SDK_ROOT" -ForegroundColor Green
} else {
    Write-Host "  ✗ ANDROID_HOME 或 ANDROID_SDK_ROOT 未设置" -ForegroundColor Red
    Write-Host "    Android Studio 应该会自动设置此变量" -ForegroundColor Yellow
    $allGood = $false
}
Write-Host ""

# 5. 检查 Ninja 构建工具
Write-Host "[5] 检查 Ninja 构建工具..." -ForegroundColor Yellow
$ninjaPath = $null
if ($env:ANDROID_HOME) {
    $ninjaPath = Join-Path $env:ANDROID_HOME "cmake\3.31.6\bin\ninja.exe"
} elseif ($env:ANDROID_SDK_ROOT) {
    $ninjaPath = Join-Path $env:ANDROID_SDK_ROOT "cmake\3.31.6\bin\ninja.exe"
}

if ($ninjaPath -and (Test-Path $ninjaPath)) {
    Write-Host "  ✓ Ninja 已安装: $ninjaPath" -ForegroundColor Green
} else {
    # 尝试其他常见路径
    $commonPaths = @(
        "C:\Android\Sdk\cmake\3.31.6\bin\ninja.exe",
        "E:\android\android_sdk\cmake\3.31.6\bin\ninja.exe"
    )
    $found = $false
    foreach ($path in $commonPaths) {
        if (Test-Path $path) {
            Write-Host "  ✓ Ninja 已安装: $path" -ForegroundColor Green
            $found = $true
            break
        }
    }
    if (-not $found) {
        Write-Host "  ⚠ Ninja 未找到（可能使用其他版本）" -ForegroundColor Yellow
        Write-Host "    CMakeLists.txt 会尝试自动检测" -ForegroundColor Gray
    }
}
Write-Host ""

# 6. 检查 Android NDK
Write-Host "[6] 检查 Android NDK..." -ForegroundColor Yellow
if ($env:ANDROID_NDK_HOME) {
    Write-Host "  ✓ ANDROID_NDK_HOME = $env:ANDROID_NDK_HOME" -ForegroundColor Green

    # 检查 NDK 版本
    $ndkVersion = Split-Path $env:ANDROID_NDK_HOME -Leaf
    Write-Host "    版本: $ndkVersion" -ForegroundColor Gray

    # 检查 NDK 的 Vulkan 头文件
    $ndkVulkan = Join-Path $env:ANDROID_NDK_HOME "sysroot\usr\include\vulkan\vulkan_core.h"
    if (Test-Path $ndkVulkan) {
        Write-Host "  ✓ NDK Vulkan 头文件存在" -ForegroundColor Green
    } else {
        Write-Host "  ✗ NDK Vulkan 头文件不存在" -ForegroundColor Red
        Write-Host "    请升级 NDK 到 r21 或更高版本" -ForegroundColor Yellow
        $allGood = $false
    }
} else {
    Write-Host "  ⚠ ANDROID_NDK_HOME 未设置（通常由 Gradle 自动管理）" -ForegroundColor Yellow
}
Write-Host ""

# 7. 检查 llama.android 编译配置
Write-Host "[7] 检查 llama.android Vulkan 配置..." -ForegroundColor Yellow
$gradleFile = Join-Path $PSScriptRoot "examples\llama.android\lib\build.gradle.kts"
if (Test-Path $gradleFile) {
    $content = Get-Content $gradleFile -Raw
    if ($content -match "-DGGML_VULKAN=ON") {
        Write-Host "  ✓ GGML_VULKAN=ON 已启用" -ForegroundColor Green
    } else {
        Write-Host "  ✗ GGML_VULKAN=ON 未启用" -ForegroundColor Red
        Write-Host "    请在 build.gradle.kts 中添加 -DGGML_VULKAN=ON" -ForegroundColor Yellow
        $allGood = $false
    }
} else {
    Write-Host "  ⊘ build.gradle.kts 未找到" -ForegroundColor Gray
}
Write-Host ""

# 8. 检查 GGUFChat CMakeLists.txt
Write-Host "[8] 检查 GGUFChat Vulkan 配置..." -ForegroundColor Yellow
$cmakeFile = Join-Path $PSScriptRoot "GGUFChat\llama-android\src\main\cpp\CMakeLists.txt"
if (Test-Path $cmakeFile) {
    $content = Get-Content $cmakeFile -Raw
    if ($content -match "libggml-vulkan\.so") {
        Write-Host "  ✓ libggml-vulkan.so 已配置" -ForegroundColor Green
    } else {
        Write-Host "  ✗ libggml-vulkan.so 未配置" -ForegroundColor Red
        Write-Host "    CMakeLists.txt 需要添加 Vulkan 库依赖" -ForegroundColor Yellow
        $allGood = $false
    }
} else {
    Write-Host "  ⊘ CMakeLists.txt 未找到" -ForegroundColor Gray
}
Write-Host ""

# 总结
Write-Host "========================================" -ForegroundColor Cyan
if ($allGood) {
    Write-Host "✓ 所有检查通过!" -ForegroundColor Green
    Write-Host ""
    Write-Host "下一步操作:" -ForegroundColor Yellow
    Write-Host "  1. 编译 llama.android 的 Vulkan 支持:" -ForegroundColor White
    Write-Host "     cd examples\llama.android" -ForegroundColor Gray
    Write-Host "     .\gradlew clean" -ForegroundColor Gray
    Write-Host "     .\gradlew :lib:assembleRelease" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  2. 复制库文件到 GGUFChat:" -ForegroundColor White
    Write-Host "     .\copy_vulkan_to_ggufchat.ps1" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  3. 编译 GGUFChat:" -ForegroundColor White
    Write-Host "     cd GGUFChat" -ForegroundColor Gray
    Write-Host "     .\gradlew assembleRelease" -ForegroundColor Gray
} else {
    Write-Host "✗ 存在问题需要修复" -ForegroundColor Red
    Write-Host ""
    Write-Host "主要步骤:" -ForegroundColor Yellow
    Write-Host "  1. 安装 Vulkan SDK for Windows:" -ForegroundColor White
    Write-Host "     https://vulkan.lunarg.com/sdk/home#windows" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  2. 重启终端/IDE 使环境变量生效" -ForegroundColor White
    Write-Host ""
    Write-Host "  3. 重新运行此脚本验证" -ForegroundColor White
    Write-Host ""
    Write-Host "详细指南: WINDOWS_VULKAN_ANDROID_FIX_CN.md" -ForegroundColor Cyan
}
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
