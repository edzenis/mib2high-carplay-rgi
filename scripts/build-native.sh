#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SRC="$ROOT/native"
OUT="$ROOT/build/libmib2high-carplay-rgi.so"

: "${QNX_HOST:?Set QNX_HOST to the host directory of your QNX SDP 6.5 installation}"
: "${QNX_TARGET:?Set QNX_TARGET to the target directory of your QNX SDP 6.5 installation}"
export QNX_HOST QNX_TARGET
PATH="$QNX_HOST/usr/bin:$PATH"
export PATH

command -v qcc >/dev/null 2>&1 || {
    echo "ERROR: qcc not found. Check QNX_HOST and your QNX SDP 6.5 environment." >&2
    exit 1
}

mkdir -p "$ROOT/build"

qcc -Vgcc_ntoarmv7le \
    -shared -fPIC -O2 -Wc,-std=gnu99 \
    -I"$SRC" \
    -I"$SRC/framework" \
    -I"$SRC/routeguidance" \
    "$SRC/observer.c" \
    "$SRC/altscreen_k2161.c" \
    "$SRC/altscreen_airplay.c" \
    "$SRC/ident_hook.c" \
    "$SRC/message_table.c" \
    "$SRC/send_hook.c" \
    "$SRC/receive_probe.c" \
    "$SRC/status.c" \
    "$SRC/rgi_bridge.c" \
    "$SRC/diagnostics.c" \
    "$SRC/framework/logging.c" \
    "$SRC/framework/bus.c" \
    "$SRC/framework/iap2_protocol.c" \
    "$SRC/routeguidance/rgd_tlv.c" \
    "$SRC/routeguidance/rgd_hook.c" \
    -lsocket \
    -o "$OUT"

echo "OUTPUT=$OUT"
sha256sum "$OUT"
