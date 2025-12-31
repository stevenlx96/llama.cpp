#!/bin/bash
# 编译启用Vulkan的llama.cpp Android库

# Android NDK路径（请修改为你的NDK路径）
export ANDROID_NDK="$HOME/Android/Sdk/ndk/29.0.13113456"

# 目标架构
ABI="arm64-v8a"  # 或 armeabi-v7a

# 编译目录
BUILD_DIR="build-android-vulkan-${ABI}"

# 清理之前的编译
rm -rf $BUILD_DIR

# 配置CMake
cmake -B $BUILD_DIR \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=$ABI \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DGGML_VULKAN=ON \
  -DGGML_BACKEND_DL=ON \
  -DGGML_CPU_ALL_VARIANTS=ON \
  -DGGML_NATIVE=OFF \
  -DGGML_OPENMP=ON \
  -DGGML_LLAMAFILE=OFF \
  -DLLAMA_BUILD_COMMON=ON \
  -DLLAMA_CURL=OFF

# 编译
cmake --build $BUILD_DIR --config Release -j$(nproc)

echo "编译完成！生成的库文件在: $BUILD_DIR"
echo "需要的so文件："
find $BUILD_DIR -name "*.so" -type f | grep -E "(libggml|libllama)" | sort
