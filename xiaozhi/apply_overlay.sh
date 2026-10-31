#!/usr/bin/env bash
#
# 把小智端（AtomS3R Echo Base）的板级源码与语音资源覆盖到 xiaozhi-esp32。
#
# 用法:
#   ./apply_overlay.sh /path/to/xiaozhi-esp32
#
# 说明:
#   - 覆盖 mcb_client.h / mcb_alert_sounds.h / mcb_report_sounds.h 与 OGG 资源；
#   - 用 git apply 应用 patches/ 下的两处修改；
#   - 已存在的文件先备份为 *.pre_sentinel。
#
set -euo pipefail

X="${1:-}"
if [ -z "$X" ] || [ ! -d "$X/main" ]; then
    echo "用法: $0 /path/to/xiaozhi-esp32 （目录下需包含 main/）" >&2
    exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
BOARD="main/boards/m5stack/atoms3r-echo-base"

# 1) 板级头文件
mkdir -p "$X/$BOARD"
for f in mcb_client.h mcb_alert_sounds.h mcb_report_sounds.h; do
    dest="$X/$BOARD/$f"
    if [ -f "$dest" ]; then
        cp -p "$dest" "$dest.pre_sentinel"
    fi
    cp -p "$HERE/overlay/$BOARD/$f" "$dest"
    echo "覆盖 $BOARD/$f"
done

# 2) 语音资源
mkdir -p "$X/main/assets/common"
for f in "$HERE"/assets/common/*.ogg; do
    [ -e "$f" ] || continue
    dest="$X/main/assets/common/$(basename "$f")"
    if [ -f "$dest" ]; then
        cp -p "$dest" "$dest.pre_sentinel"
    fi
    cp -p "$f" "$dest"
done
echo "覆盖语音资源 $(ls "$HERE"/assets/common/*.ogg | wc -l) 个"

# 3) 两处源码修改
cd "$X"
for p in "$HERE"/patches/*.patch; do
    [ -e "$p" ] || continue
    if git apply --check "$p" 2>/dev/null; then
        git apply "$p"
        echo "已应用 $(basename "$p")"
    else
        echo "跳过 $(basename "$p")（可能已应用，或上游代码已变化，请手工合并）"
    fi
done

echo
echo "下一步："
echo "  cd $X"
echo "  idf.py reconfigure    # 资源使用 file(GLOB)，新增 OGG 后必须执行"
echo "  idf.py build"
echo "  idf.py -p <串口> flash monitor"
