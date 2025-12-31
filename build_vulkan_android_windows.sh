#!/bin/bash
# Windows环境下编译启用Vulkan的llama.cpp Android库

# ============ 配置区 ============
# 请根据你的实际路径修改以下变量

# Android SDK路径（通常在用户目录下）
# 示例: C:/Users/YourName/AppData/Local/Android/Sdk
ANDROID_SDK_ROOT="C:/Users/$USERNAME/AppData/Local/Android/Sdk"

# NDK版本（查看 $ANDROID_SDK_ROOT/ndk 目录下的版本）
NDK_VERSION="26.1.10909125"  # 修改为你的NDK版本

# CMake版本（查看 $ANDROID_SDK_ROOT/cmake 目录下的版本）
CMAKE_VERSION="3.22.1"  # 修改为你的CMake版本

# ============ 自动配置 ============
export ANDROID_NDK="$ANDROID_SDK_ROOT/ndk/$NDK_VERSION"
CMAKE_BIN="$ANDROID_SDK_ROOT/cmake/$CMAKE_VERSION/bin/cmake"
NINJA_BIN="$ANDROID_SDK_ROOT/cmake/$CMAKE_VERSION/bin/ninja"

# 转换为MINGW路径格式
ANDROID_NDK=$(cygpath -u "$ANDROID_NDK" 2>/dev/null || echo "$ANDROID_NDK")
CMAKE_BIN=$(cygpath -u "$CMAKE_BIN" 2>/dev/null || echo "$CMAKE_BIN")
NINJA_BIN=$(cygpath -u "$NINJA_BIN" 2>/dev/null || echo "$NINJA_BIN")

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=== Android Vulkan编译脚本 (Windows) ===${NC}"
echo ""
echo "配置信息："
echo "  Android SDK: $ANDROID_SDK_ROOT"
echo "  NDK路径: $ANDROID_NDK"
echo "  CMake路径: $CMAKE_BIN"
echo "  Ninja路径: $NINJA_BIN"
echo ""

# ============ 检查依赖 ============
echo -e "${YELLOW}检查依赖...${NC}"

if [ ! -d "$ANDROID_NDK" ]; then
    echo -e "${RED}错误: NDK目录不存在: $ANDROID_NDK${NC}"
    echo ""
    echo "请执行以下步骤："
    echo "1. 打开Android Studio"
    echo "2. Tools -> SDK Manager -> SDK Tools"
    echo "3. 安装 NDK (Side by side)"
    echo "4. 记下安装的版本号"
    echo "5. 修改本脚本第9行的NDK_VERSION变量"
    exit 1
fi

if [ ! -f "$CMAKE_BIN" ]; then
    echo -e "${RED}错误: CMake不存在: $CMAKE_BIN${NC}"
    echo ""
    echo "请执行以下步骤："
    echo "1. 打开Android Studio"
    echo "2. Tools -> SDK Manager -> SDK Tools"
    echo "3. 安装 CMake"
    echo "4. 记下安装的版本号"
    echo "5. 修改本脚本第15行的CMAKE_VERSION变量"
    exit 1
fi

if [ ! -f "$NINJA_BIN" ]; then
    echo -e "${RED}错误: Ninja不存在: $NINJA_BIN${NC}"
    echo ""
    echo "Ninja通常和CMake一起安装，请检查："
    echo "  $ANDROID_SDK_ROOT/cmake/$CMAKE_VERSION/bin/"
    echo ""
    echo "如果确实没有ninja，尝试重新安装CMake"
    exit 1
fi

echo -e "${GREEN}✓ 所有依赖已找到${NC}"
echo ""

# ============ 编译配置 ============
ABI="arm64-v8a"  # 或 "armeabi-v7a"
BUILD_DIR="build-android-vulkan-${ABI}"

echo -e "${YELLOW}开始编译...${NC}"
echo "  目标ABI: $ABI"
echo "  构建目录: $BUILD_DIR"
echo ""

# 清理之前的编译
if [ -d "$BUILD_DIR" ]; then
    echo "清理旧的构建目录..."
    rm -rf "$BUILD_DIR"
fi

# 配置CMake
echo -e "${YELLOW}配置CMake...${NC}"
"$CMAKE_BIN" -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_MAKE_PROGRAM="$NINJA_BIN" \
  -DBUILD_SHARED_LIBS=ON \
  -DGGML_VULKAN=ON \
  -DGGML_BACKEND_DL=ON \
  -DGGML_CPU_ALL_VARIANTS=ON \
  -DGGML_NATIVE=OFF \
  -DGGML_OPENMP=ON \
  -DGGML_LLAMAFILE=OFF \
  -DLLAMA_BUILD_COMMON=ON \
  -DLLAMA_CURL=OFF \
  -G Ninja

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake配置失败！${NC}"
    exit 1
fi

# 编译
echo ""
echo -e "${YELLOW}开始编译（这可能需要5-15分钟）...${NC}"
"$CMAKE_BIN" --build "$BUILD_DIR" --config Release -j$(nproc 2>/dev/null || echo 4)

if [ $? -ne 0 ]; then
    echo -e "${RED}编译失败！${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}=== 编译完成！ ===${NC}"
echo ""
echo "生成的库文件："
find "$BUILD_DIR" -name "*.so" -type f | grep -E "(libggml|libllama)" | sort | while read f; do
    size=$(du -h "$f" | cut -f1)
    echo "  [${size}] $f"
done

echo ""
echo -e "${GREEN}下一步：${NC}"
echo "  运行: ./copy_vulkan_libs.sh"
