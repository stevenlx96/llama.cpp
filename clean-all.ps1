# Nuclear Clean Script - Remove all CMake and Gradle caches
# PowerShell version (English)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Nuclear Clean - Remove All Caches" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$rootDir = $PSScriptRoot
if (-not $rootDir) {
    $rootDir = Get-Location
}

Set-Location $rootDir

Write-Host "[1/6] Cleaning Android project caches..." -ForegroundColor Yellow
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
        Write-Host "    Removing $dir" -ForegroundColor Gray
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

Write-Host "[2/6] Cleaning global Gradle cache..." -ForegroundColor Yellow
$gradleCache = "$env:USERPROFILE\.gradle\caches"
if (Test-Path $gradleCache) {
    Write-Host "    Removing $gradleCache" -ForegroundColor Gray
    Remove-Item -Recurse -Force $gradleCache -ErrorAction SilentlyContinue
}

Write-Host "[3/6] Cleaning main project build cache..." -ForegroundColor Yellow
Set-Location $rootDir

$rootDirsToRemove = @(
    "build",
    "CMakeCache.txt",
    "CMakeFiles"
)

foreach ($dir in $rootDirsToRemove) {
    if (Test-Path $dir) {
        Write-Host "    Removing $dir" -ForegroundColor Gray
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

Write-Host "[4/6] Cleaning ggml build cache..." -ForegroundColor Yellow
Set-Location ggml\src

$ggmlDirsToRemove = @(
    "build",
    "CMakeCache.txt",
    "CMakeFiles"
)

foreach ($dir in $ggmlDirsToRemove) {
    if (Test-Path $dir) {
        Write-Host "    Removing ggml\src\$dir" -ForegroundColor Gray
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

Set-Location $rootDir

Write-Host "[5/6] Cleaning ggml-vulkan build cache..." -ForegroundColor Yellow
Set-Location ggml\src\ggml-vulkan

$vulkanDirsToRemove = @(
    "build",
    "CMakeCache.txt",
    "CMakeFiles"
)

foreach ($dir in $vulkanDirsToRemove) {
    if (Test-Path $dir) {
        Write-Host "    Removing ggml\src\ggml-vulkan\$dir" -ForegroundColor Gray
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

Set-Location $rootDir

Write-Host "[6/6] Removing all generated host-toolchain.cmake files..." -ForegroundColor Yellow
Get-ChildItem -Path . -Recurse -Filter "host-toolchain.cmake" -File -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "    Removing $($_.FullName)" -ForegroundColor Gray
    Remove-Item -Force $_.FullName -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Clean completed!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Close Android Studio" -ForegroundColor White
Write-Host "2. Reopen Android Studio" -ForegroundColor White
Write-Host "3. Run: .\gradlew :lib:assembleRelease" -ForegroundColor White
Write-Host ""
