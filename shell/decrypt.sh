#!/bin/sh

# 定义目录路径
SRC_DIR="/data/local/tmp/plugin/meta/vpk"
ENC_DIR="/data/local/tmp/plugin/meta/vpk-enc"
# PLUS_DIR="/data/local/tmp/plugin/meta/vpk-plus"

# 确保目标目录存在
# mkdir -p "$ENC_DIR" "$PLUS_DIR"

# 遍历源目录中的 tar.gz 文件
for TAR_FILE in "$SRC_DIR"/*.tar.gz; do
    # 获取文件名（不包含路径）
    FILENAME=$(basename "$TAR_FILE")

    # 使用 = 分隔文件名字段
    IFS='=' read -r MANUFACTURER MODEL DEVICE VERSION BUILD ENCFLAG FIELD_HASH <<< "${FILENAME%.tar.gz}"

    # 检查第六个字段是否为 N
    if [[ "$ENCFLAG" != "N" ]]; then
        echo "Skipping $FILENAME as the sixth field is not 'N'."
        continue
    fi

    # 修改目录名，将第六个字段 N 替换为 E
    NEW_ENCFLAG="E"
    
    OLD_DIR="${MANUFACTURER}=${MODEL}=${DEVICE}=${VERSION}=${BUILD}=${ENCFLAG}=${FIELD_HASH}"
    NEW_DIR="${MANUFACTURER}=${MODEL}=${DEVICE}=${VERSION}=${BUILD}=${NEW_ENCFLAG}=${FIELD_HASH}"
    NEW_FILENAME="${MANUFACTURER}=${MODEL}=${DEVICE}=${VERSION}=${BUILD}=${NEW_ENCFLAG}=${FIELD_HASH}.tar.gz"
    

    # 解压 tar.gz 包
    TMP_DIR=$(mktemp -d)
    tar -xzf "$TAR_FILE" -C "$TMP_DIR"

    mv $TMP_DIR/$OLD_DIR $TMP_DIR/$NEW_DIR
    # 重新打包为新的 tar.gz
    NEW_TAR_FILE="$SRC_DIR/$NEW_FILENAME"
    tar -czf "$NEW_TAR_FILE" -C "$TMP_DIR" .

    # 删除临时目录
    rm -rf "$TMP_DIR"

    # 使用 vpick 加密新的 tar.gz 包
    ENC_FILE="$ENC_DIR/${NEW_FILENAME%.tar.gz}.tar.gz"
    vpick encrypt --input "$NEW_TAR_FILE" --output "$ENC_FILE"

    # 拷贝一份原 tar.gz 包到 vpk-plus 目录
    # PLUS_FILE="$PLUS_DIR/$FILENAME"
    # cp "$ENC_FILE" "$PLUS_FILE"

done

echo "Processing complete."
