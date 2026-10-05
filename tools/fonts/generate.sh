#!/usr/bin/env bash
# Regenerates the LVGL fonts in components/fonts/ from their source TTFs.
# Requires Node.js (for npx lv_font_conv) and curl. Run from the repo root.
set -euo pipefail

CACHE="tools/fonts/.cache"
OUT="components/fonts"
mkdir -p "$CACHE" "$OUT"

# Montserrat Bold (SIL Open Font License 1.1)
MONTSERRAT="$CACHE/Montserrat-Bold.ttf"
[ -f "$MONTSERRAT" ] || curl -sSfL -o "$MONTSERRAT" \
  https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Bold.ttf

# Font Awesome 6 Free Solid (icons CC BY 4.0, font SIL OFL 1.1)
FA="$CACHE/fa-solid-900.ttf"
if [ ! -f "$FA" ]; then
  (cd "$CACHE" && npm pack --silent @fortawesome/fontawesome-free@6 >/dev/null \
    && tar -xzf fortawesome-fontawesome-free-*.tgz package/webfonts/fa-solid-900.ttf \
    && mv package/webfonts/fa-solid-900.ttf . && rm -rf package fortawesome-fontawesome-free-*.tgz)
fi

conv() {
  npx --yes lv_font_conv@1.5.3 --no-compress --format lvgl --bpp 4 --lv-include lvgl.h "$@"
}

# Pickup day ("THU 8") and "TODAY"
conv --font "$MONTSERRAT" --size 40 --symbols "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 " \
  --lv-font-name font_bold_40 -o "$OUT/font_bold_40.c"

# Temperature and humidity readings
conv --font "$MONTSERRAT" --size 18 --symbols "0123456789-°C% " \
  --lv-font-name font_bold_18 -o "$OUT/font_bold_18.c"

# Bin icons: trash-can, recycle, apple-whole
conv --font "$FA" --size 52 --range 0xF2ED,0xF1B8,0xF5D1 \
  --lv-font-name icons_52 -o "$OUT/icons_52.c"

# Top row icons: wifi, temperature-half, droplet
conv --font "$FA" --size 22 --range 0xF1EB,0xF2C9,0xF043 \
  --lv-font-name icons_22 -o "$OUT/icons_22.c"

# Collection marks: check, xmark
conv --font "$FA" --size 30 --range 0xF00C,0xF00D \
  --lv-font-name icons_30 -o "$OUT/icons_30.c"

echo "Fonts written to $OUT/"
