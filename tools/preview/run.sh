#!/usr/bin/env bash
# Builds and runs the host preview, writing PNGs to tools/preview/out/.
# Needs a C/C++ compiler and zlib, plus LVGL in managed_components/ (run
# `pio run` once first). Run from the repo root.
set -euo pipefail

LVGL="managed_components/lvgl__lvgl"
BUILD="tools/preview/build"
OUT="tools/preview/out"
[ -d "$LVGL" ] || { echo "LVGL not found; run 'pio run' first" >&2; exit 1; }
mkdir -p "$BUILD/lvgl" "$OUT"

CFLAGS=(-O2 -DLV_CONF_INCLUDE_SIMPLE -Itools/preview -I"$LVGL" -Isrc -Icomponents/fonts -Icomponents/mascot -w)

# LVGL is compiled once and reused.
if [ ! -f "$BUILD/liblvgl.a" ]; then
  echo "Building LVGL (first run only)..."
  export BUILD CFLAGS_STR="${CFLAGS[*]}"
  find "$LVGL/src" -name '*.c' -print0 | xargs -0 -n 1 -P 8 sh -c \
    'cc $CFLAGS_STR -c "$1" -o "$BUILD/lvgl/$(echo "$1" | tr / _).o"' _
  ar rcs "$BUILD/liblvgl.a" "$BUILD"/lvgl/*.o
fi

rm -f "$BUILD"/art_*.o
for f in components/fonts/*.c components/mascot/*.c; do
  cc "${CFLAGS[@]}" -c "$f" -o "$BUILD/art_$(basename "$f" .c).o"
done
c++ -std=c++17 "${CFLAGS[@]}" tools/preview/preview.cpp src/ui/*.cpp \
  "$BUILD"/art_*.o "$BUILD/liblvgl.a" -lz -o "$BUILD/preview"
"$BUILD/preview" "$OUT"
