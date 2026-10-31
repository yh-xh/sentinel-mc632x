#!/bin/bash
# 构建后自动把固件同步到 tftp 根目录，省掉手动 sudo cp。
# 没有写权限时只提示，不让构建失败。
BIN="$(pwd)/app/sentinel/out/bin/sentinel.bin"
if [ -f "$BIN" ]; then
    if cp -f "$BIN" /srv/tftp/ 2>/dev/null; then
        echo "[post-build] sentinel.bin -> /srv/tftp/"
    else
        echo "[post-build] 无权限写 /srv/tftp，请手动执行: sudo cp \"$BIN\" /srv/tftp/"
    fi
fi
