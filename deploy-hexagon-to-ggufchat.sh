#!/bin/bash
set -e

echo "=========================================="
echo "Hexagon NPU 库部署脚本 → GGUFChat"
echo "=========================================="
echo ""

# 颜色定义
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

SOURCE_DIR="pkg-hexagon/lib"
TARGET_DIR="GGUFChat/llama-android/src/main/jniLibs/arm64-v8a"

# 检查源目录
if [ ! -d "${SOURCE_DIR}" ]; then
    echo -e "${RED}错误: 源目录不存在: ${SOURCE_DIR}${NC}"
    echo "请先运行 ./build-hexagon-npu.sh 编译 Hexagon 后端"
    exit 1
fi

# 检查目标目录
if [ ! -d "${TARGET_DIR}" ]; then
    echo -e "${YELLOW}警告: 目标目录不存在: ${TARGET_DIR}${NC}"
    echo "创建目录..."
    mkdir -p "${TARGET_DIR}"
fi

echo -e "${BLUE}源目录:${NC} ${SOURCE_DIR}"
echo -e "${BLUE}目标目录:${NC} ${TARGET_DIR}"
echo ""

# 要复制的库文件列表
LIBS=(
    "libggml-base.so"
    "libggml-cpu.so"
    "libggml-hexagon.so"
    "libggml-htp-v73.so"
    "libggml-htp-v75.so"
    "libggml-htp-v79.so"
    "libggml-htp-v81.so"
    "libggml.so"
    "libllama.so"
)

echo "复制库文件到 GGUFChat..."
echo ""

COPIED=0
SKIPPED=0

for lib in "${LIBS[@]}"; do
    SOURCE="${SOURCE_DIR}/${lib}"
    TARGET="${TARGET_DIR}/${lib}"

    if [ -f "${SOURCE}" ]; then
        cp "${SOURCE}" "${TARGET}"
        SIZE=$(du -h "${TARGET}" | cut -f1)
        echo -e "  ${GREEN}✓${NC} ${lib} (${SIZE})"
        COPIED=$((COPIED + 1))
    else
        echo -e "  ${YELLOW}✗${NC} ${lib} - 源文件不存在，跳过"
        SKIPPED=$((SKIPPED + 1))
    fi
done

echo ""
echo "=========================================="
echo -e "${GREEN}✓ 部署完成！${NC}"
echo "=========================================="
echo ""
echo "统计:"
echo "  - 复制成功: ${COPIED} 个文件"
echo "  - 跳过: ${SKIPPED} 个文件"
echo ""
echo "目标位置: ${TARGET_DIR}/"
echo ""

# 显示目标目录内容
echo "目标目录内容:"
ls -lh "${TARGET_DIR}"/*.so 2>/dev/null || echo "  (空)"

echo ""
echo "下一步:"
echo "  1. 在 Android Studio 中重新构建 GGUFChat 项目"
echo "  2. 安装到设备并测试 NPU 性能"
echo ""
