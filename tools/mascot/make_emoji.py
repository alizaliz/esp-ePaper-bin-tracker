#!/usr/bin/env python3
"""Generates the emoji mascot images in components/mascot/.

Draws each expression as an SVG, rasterises it with macOS Quick Look and sips,
scales it down, dithers it to black and white (Floyd-Steinberg), and writes
LVGL 1-bit (I1) image descriptors. Pre-dithering makes the shading look the
same whatever the display's dithering setting. Run from the repo root on macOS:
    python3 tools/mascot/make_emoji.py
"""
import math
import os
import struct
import subprocess
import tempfile

SIZE = 84  # pixels square
OUT = "components/mascot"
FACES = ["happy", "wink", "excited", "proud", "worried", "sleepy"]


def svg(body, defs=""):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="440" height="440" viewBox="0 0 220 220">'
            f'<defs>{defs}</defs><rect width="220" height="220" fill="#fff"/>{body}</svg>')


def sparkle(x, y, r=8, fill="#000"):
    return f'<path d="M{x} {y-r} Q{x} {y} {x+r} {y} Q{x} {y} {x} {y+r} Q{x} {y} {x-r} {y} Q{x} {y} {x} {y-r}Z" fill="{fill}"/>'


def zzz(x, y):
    return (f'<text x="{x}" y="{y}" font-family="Arial Black,Arial" font-weight="900" font-size="26" fill="#000">Z</text>'
            f'<text x="{x+22}" y="{y-16}" font-family="Arial Black,Arial" font-weight="900" font-size="18" fill="#000">z</text>')


def sweat(x, y, s=1.0):
    return (f'<path transform="translate({x} {y}) scale({s})" d="M0 -16 C6 -6 10 0 10 6 A10 10 0 0 1 -10 6 C-10 0 -6 -6 0 -16Z" '
            f'fill="#bbb" stroke="#000" stroke-width="3"/>')


def star(x, y, r):
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        rr = r if i % 2 == 0 else r * 0.45
        pts.append(f"{x + rr * math.cos(a):.1f},{y + rr * math.sin(a):.1f}")
    return f'<polygon points="{" ".join(pts)}" fill="#000"/>'


def emoji(face):
    defs = ('<radialGradient id="shade" cx="0.4" cy="0.32" r="0.75">'
            '<stop offset="0" stop-color="#ffffff"/><stop offset="0.6" stop-color="#d8d8d8"/>'
            '<stop offset="1" stop-color="#8a8a8a"/></radialGradient>')
    body = '<circle cx="110" cy="114" r="88" fill="url(#shade)" stroke="#000" stroke-width="5"/>'
    eyes = ('<ellipse cx="80" cy="96" rx="10" ry="15" fill="#000"/><ellipse cx="140" cy="96" rx="10" ry="15" fill="#000"/>'
            '<circle cx="83" cy="90" r="3.5" fill="#fff"/><circle cx="143" cy="90" r="3.5" fill="#fff"/>')
    grin = ('<path d="M76 126 Q110 180 144 126 Z" fill="#000"/>'
            '<path d="M82 130 L138 130 Q136 139 131 143 L89 143 Q84 139 82 130 Z" fill="#fff"/>')
    blush = '<ellipse cx="54" cy="134" rx="12" ry="7" fill="#aaa"/><ellipse cx="166" cy="134" rx="12" ry="7" fill="#aaa"/>'
    if face == "happy":
        body += eyes + grin + blush
    elif face == "wink":
        body += ('<ellipse cx="80" cy="96" rx="10" ry="15" fill="#000"/><circle cx="83" cy="90" r="3.5" fill="#fff"/>'
                 '<path d="M126 98 Q140 84 154 98" fill="none" stroke="#000" stroke-width="6" stroke-linecap="round"/>'
                 '<path d="M76 136 Q110 162 144 136" fill="none" stroke="#000" stroke-width="6" stroke-linecap="round"/>'
                 '<path d="M114 148 Q120 176 136 168 Q140 152 134 144 Z" fill="#888" stroke="#000" stroke-width="4"/>' + blush)
    elif face == "excited":
        body += (star(80, 94, 22) + star(140, 94, 22)
                 + '<path d="M70 130 Q110 196 150 130 Z" fill="#000"/><path d="M92 160 Q110 182 128 160 Z" fill="#888"/>'
                 + sparkle(26, 40) + sparkle(196, 36) + sparkle(200, 184, 7))
    elif face == "proud":  # cool shades and a smirk
        body += ('<path d="M52 82 L168 82 L166 92 Q160 116 136 116 Q118 116 114 96 L106 96 Q102 116 84 116 Q60 116 54 92 Z" fill="#000"/>'
                 '<path d="M64 88 L80 88 L72 100 Z" fill="#666"/><path d="M126 88 L142 88 L134 100 Z" fill="#666"/>'
                 '<path d="M82 144 Q118 158 146 132" fill="none" stroke="#000" stroke-width="6" stroke-linecap="round"/>'
                 + sparkle(198, 60))
    elif face == "worried":
        body += (eyes + '<path d="M62 70 L94 80" stroke="#000" stroke-width="5" stroke-linecap="round"/>'
                 '<path d="M158 70 L126 80" stroke="#000" stroke-width="5" stroke-linecap="round"/>'
                 '<path d="M82 154 Q110 132 138 154" fill="none" stroke="#000" stroke-width="6" stroke-linecap="round"/>'
                 + sweat(176, 58, 1.6))
    elif face == "sleepy":
        body += ('<path d="M66 98 Q80 110 94 98" fill="none" stroke="#000" stroke-width="6" stroke-linecap="round"/>'
                 '<path d="M126 98 Q140 110 154 98" fill="none" stroke="#000" stroke-width="6" stroke-linecap="round"/>'
                 '<ellipse cx="110" cy="146" rx="9" ry="11" fill="#000"/>' + zzz(160, 40))
    return svg(body, defs)


def rasterise(svg_text, tmp):
    """SVG -> 440px greyscale rows, via Quick Look and a BMP from sips."""
    src = os.path.join(tmp, "art.svg")
    open(src, "w").write(svg_text)
    subprocess.run(["qlmanage", "-t", "-s", "440", "-o", tmp, src], capture_output=True, check=True)
    bmp = os.path.join(tmp, "art.bmp")
    subprocess.run(["sips", "-s", "format", "bmp", src + ".png", "--out", bmp], capture_output=True, check=True)
    d = open(bmp, "rb").read()
    off = struct.unpack_from("<I", d, 10)[0]
    w, h = struct.unpack_from("<ii", d, 18)
    bpp = struct.unpack_from("<H", d, 28)[0] // 8
    stride = (w * bpp + 3) & ~3
    rows = []
    for y in range(abs(h)):
        base = off + (abs(h) - 1 - y if h > 0 else y) * stride
        rows.append([0.299 * d[base + x * bpp + 2] + 0.587 * d[base + x * bpp + 1] + 0.114 * d[base + x * bpp] for x in range(w)])
    return rows


def scale_down(rows, size):
    h, w = len(rows), len(rows[0])
    out = []
    for y in range(size):
        y0, y1 = y * h // size, max(y * h // size + 1, (y + 1) * h // size)
        row = []
        for x in range(size):
            x0, x1 = x * w // size, max(x * w // size + 1, (x + 1) * w // size)
            row.append(sum(rows[yy][xx] for yy in range(y0, y1) for xx in range(x0, x1)) / ((y1 - y0) * (x1 - x0)))
        out.append(row)
    return out


def dither(grey):
    """Floyd-Steinberg; returns rows of booleans, True = black."""
    g = [row[:] for row in grey]
    n = len(g)
    out = [[False] * n for _ in range(n)]
    for y in range(n):
        for x in range(n):
            black = g[y][x] < 128
            out[y][x] = black
            e = g[y][x] - (0 if black else 255)
            if x + 1 < n: g[y][x + 1] += e * 7 / 16
            if y + 1 < n:
                if x > 0: g[y + 1][x - 1] += e * 3 / 16
                g[y + 1][x] += e * 5 / 16
                if x + 1 < n: g[y + 1][x + 1] += e / 16
    return out


def main():
    stride = (SIZE + 7) // 8
    lines = ["// Generated by tools/mascot/make_emoji.py; do not edit.", '#include "mascot.h"', ""]
    with tempfile.TemporaryDirectory() as tmp:
        for face in FACES:
            bits = dither(scale_down(rasterise(emoji(face), tmp), SIZE))
            # I1: a 2-colour ARGB8888 palette (index 0 white, 1 black), then
            # rows of 1-bit indices, most significant bit first.
            data = [0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF]
            for row in bits:
                for b in range(stride):
                    byte = 0
                    for bit in range(8):
                        x = b * 8 + bit
                        if x < SIZE and row[x]:
                            byte |= 0x80 >> bit
                    data.append(byte)
            hexes = ", ".join(f"0x{v:02x}" for v in data)
            lines += [
                f"static const uint8_t emoji_{face}_data[] = {{{hexes}}};",
                f"const lv_image_dsc_t mascot_emoji_{face} = {{",
                f"    .header = {{.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_I1, .flags = 0,",
                f"               .w = {SIZE}, .h = {SIZE}, .stride = {stride}}},",
                f"    .data_size = sizeof(emoji_{face}_data),",
                f"    .data = emoji_{face}_data,",
                "};",
                "",
            ]
    open(os.path.join(OUT, "mascot_emoji.c"), "w").write("\n".join(lines))
    print(f"wrote {len(FACES)} faces at {SIZE}px to {OUT}/mascot_emoji.c")


if __name__ == "__main__":
    main()
