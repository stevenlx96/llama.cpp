#!/bin/bash
set -e

echo "=========================================="
echo "Hexagon NPU Backend 编译脚本"
echo "=========================================="
echo ""

# 颜色定义
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

WORKSPACE="/workspace"
BUILD_DIR="build-hexagon"
INSTALL_DIR="pkg-hexagon"

echo -e "${BLUE}[1/5]${NC} 检查 Docker 环境..."
if ! command -v docker &> /dev/null; then
    echo -e "${YELLOW}错误: 未找到 Docker 命令${NC}"
    echo "请先安装 Docker: https://docs.docker.com/get-docker/"
    exit 1
fi
echo -e "${GREEN}✓${NC} Docker 已安装"

echo ""
echo -e "${BLUE}[2/5]${NC} 拉取 Snapdragon 工具链 Docker 镜像..."
echo "镜像: ghcr.io/snapdragon-toolchain/arm64-android:v0.3"
docker pull ghcr.io/snapdragon-toolchain/arm64-android:v0.3

echo ""
echo -e "${BLUE}[3/5]${NC} 复制 CMake 预设文件..."
if [ ! -f "CMakeUserPresets.json" ]; then
    cp docs/backend/hexagon/CMakeUserPresets.json .
    echo -e "${GREEN}✓${NC} 已复制 CMakeUserPresets.json"
else
    echo -e "${YELLOW}⚠${NC}  CMakeUserPresets.json 已存在，跳过"
fi

echo ""
echo -e "${BLUE}[4/5]${NC} 在 Docker 容器中编译 Hexagon NPU 后端..."
echo "这可能需要 10-20 分钟，请耐心等待..."
echo ""

# 运行 Docker 编译
docker run --rm \
    --volume "$(pwd):${WORKSPACE}" \
    --platform linux/amd64 \
    ghcr.io/snapdragon-toolchain/arm64-android:v0.3 \
    bash -c "
        set -e
        cd ${WORKSPACE}

        echo '----------------------------------------'
        echo '配置 CMake 项目...'
        echo '----------------------------------------'
        cmake --preset arm64-android-snapdragon-release -B ${BUILD_DIR}

        echo ''
        echo '----------------------------------------'
        echo '编译项目...'
        echo '----------------------------------------'
        cmake --build ${BUILD_DIR} -j\$(nproc)

        echo ''
        echo '----------------------------------------'
        echo '安装到 ${INSTALL_DIR}...'
        echo '----------------------------------------'
        cmake --install ${BUILD_DIR} --prefix ${INSTALL_DIR}

        echo ''
        echo '✓ 编译完成！'
    "

echo ""
echo -e "${BLUE}[5/5]${NC} 验证编译产物..."
echo ""

# 检查关键库文件
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

echo "检查库文件:"
for lib in "${LIBS[@]}"; do
    if [ -f "${INSTALL_DIR}/lib/${lib}" ]; then
        SIZE=$(du -h "${INSTALL_DIR}/lib/${lib}" | cut -f1)
        echo -e "  ${GREEN}✓${NC} ${lib} (${SIZE})"
    else
        echo -e "  ${YELLOW}✗${NC} ${lib} - 未找到"
    fi
done

echo ""
echo "=========================================="
echo -e "${GREEN}✓ Hexagon NPU 后端编译成功！${NC}"
echo "=========================================="
echo ""
echo "编译产物位置: ${INSTALL_DIR}/lib/"
echo ""
echo "包含的 HTP 版本:"
echo "  - v73: Snapdragon 888, 8 Gen 1"
echo "  - v75: Snapdragon 8 Gen 2"
echo "  - v79: Snapdragon 8 Gen 3"
echo "  - v81: Snapdragon 8 Elite (三星 S25)"
echo ""
echo "下一步: 运行 ./deploy-hexagon-to-ggufchat.sh 部署到 GGUFChat"
echo ""
