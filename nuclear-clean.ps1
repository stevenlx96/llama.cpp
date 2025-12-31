# Nuclear Clean Script - 彻底清除所有 CMake 和 Gradle 缓存
# PowerShell 版本
# 在 E:\MyGithub\llama.cpp 目录下运行

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Nuclear Clean - 彻底清除所有缓存" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$rootDir = $PSScriptRoot
if (-not $rootDir) {
    $rootDir = Get-Location
}

Set-Location $rootDir

Write-Host "[1/6] 清除 Android 项目缓存..." -ForegroundColor Yellow
Set-Location examples\llama.android

$dirsToRemove = @(
    "lib\.cxx",
    "lib\build",
    "app\.cxx",
    "app\build",
    ".gradle",
    "build"
)

foreach ($dir in $dirsToRemove) {
    if (Test-Path $dir) {
        Write-Host "    删除 $dir" -ForegroundColor Gray
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

Write-Host "[2/6] 清除全局 Gradle 缓存..." -ForegroundColor Yellow
$gradleCache = "$env:USERPROFILE\.gradle\caches"
if (Test-Path $gradleCache) {
    Write-Host "    删除 $gradleCache" -ForegroundColor Gray
    Remove-Item -Recurse -Force $gradleCache -ErrorAction SilentlyContinue
}

Write-Host "[3/6] 清除主项目构建缓存..." -ForegroundColor Yellow
Set-Location $rootDir

$rootDirsToRemove = @(
    "build",
    "CMakeCache.txt",
    "CMakeFiles"
)

foreach ($dir in $rootDirsToRemove) {
    if (Test-Path $dir) {
        Write-Host "    删除 $dir" -ForegroundColor Gray
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

Write-Host "[4/6] 清除 ggml 构建缓存..." -ForegroundColor Yellow
Set-Location ggml\src

$ggmlDirsToRemove = @(
    "build",
    "CMakeCache.txt",
    "CMakeFiles"
)

foreach ($dir in $ggmlDirsToRemove) {
    if (Test-Path $dir) {
        Write-Host "    删除 ggml\src\$dir" -ForegroundColor Gray
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

Set-Location $rootDir

Write-Host "[5/6] 清除 ggml-vulkan 构建缓存..." -ForegroundColor Yellow
Set-Location ggml\src\ggml-vulkan

$vulkanDirsToRemove = @(
    "build",
    "CMakeCache.txt",
    "CMakeFiles"
)

foreach ($dir in $vulkanDirsToRemove) {
    if (Test-Path $dir) {
        Write-Host "    删除 ggml\src\ggml-vulkan\$dir" -ForegroundColor Gray
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

Set-Location $rootDir

Write-Host "[6/6] 清除所有生成的 host-toolchain.cmake 文件..." -ForegroundColor Yellow
Get-ChildItem -Path . -Recurse -Filter "host-toolchain.cmake" -File -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "    删除 $($_.FullName)" -ForegroundColor Gray
    Remove-Item -Force $_.FullName -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "清理完成！" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "下一步操作：" -ForegroundColor Cyan
Write-Host "1. 关闭 Android Studio" -ForegroundColor White
Write-Host "2. 重新打开 Android Studio" -ForegroundColor White
Write-Host "3. 运行: .\gradlew :lib:assembleRelease" -ForegroundColor White
Write-Host ""
