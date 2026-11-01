#!/usr/bin/env bash
# 用无界面 Edge 把各界面 HTML 截成 png，输出到 ../UI界面/
# 用法：bash render.sh [编号...]，不带参数时渲染全部
set -e
cd "$(dirname "$0")"
EDGE="/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"
OUT="../UI界面"
mkdir -p "$OUT"

# 编号 文件 宽 高 缩放
PAGES="
1 01-login.html 390 844 2
2 02-plaza.html 1440 1024 1
3 03-detail.html 1440 1024 1
4 04-tickets.html 390 844 2
5 05-messages.html 390 844 2
6 06-checkin.html 390 844 2
7 07-edit.html 1440 1024 1
8 08-manage.html 1440 1024 1
9 09-review.html 1440 1024 1
10 10-logs.html 1440 1024 1
"

echo "$PAGES" | while read -r n file w h s; do
  [ -z "$n" ] && continue
  if [ $# -gt 0 ] && ! printf ' %s ' "$@" | grep -q " $n "; then continue; fi
  [ -f "$file" ] || continue
  url="file:///$(cygpath -m "$PWD/$file")"
  "$EDGE" --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor="$s" \
    --window-size="$w,$h" --screenshot="$(cygpath -w "$PWD/$OUT/界面$n.png")" "$url" >/dev/null 2>&1
  echo "界面$n.png <- $file"
done
