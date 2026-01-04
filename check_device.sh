#!/bin/bash
# 检查 Android 设备芯片和 NPU 支持情况

echo "========================================="
echo "Android 设备信息检查工具"
echo "========================================="
echo ""

# 检查 ADB 是否连接
if ! command -v adb &> /dev/null; then
    echo "❌ 未找到 adb 命令"
    echo "请安装 Android SDK Platform Tools"
    exit 1
fi

echo "检查设备连接..."
adb devices -l

echo ""
echo "----------------------------------------"
echo "设备基本信息："
echo "----------------------------------------"
echo "制造商: $(adb shell getprop ro.product.manufacturer)"
echo "型号: $(adb shell getprop ro.product.model)"
echo "Android 版本: $(adb shell getprop ro.build.version.release)"
echo "SDK 版本: $(adb shell getprop ro.build.version.sdk)"
echo ""

echo "----------------------------------------"
echo "芯片信息："
echo "----------------------------------------"
echo "硬件平台: $(adb shell getprop ro.product.board)"
echo "CPU 架构: $(adb shell getprop ro.product.cpu.abi)"
echo "SoC 型号: $(adb shell getprop ro.board.platform)"
echo ""

# 检查是否是高通芯片
SOC=$(adb shell getprop ro.board.platform)
if [[ "$SOC" == *"taro"* ]] || [[ "$SOC" == *"kalama"* ]] || [[ "$SOC" == *"pineapple"* ]]; then
    echo "✅ 检测到高通 Snapdragon 芯片"
    echo "   平台代号: $SOC"

    # 检查 Hexagon DSP 支持
    echo ""
    echo "----------------------------------------"
    echo "Hexagon NPU 支持检查："
    echo "----------------------------------------"

    # 检查 HTP 库文件
    echo "检查 HTP 库文件..."
    adb shell "ls -la /vendor/lib64/*hexagon* 2>/dev/null || echo '未找到 Hexagon 库'"
    adb shell "ls -la /vendor/lib64/*cdsp* 2>/dev/null || echo '未找到 CDSP 库'"

    # 检查 DSP 设备节点
    echo ""
    echo "检查 DSP 设备节点..."
    adb shell "ls -la /dev/cdsp* 2>/dev/null || echo '未找到 DSP 设备节点'"

elif [[ "$SOC" == *"exynos"* ]] || [[ "$SOC" == *"s5e"* ]]; then
    echo "⚠️  检测到三星 Exynos 芯片"
    echo "   平台代号: $SOC"
    echo "   → Exynos 不支持 Hexagon NPU"
    echo "   → 可以使用 OpenCL 访问 Mali GPU"
    echo "   → 或通过 Android NNAPI 访问 NPU"
else
    echo "⚠️  未识别的芯片平台: $SOC"
fi

echo ""
echo "----------------------------------------"
echo "GPU 信息："
echo "----------------------------------------"
adb shell dumpsys SurfaceFlinger | grep "GLES" || echo "无法获取 GPU 信息"

echo ""
echo "----------------------------------------"
echo "OpenCL 支持检查："
echo "----------------------------------------"
adb shell "ls -la /vendor/lib64/*opencl* 2>/dev/null || echo '未找到 OpenCL 库'"
adb shell "ls -la /system/lib64/*opencl* 2>/dev/null || echo '系统未找到 OpenCL 库'"

echo ""
echo "----------------------------------------"
echo "Vulkan 支持检查："
echo "----------------------------------------"
adb shell "ls -la /vendor/lib64/*vulkan* 2>/dev/null || echo '未找到 Vulkan 库'"

echo ""
echo "========================================="
echo "检查完成"
echo "========================================="
