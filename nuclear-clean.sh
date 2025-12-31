#!/bin/bash
# Nuclear Clean Script - 彻底清除所有 CMake 和 Gradle 缓存
# Run this in E:\MyGithub\llama.cpp (Git Bash)

echo "========================================"
echo "Nuclear Clean - 彻底清除所有缓存"
echo "========================================"
echo ""

cd "$(dirname "$0")"

echo "[1/6] 清除 Android 项目缓存..."
cd examples/llama.android
if [ -d "lib/.cxx" ]; then
    echo "    删除 lib/.cxx"
    rm -rf lib/.cxx
fi
if [ -d "lib/build" ]; then
    echo "    删除 lib/build"
    rm -rf lib/build
fi
if [ -d "app/.cxx" ]; then
    echo "    删除 app/.cxx"
    rm -rf app/.cxx
fi
if [ -d "app/build" ]; then
    echo "    删除 app/build"
    rm -rf app/build
fi
if [ -d ".gradle" ]; then
    echo "    删除 .gradle"
    rm -rf .gradle
fi
if [ -d "build" ]; then
    echo "    删除 build"
    rm -rf build
fi

echo "[2/6] 清除全局 Gradle 缓存..."
if [ -d "$HOME/.gradle/caches" ]; then
    echo "    删除 $HOME/.gradle/caches"
    rm -rf "$HOME/.gradle/caches"
fi

echo "[3/6] 清除主项目构建缓存..."
cd ../..
if [ -d "build" ]; then
    echo "    删除 build"
    rm -rf build
fi
if [ -f "CMakeCache.txt" ]; then
    echo "    删除 CMakeCache.txt"
    rm -f CMakeCache.txt
fi
if [ -d "CMakeFiles" ]; then
    echo "    删除 CMakeFiles"
    rm -rf CMakeFiles
fi

echo "[4/6] 清除 ggml 构建缓存..."
cd ggml/src
if [ -d "build" ]; then
    echo "    删除 ggml/src/build"
    rm -rf build
fi
if [ -f "CMakeCache.txt" ]; then
    echo "    删除 ggml/src/CMakeCache.txt"
    rm -f CMakeCache.txt
fi
if [ -d "CMakeFiles" ]; then
    echo "    删除 ggml/src/CMakeFiles"
    rm -rf CMakeFiles
fi
cd ../..

echo "[5/6] 清除 ggml-vulkan 构建缓存..."
cd ggml/src/ggml-vulkan
if [ -d "build" ]; then
    echo "    删除 ggml/src/ggml-vulkan/build"
    rm -rf build
fi
if [ -f "CMakeCache.txt" ]; then
    echo "    删除 ggml/src/ggml-vulkan/CMakeCache.txt"
    rm -f CMakeCache.txt
fi
if [ -d "CMakeFiles" ]; then
    echo "    删除 ggml/src/ggml-vulkan/CMakeFiles"
    rm -rf CMakeFiles
fi
cd ../../..

echo "[6/6] 清除所有生成的 host-toolchain.cmake 文件..."
find . -name "host-toolchain.cmake" -type f -exec echo "    删除 {}" \; -exec rm -f {} \;

echo ""
echo "========================================"
echo "清理完成！"
echo "========================================"
echo ""
echo "下一步操作："
echo "1. 关闭 Android Studio"
echo "2. 重新打开 Android Studio"
echo "3. 运行: ./gradlew :lib:assembleRelease"
echo ""
