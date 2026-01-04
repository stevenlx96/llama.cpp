# 检查当前系统的 Vulkan 配置
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "检查你的 Vulkan SDK 配置" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查 Vulkan SDK 路径
Write-Host "[1] Vulkan SDK 路径检查..." -ForegroundColor Yellow
$vulkanPath = "C:\VulkanSDK\1.4.335.0"
if (Test-Path $vulkanPath) {
    Write-Host "  ✓ Vulkan SDK 存在: $vulkanPath" -ForegroundColor Green

    # 检查 vulkan.hpp
    $vulkanHpp = Join-Path $vulkanPath "Include\vulkan\vulkan.hpp"
    if (Test-Path $vulkanHpp) {
        Write-Host "  ✓ vulkan.hpp 存在" -ForegroundColor Green
        Write-Host "    位置: $vulkanHpp" -ForegroundColor Gray
    } else {
        Write-Host "  ✗ vulkan.hpp 不存在!" -ForegroundColor Red
        Write-Host "    预期位置: $vulkanHpp" -ForegroundColor Yellow
        # 列出 Include/vulkan 目录内容
        $vulkanInclude = Join-Path $vulkanPath "Include\vulkan"
        if (Test-Path $vulkanInclude) {
            Write-Host "    实际文件:" -ForegroundColor Yellow
            Get-ChildItem $vulkanInclude | ForEach-Object { Write-Host "      - $($_.Name)" -ForegroundColor Gray }
        }
    }

    # 检查 glslc
    $glslc = Join-Path $vulkanPath "Bin\glslc.exe"
    if (Test-Path $glslc) {
        Write-Host "  ✓ glslc.exe 存在" -ForegroundColor Green
    } else {
        Write-Host "  ✗ glslc.exe 不存在!" -ForegroundColor Red
    }
} else {
    Write-Host "  ✗ Vulkan SDK 路径不存在: $vulkanPath" -ForegroundColor Red
}
Write-Host ""

# 检查环境变量
Write-Host "[2] 环境变量检查..." -ForegroundColor Yellow
if ($env:VULKAN_SDK) {
    Write-Host "  ✓ VULKAN_SDK = $env:VULKAN_SDK" -ForegroundColor Green
} else {
    Write-Host "  ✗ VULKAN_SDK 未设置" -ForegroundColor Red
    Write-Host "    需要设置为: C:\VulkanSDK\1.4.335.0" -ForegroundColor Yellow
}

if ($env:VK_SDK_PATH) {
    Write-Host "  ✓ VK_SDK_PATH = $env:VK_SDK_PATH" -ForegroundColor Green
} else {
    Write-Host "  ⚠ VK_SDK_PATH 未设置（可选）" -ForegroundColor Yellow
}

# 检查 PATH
Write-Host ""
Write-Host "[3] PATH 环境变量检查..." -ForegroundColor Yellow
if ($env:PATH -match "VulkanSDK") {
    Write-Host "  ✓ PATH 包含 VulkanSDK" -ForegroundColor Green
    $env:PATH -split ';' | Where-Object { $_ -match "Vulkan" } | ForEach-Object {
        Write-Host "    - $_" -ForegroundColor Gray
    }
} else {
    Write-Host "  ✗ PATH 不包含 VulkanSDK" -ForegroundColor Red
}

# 测试 glslc
Write-Host ""
Write-Host "[4] 测试 glslc 命令..." -ForegroundColor Yellow
try {
    $glslcOutput = & glslc --version 2>&1 | Select-Object -First 1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ glslc 可用: $glslcOutput" -ForegroundColor Green
    } else {
        Write-Host "  ✗ glslc 执行失败" -ForegroundColor Red
    }
} catch {
    Write-Host "  ✗ glslc 命令不可用" -ForegroundColor Red
    Write-Host "    建议添加到 PATH: C:\VulkanSDK\1.4.335.0\Bin" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "修复建议" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "如果 VULKAN_SDK 环境变量未设置，请在 PowerShell 中运行：" -ForegroundColor Yellow
Write-Host '  $env:VULKAN_SDK = "C:\VulkanSDK\1.4.335.0"' -ForegroundColor White
Write-Host '  $env:PATH = "C:\VulkanSDK\1.4.335.0\Bin;" + $env:PATH' -ForegroundColor White
Write-Host ""
Write-Host "或永久设置（系统环境变量）：" -ForegroundColor Yellow
Write-Host "  1. Win+R 输入: sysdm.cpl" -ForegroundColor White
Write-Host "  2. 高级 → 环境变量" -ForegroundColor White
Write-Host "  3. 新建系统变量:" -ForegroundColor White
Write-Host "     名称: VULKAN_SDK" -ForegroundColor White
Write-Host "     值: C:\VulkanSDK\1.4.335.0" -ForegroundColor White
Write-Host "  4. 编辑 PATH，添加: %VULKAN_SDK%\Bin" -ForegroundColor White
Write-Host ""
