#!/bin/sh
set -eu

QCC="${QCC:-qcc}"
SRC="${SRC:-native/vc_render-k2161/vc_render.c}"
OUT="${1:-build/vc_render-k2161}"

mkdir -p "$(dirname "$OUT")"

"$QCC" \
  -Vgcc_ntoarmv7le -mfloat-abi=softfp \
  -O2 -g -Wc,-std=gnu99 -Wall -Wextra \
  -o "$OUT" "$SRC" -lsocket

echo "Built $OUT"
