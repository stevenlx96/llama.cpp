# 复制 Vulkan 编译产物到 GGUFChat 项目
# 使用方法: .\copy_vulkan_to_ggufchat.ps1

param(
    [string]$BuildType = "Release",  # Release 或 Debug
    [string]$ABI = "arm64-v8a"       # arm64-v8a 或 armeabi-v7a
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "复制 Vulkan 库到 GGUFChat 项目" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "构建类型: $BuildType" -ForegroundColor Yellow
Write-Host "ABI: $ABI" -ForegroundColor Yellow
Write-Host ""

# 项目根目录
$ROOT = $PSScriptRoot

# 查找 llama.android 的构建产物
$ANDROID_LIB_PATH = Join-Path $ROOT "examples\llama.android\lib"
$CXX_BUILD_PATH = Join-Path $ANDROID_LIB_PATH ".cxx\$BuildType"

# 查找实际的构建目录（hash 可能不同）
$BUILD_DIRS = Get-ChildItem -Path $CXX_BUILD_PATH -Directory -ErrorAction SilentlyContinue

if ($BUILD_DIRS.Count -eq 0) {
    Write-Host "错误: 找不到构建目录在 $CXX_BUILD_PATH" -ForegroundColor Red
    Write-Host "请先编译 llama.android 项目:" -ForegroundColor Yellow
    Write-Host "  cd examples\llama.android" -ForegroundColor Yellow
    Write-Host "  .\gradlew :lib:assemble$BuildType" -ForegroundColor Yellow
    exit 1
}

# 使用第一个找到的构建目录
$BUILD_HASH_DIR = $BUILD_DIRS[0].FullName
$SOURCE_BASE = Join-Path $BUILD_HASH_DIR "$ABI\build-llama"

Write-Host "源目录: $SOURCE_BASE" -ForegroundColor Green

# 目标目录
$DEST_DIR = Join-Path $ROOT "GGUFChat\llama-android\src\main\jniLibs\$ABI"

# 创建目标目录
if (-not (Test-Path $DEST_DIR)) {
    New-Item -ItemType Directory -Force -Path $DEST_DIR | Out-Null
    Write-Host "创建目录: $DEST_DIR" -ForegroundColor Green
}

# 定义需要复制的库文件
$LIBS = @(
    @{
        Source = "ggml\src\libggml-base.so"
        Name = "libggml-base.so"
    },
    @{
        Source = "ggml\src\libggml-cpu.so"
        Name = "libggml-cpu.so"
    },
    @{
        Source = "ggml\src\ggml-vulkan\libggml-vulkan.so"
        Name = "libggml-vulkan.so"
        IsVulkan = $true
    },
    @{
        Source = "ggml\src\libggml.so"
        Name = "libggml.so"
    },
    @{
        Source = "src\libllama.so"
        Name = "libllama.so"
    }
)

Write-Host ""
Write-Host "开始复制库文件..." -ForegroundColor Cyan
Write-Host ""

$success = 0
$failed = 0

foreach ($lib in $LIBS) {
    $sourcePath = Join-Path $SOURCE_BASE $lib.Source
    $destPath = Join-Path $DEST_DIR $lib.Name

    if (Test-Path $sourcePath) {
        try {
            Copy-Item -Path $sourcePath -Destination $destPath -Force
            $fileSize = (Get-Item $sourcePath).Length / 1MB
            $marker = if ($lib.IsVulkan) { "⭐" } else { "✓" }
            Write-Host "$marker $($lib.Name) ($([math]::Round($fileSize, 2)) MB)" -ForegroundColor Green
            $success++
        } catch {
            Write-Host "✗ $($lib.Name) - 复制失败: $_" -ForegroundColor Red
            $failed++
        }
    } else {
        Write-Host "✗ $($lib.Name) - 源文件不存在: $sourcePath" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "复制完成!" -ForegroundColor Cyan
Write-Host "成功: $success 个库文件" -ForegroundColor Green
if ($failed -gt 0) {
    Write-Host "失败: $failed 个库文件" -ForegroundColor Red
}
Write-Host "目标目录: $DEST_DIR" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan

# 验证所有文件
Write-Host ""
Write-Host "验证库文件..." -ForegroundColor Cyan
$allFiles = Get-ChildItem -Path $DEST_DIR -Filter "*.so"
foreach ($file in $allFiles) {
    $size = $file.Length / 1MB
    Write-Host "  $($file.Name) - $([math]::Round($size, 2)) MB" -ForegroundColor Gray
}

# 检查 Vulkan 库
$vulkanLib = Join-Path $DEST_DIR "libggml-vulkan.so"
if (Test-Path $vulkanLib) {
    Write-Host ""
    Write-Host "✓ Vulkan 支持已启用!" -ForegroundColor Green
    Write-Host ""
    Write-Host "下一步:" -ForegroundColor Yellow
    Write-Host "  1. cd GGUFChat" -ForegroundColor White
    Write-Host "  2. .\gradlew clean" -ForegroundColor White
    Write-Host "  3. .\gradlew :llama-android:assembleRelease" -ForegroundColor White
    Write-Host "  4. .\gradlew assembleRelease (编译整个 App)" -ForegroundColor White
} else {
    Write-Host ""
    Write-Host "警告: libggml-vulkan.so 未找到!" -ForegroundColor Red
    Write-Host "请确保使用 -DGGML_VULKAN=ON 编译 llama.android" -ForegroundColor Yellow
}

Write-Host ""
