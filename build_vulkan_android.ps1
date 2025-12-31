# 编译 llama.android 的 Vulkan 支持
# 使用方法: .\build_vulkan_android.ps1

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "编译 llama.android Vulkan 支持" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查是否在正确的目录
$projectRoot = $PSScriptRoot
$androidLibPath = Join-Path $projectRoot "examples\llama.android"

if (-not (Test-Path $androidLibPath)) {
    Write-Host "错误: 找不到 examples\llama.android 目录" -ForegroundColor Red
    Write-Host "请确保在 llama.cpp 项目根目录运行此脚本" -ForegroundColor Yellow
    exit 1
}

# 切换到 Android 项目目录
Write-Host "[1] 切换到 Android 项目目录..." -ForegroundColor Yellow
Set-Location $androidLibPath
Write-Host "  当前目录: $(Get-Location)" -ForegroundColor Gray
Write-Host ""

# 清理之前的构建
Write-Host "[2] 清理之前的构建..." -ForegroundColor Yellow
if (Test-Path "lib\.cxx") {
    Write-Host "  删除 lib\.cxx 目录..." -ForegroundColor Gray
    Remove-Item -Recurse -Force "lib\.cxx" -ErrorAction SilentlyContinue
}

Write-Host "  运行 gradlew clean..." -ForegroundColor Gray
& .\gradlew clean
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ gradlew clean 失败" -ForegroundColor Red
    exit 1
}
Write-Host "  ✓ 清理完成" -ForegroundColor Green
Write-Host ""

# 编译 Release 版本
Write-Host "[3] 编译 Vulkan 支持的库 (Release)..." -ForegroundColor Yellow
Write-Host "  这可能需要 5-10 分钟，请耐心等待..." -ForegroundColor Gray
Write-Host ""

$startTime = Get-Date
& .\gradlew :lib:assembleRelease
$exitCode = $LASTEXITCODE
$duration = (Get-Date) - $startTime

Write-Host ""
if ($exitCode -eq 0) {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "✓ 编译成功!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "耗时: $([math]::Round($duration.TotalMinutes, 1)) 分钟" -ForegroundColor Yellow
    Write-Host ""

    # 查找生成的库文件
    Write-Host "[4] 检查生成的库文件..." -ForegroundColor Yellow
    $cxxPath = "lib\.cxx\Release"

    if (Test-Path $cxxPath) {
        $buildDirs = Get-ChildItem -Path $cxxPath -Directory
        if ($buildDirs.Count -gt 0) {
            $buildDir = $buildDirs[0].FullName
            $vulkanLib = Get-ChildItem -Path $buildDir -Filter "libggml-vulkan.so" -Recurse | Select-Object -First 1

            if ($vulkanLib) {
                $fileSize = $vulkanLib.Length / 1MB
                Write-Host "  ✓ libggml-vulkan.so 已生成!" -ForegroundColor Green
                Write-Host "    位置: $($vulkanLib.FullName)" -ForegroundColor Gray
                Write-Host "    大小: $([math]::Round($fileSize, 2)) MB" -ForegroundColor Gray
                Write-Host ""

                # 列出所有生成的 .so 文件
                Write-Host "  所有生成的库文件:" -ForegroundColor Cyan
                $allLibs = Get-ChildItem -Path $buildDir -Filter "*.so" -Recurse | Where-Object { $_.Name -match "^lib(ggml|llama)" }
                foreach ($lib in $allLibs) {
                    $size = $lib.Length / 1MB
                    Write-Host "    - $($lib.Name) ($([math]::Round($size, 2)) MB)" -ForegroundColor Gray
                }
                Write-Host ""

                Write-Host "下一步操作:" -ForegroundColor Yellow
                Write-Host "  1. 返回项目根目录: cd ..\.." -ForegroundColor White
                Write-Host "  2. 复制库文件到 GGUFChat: .\copy_vulkan_to_ggufchat.ps1" -ForegroundColor White
                Write-Host "  3. 编译 GGUFChat: cd GGUFChat && .\gradlew assembleRelease" -ForegroundColor White
            } else {
                Write-Host "  ✗ libggml-vulkan.so 未找到!" -ForegroundColor Red
                Write-Host "    请检查编译日志中的 GGML_VULKAN 相关输出" -ForegroundColor Yellow
            }
        } else {
            Write-Host "  ✗ 找不到构建目录" -ForegroundColor Red
        }
    } else {
        Write-Host "  ✗ .cxx/Release 目录不存在" -ForegroundColor Red
    }
} else {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "✗ 编译失败" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "耗时: $([math]::Round($duration.TotalMinutes, 1)) 分钟" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "故障排除:" -ForegroundColor Yellow
    Write-Host "  1. 检查 Vulkan SDK 是否正确安装:" -ForegroundColor White
    Write-Host "     .\check_my_vulkan.ps1" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  2. 验证 Vulkan SDK 路径:" -ForegroundColor White
    Write-Host "     echo `$env:VULKAN_SDK" -ForegroundColor Gray
    Write-Host "     dir \"`$env:VULKAN_SDK\Include\vulkan\vulkan.hpp\"" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  3. 检查 Android NDK:" -ForegroundColor White
    Write-Host "     echo `$env:ANDROID_NDK_HOME" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  4. 查看详细错误日志，搜索 'vulkan' 相关错误" -ForegroundColor White
    Write-Host ""
    exit 1
}

Write-Host ""
