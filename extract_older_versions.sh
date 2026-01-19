#!/bin/bash

# 提取更早的 15 个 JNI CPP 版本（版本 16-30）
# 从 b625631 之前开始倒序提取

FILE_PATH="GGUFChat/app/src/main/cpp/llama-android-jni.cpp"
OUTPUT_DIR="jni_history_versions_16_30"

# 定义要提取的 commit（从新到旧）
declare -a COMMITS=(
    "df8af3d:version_16:Remove load_tensors log filter"
    "4e0546c:version_17:Fix compilation error headers"
    "3a1f779:version_18:Add tensor allocation debug"
    "3af70fe:version_19:Add OpenCL diagnostics"
    "523823e:version_20:Add OpenCL backend support"
    "ee8d150:version_21:Add tensor allocation debug"
    "2dc17d1:version_22:Fix duplicate Hexagon registration"
    "cd830ba:version_23:Add backend registration tracking"
    "f1f44c9:version_24:Remove duplicate registration"
    "eee8487:version_25:Remove duplicate Hexagon registration"
    "9bb7ca4:version_26:Add NULL terminator to device array"
    "4e02129:version_27:Fix compilation error llama_get_buf"
    "066268c:version_28:Add backend synchronization"
    "9cfa7ec:version_29:Update jni so"
    "62b561d:version_30:Test Hexagon device usability"
)

echo "开始提取 JNI CPP 历史版本（版本 16-30）..."
echo ""

for entry in "${COMMITS[@]}"; do
    IFS=':' read -r commit version desc <<< "$entry"

    echo "[$version] 提取 $commit - $desc"

    # 检查文件是否存在于该 commit
    if git cat-file -e "$commit:$FILE_PATH" 2>/dev/null; then
        git show "$commit:$FILE_PATH" > "$OUTPUT_DIR/${version}_${commit:0:7}.cpp"
        echo "  ✅ 成功: $OUTPUT_DIR/${version}_${commit:0:7}.cpp"
    else
        echo "  ⚠️  文件不存在于 commit $commit"
    fi
    echo ""
done

echo "========================================="
echo "✅ 完成！提取了 ${#COMMITS[@]} 个历史版本"
echo "输出目录: $OUTPUT_DIR"
