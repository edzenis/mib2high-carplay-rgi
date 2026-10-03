#!/usr/bin/env bash
set -euo pipefail

FFMPEG_VERSION="${FFMPEG_VERSION:-6.1.5}"
FFMPEG_SHA256="${FFMPEG_SHA256:-b8c8e926b948c14df1264cd0beac1c773df9170ac9cac97bdf1275cd3d385902}"

SRC="${1:-native/direct-ts-remux/direct_ts_remux.c}"
OUT="${2:-build/run8/direct-ts-remux}"
WORK="${WORK:-build/ffmpeg-remux}"
JOBS="${JOBS:-2}"

CC=arm-unknown-nto-qnx6.5.0eabi-gcc
AR=arm-unknown-nto-qnx6.5.0eabi-ar

if [ ! -e /usr/qnx650 ]; then
    ln -s /opt/qnx650 /usr/qnx650
fi

echo "===== TOOLCHAIN ====="
"$CC" --version | head -1

echo "===== QNX LINK SMOKE ====="
printf '#include <stddef.h>\nint main(void){return 0;}\n' | "$CC" \
    -include stddef.h \
    -D_QNX_SOURCE \
    -O2 \
    -march=armv7-a \
    -mfloat-abi=softfp \
    -mfpu=vfpv3-d16 \
    -x c - \
    -lsocket \
    -o /tmp/run8-qnx-link-smoke

mkdir -p "$WORK" "$(dirname "$OUT")"

TARBALL="$WORK/ffmpeg-${FFMPEG_VERSION}.tar.gz"
FFDIR="$WORK/ffmpeg-${FFMPEG_VERSION}"

if [ ! -f "$TARBALL" ]; then
    curl -fL --retry 3 \
        "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.gz" \
        -o "$TARBALL"
fi

echo "${FFMPEG_SHA256}  $TARBALL" | sha256sum -c -

if [ ! -f "$FFDIR/configure" ]; then
    rm -rf "$FFDIR"
    mkdir -p "$FFDIR"
    tar xf "$TARBALL" -C "$FFDIR" --strip-components=1
fi

if [ ! -f "$FFDIR/libavformat/libavformat.a" ]; then
    (
        cd "$FFDIR"

        ./configure \
            --cc="$CC" \
            --ar="$AR" \
            --ld="$CC" \
            --arch=arm \
            --target-os=qnx \
            --disable-asm \
            --disable-debug \
            --enable-cross-compile \
            --extra-cflags='-include stddef.h -D_QNX_SOURCE -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16' \
            --extra-libs='-lsocket' \
            --disable-doc \
            --disable-programs \
            --disable-avdevice \
            --disable-swresample \
            --disable-swscale \
            --disable-postproc \
            --disable-avfilter \
            --disable-everything \
            --enable-demuxer=h264 \
            --enable-muxer=mpegts \
            --enable-protocol=file \
            --enable-protocol=tcp \
            --enable-parser=h264

        make -j"$JOBS"
    )
fi

echo "===== BUILD RUN-8 REMUX ====="

"$CC" \
    -include stddef.h \
    -D_QNX_SOURCE \
    -O2 \
    -g \
    -std=gnu99 \
    -march=armv7-a \
    -mfloat-abi=softfp \
    -mfpu=vfpv3-d16 \
    -I"$FFDIR" \
    "$SRC" \
    -Wl,--start-group \
    "$FFDIR/libavformat/libavformat.a" \
    "$FFDIR/libavcodec/libavcodec.a" \
    "$FFDIR/libavutil/libavutil.a" \
    -Wl,--end-group \
    -lbz2 \
    -lz \
    -lsocket \
    -lm \
    -lc \
    -o "$OUT"

test -s "$OUT"

echo "===== RESULT ====="
ls -l "$OUT"
sha256sum "$OUT"