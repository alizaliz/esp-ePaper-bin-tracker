#!/usr/bin/env bash
# Regenerates the LVGL fonts in components/fonts/ from their source TTF.
# Requires Node.js (for npx lv_font_conv) and curl. Run from the repo root.
set -euo pipefail

CACHE="tools/fonts/.cache"
OUT="components/fonts"
mkdir -p "$CACHE" "$OUT"

# Gochi Hand, a marker-pen handwriting font (SIL Open Font License 1.1)
HAND="$CACHE/GochiHand-Regular.ttf"
[ -f "$HAND" ] || curl -sSfL -o "$HAND" \
  https://github.com/google/fonts/raw/main/ofl/gochihand/GochiHand-Regular.ttf

conv() {
  npx --yes lv_font_conv@1.5.3 --no-compress --format lvgl --bpp 4 --lv-include lvgl.h "$@"
}

# Speech bubble day ("THU 8"), and the indoor temperature when there's no forecast
conv --font "$HAND" --size 42 --symbols "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -°" \
  --lv-font-name font_hand_42 -o "$OUT/font_hand_42.c"

# "TONIGHT" and "TODAY"
conv --font "$HAND" --size 38 --symbols "ABCDEFGHIJKLMNOPQRSTUVWXYZ " \
  --lv-font-name font_hand_38 -o "$OUT/font_hand_38.c"

# Forecast temperature over the weather icon
conv --font "$HAND" --size 24 --symbols "0123456789-°" \
  --lv-font-name font_hand_24 -o "$OUT/font_hand_24.c"

# Small text ("Bins out", "Last known", "inside")
conv --font "$HAND" --size 20 --symbols "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 !'.,:-°" \
  --lv-font-name font_hand_20 -o "$OUT/font_hand_20.c"

echo "Fonts written to $OUT/"
