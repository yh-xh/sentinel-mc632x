#!/usr/bin/env bash
#
# 把本仓库的 MC632X 覆盖层安装到富瀚 SDK 根目录。
#
# 用法:
#   ./apply_overlay.sh /path/to/FH_RT_V3.4.0_20250123
#
# 说明:
#   - 目标目录下必须存在 rt-thread/；
#   - 已存在的文件会先备份为 *.pre_sentinel；
#   - 只新增/覆盖 overlay 中列出的文件，不删除任何东西。
#
set -euo pipefail

SDK="${1:-}"
if [ -z "$SDK" ] || [ ! -d "$SDK/rt-thread" ]; then
    echo "用法: $0 /path/to/FH_RT_V3.4.0_20250123 （目录下需包含 rt-thread/）" >&2
    exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/overlay"

if [ ! -d "$SRC" ]; then
    echo "找不到 overlay 目录: $SRC" >&2
    exit 1
fi

added=0
updated=0
backed=0

while IFS= read -r f; do
    rel="${f#"$SRC"/}"
    dest="$SDK/$rel"

    mkdir -p "$(dirname "$dest")"

    if [ -f "$dest" ]; then
        cp -p "$dest" "$dest.pre_sentinel"
        backed=$((backed + 1))
        updated=$((updated + 1))
    else
        added=$((added + 1))
    fi

    cp -p "$f" "$dest"
done < <(find "$SRC" -type f)

echo "覆盖完成：新增 $added 个，覆盖 $updated 个（已备份 $backed 个 *.pre_sentinel）"
echo
echo "提示：plat_mmc.c / board.c / lwipopts.h / ffconf.h 属于 base OS 库，"
echo "      编译时请先 make lib，再 make app。"
