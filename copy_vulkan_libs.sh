#!/bin/bash
# 自动复制Vulkan库文件到GGUFChat项目

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 配置
ABI="arm64-v8a"  # 可以改为 armeabi-v7a
BUILD_DIR="build-android-vulkan-${ABI}"
TARGET_DIR="GGUFChat/llama-android/src/main/jniLibs/${ABI}"

echo -e "${GREEN}=== GGUFChat Vulkan库文件复制脚本 ===${NC}"

# 检查编译目录是否存在
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}错误: 编译目录不存在: $BUILD_DIR${NC}"
    echo -e "${YELLOW}请先运行: ./build_vulkan_android.sh${NC}"
    exit 1
fi

# 创建目标目录
echo -e "${GREEN}创建目标目录: $TARGET_DIR${NC}"
mkdir -p "$TARGET_DIR"

# 定义需要复制的库文件
declare -A LIBS=(
    ["libggml-base.so"]="$BUILD_DIR/ggml/src/libggml-base.so"
    ["libggml-cpu.so"]="$BUILD_DIR/ggml/src/libggml-cpu.so"
    ["libggml-vulkan.so"]="$BUILD_DIR/ggml/src/ggml-vulkan/libggml-vulkan.so"
    ["libggml.so"]="$BUILD_DIR/ggml/src/libggml.so"
    ["libllama.so"]="$BUILD_DIR/src/libllama.so"
)

# 复制文件
echo -e "${GREEN}开始复制库文件...${NC}"
COPIED=0
FAILED=0

for lib_name in "${!LIBS[@]}"; do
    src="${LIBS[$lib_name]}"
    dst="$TARGET_DIR/$lib_name"

    if [ -f "$src" ]; then
        cp "$src" "$dst"
        size=$(du -h "$dst" | cut -f1)
        echo -e "  ${GREEN}✓${NC} $lib_name ($size)"
        COPIED=$((COPIED + 1))
    else
        echo -e "  ${RED}✗${NC} $lib_name (未找到: $src)"
        FAILED=$((FAILED + 1))
    fi
done

# 复制头文件
echo -e "\n${GREEN}复制Vulkan头文件...${NC}"
HEADER_SRC="ggml/include/ggml-vulkan.h"
HEADER_DST="GGUFChat/llama-android/src/main/cpp/include/ggml-vulkan.h"

if [ -f "$HEADER_SRC" ]; then
    mkdir -p "$(dirname "$HEADER_DST")"
    cp "$HEADER_SRC" "$HEADER_DST"
    echo -e "  ${GREEN}✓${NC} ggml-vulkan.h"
else
    echo -e "  ${YELLOW}⚠${NC} ggml-vulkan.h 未找到"
fi

# 显示结果
echo -e "\n${GREEN}=== 复制完成 ===${NC}"
echo -e "成功: ${GREEN}$COPIED${NC} 个文件"
if [ $FAILED -gt 0 ]; then
    echo -e "失败: ${RED}$FAILED${NC} 个文件"
fi

# 显示文件列表
echo -e "\n${GREEN}目标目录内容:${NC}"
ls -lh "$TARGET_DIR"

# 显示文件大小
echo -e "\n${GREEN}总大小:${NC}"
du -sh "$TARGET_DIR"

# 验证依赖关系
echo -e "\n${GREEN}验证库依赖关系...${NC}"
if command -v readelf &> /dev/null; then
    echo -e "${YELLOW}libggml-vulkan.so 依赖:${NC}"
    readelf -d "$TARGET_DIR/libggml-vulkan.so" | grep NEEDED | head -5

    echo -e "\n${YELLOW}libllama.so 依赖:${NC}"
    readelf -d "$TARGET_DIR/libllama.so" | grep NEEDED | head -5
else
    echo -e "${YELLOW}readelf 未安装，跳过依赖检查${NC}"
fi

echo -e "\n${GREEN}下一步:${NC}"
echo -e "1. 修改 CMakeLists.txt 添加 Vulkan 库导入"
echo -e "2. 运行: cd GGUFChat && ./gradlew :llama-android:assembleRelease"
echo -e "3. 查看详细指南: cat VULKAN_ANDROID_GUIDE.md"
