#!/usr/bin/env python3
"""Generates the pen-doodle images in components/doodles/.

Draws each piece (speech bubble, bins, weather, Binny's faces, status icons)
as SVG, roughens the lines with an SVG turbulence filter so they wobble like
pen on paper, rasterises it with macOS Quick Look and sips, scales it down,
thresholds it to black and white, and writes LVGL 1-bit (I1) images with a
transparent background. Run from the repo root on macOS:
    python3 tools/doodles/make_doodles.py

Lines are drawn twice, slightly offset, for a sketched look. Coordinates in
each drawing are final screen pixels.
"""
import math
import os
import struct
import subprocess
import tempfile
import zlib

OUT = "components/doodles"
CANVAS = 800  # every piece is rasterised at this size, then scaled down


def scale_for(w, h):
    return CANVAS / max(w, h)


def svg(w, h, body, seed, wobble, wavelength=9):
    """wobble: how far lines wander; wavelength: how quickly. Both in screen
    pixels."""
    k = scale_for(w, h)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS}" height="{CANVAS}" viewBox="0 0 {CANVAS} {CANVAS}">'
            f'<defs><filter id="r" x="-5%" y="-5%" width="110%" height="110%">'
            f'<feTurbulence type="fractalNoise" baseFrequency="{1 / (wavelength * k):.5f}" numOctaves="2" seed="{seed}"/>'
            f'<feDisplacementMap in="SourceGraphic" scale="{wobble * k:.1f}"/></filter></defs>'
            f'<rect width="{CANVAS}" height="{CANVAS}" fill="#fff"/>'
            f'<g filter="url(#r)" transform="scale({k:.4f})" fill="none" stroke="#000" '
            f'stroke-linecap="round" stroke-linejoin="round">{body}</g></svg>')


def twice(path, w=2.2, dx=0.6, dy=0.4, fill="none"):
    """A sketchy line: the same stroke drawn twice, slightly offset."""
    return (f'<path d="{path}" stroke-width="{w}" fill="{fill}"/>'
            f'<path d="{path}" stroke-width="{w * 0.6:.2f}" transform="translate({dx} {dy})" fill="none"/>')


def star(cx, cy, r):
    points = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        rr = r if i % 2 == 0 else r * 0.42
        points.append(f"{cx + rr * math.cos(a):.1f} {cy + rr * math.sin(a):.1f}")
    return "M" + " L".join(points) + " Z"


# Speech bubble with its tail, 184x108. The filled version is for TONIGHT and
# TODAY.
BUBBLE = "M18 5 L166 4 Q180 5 180 19 L181 76 Q180 89 166 89 L58 89 L44 106 L46 89 L18 89 Q4 88 4 74 L4 19 Q5 5 18 5 Z"

# Weather, 84x70. Clouds are filled white so they hide the sun behind them.
CLOUD = "M12 44 C4 44 4 32 13 31 C12 20 26 16 31 24 C35 12 54 12 56 26 C66 24 72 34 66 42 C64 46 60 46 58 46 Z"


def cloud(dx=0, dy=0, s=1.0):
    return f'<g transform="translate({dx} {dy}) scale({s})">{twice(CLOUD, 2.4, fill="#fff")}</g>'


def sun(cx, cy, r):
    out = (f'<circle cx="{cx}" cy="{cy}" r="{r}" stroke-width="2.4"/>'
           f'<circle cx="{cx + 0.6}" cy="{cy - 0.5}" r="{r - 0.6}" stroke-width="1.2"/>')
    for i in range(8):
        a = i * math.pi / 4 + 0.2
        out += (f'<path d="M{cx + (r + 4) * math.cos(a):.1f} {cy + (r + 4) * math.sin(a):.1f} '
                f'L{cx + (r + 10) * math.cos(a):.1f} {cy + (r + 10) * math.sin(a):.1f}" stroke-width="2.4"/>')
    return out


def rain_cloud():
    return cloud(6, -6, 0.95)


STATUS_GRID = '<g transform="scale(1.2222)">'  # 18 -> 22px

# Binny's face, 84px, with hatched shading at the lower right.
FACE = ('<circle cx="42" cy="42" r="36" stroke-width="2.6"/><circle cx="42.8" cy="41.5" r="35.4" stroke-width="1.2"/>'
        + "".join(f'<path d="M{50 + k * 4} {74 - k * 1.5} L{74 - k * 1.5} {50 + k * 4}" stroke-width="1.1"/>' for k in range(4)))
EYES = '<ellipse cx="31" cy="36" rx="3" ry="4.5" fill="#000"/><ellipse cx="53" cy="36" rx="3" ry="4.5" fill="#000"/>'
# The melting face has its own outline: the head sags into a puddle.
# The melting face has its own outline: the head tips over to the right and
# sags into a flat puddle.
MELTY = (twice("M11 36 A30 30 0 0 1 70 47 C70 54 69 59 71 64 C73 69 82 69 82 75 C82 80 68 80 42 80 "
               "C16 80 2 80 2 75 C2 70 11 69 12 63 C13 54 10 46 11 36 Z", 2.6)
         + twice("M18 74 Q30 72 40 75 M50 73 Q60 71 68 74", 1.2, 0.3, 0.2)  # ripples in the puddle
         + "".join(f'<path d="M{56 + k * 4} {70 - k * 0.8} L{63 + k * 2} {61 - k * 3}" stroke-width="1.1"/>' for k in range(3)))
FACES = {
    "happy": EYES + twice("M27 50 Q42 64 57 50", 2.4),
    "wink": ('<ellipse cx="31" cy="36" rx="3" ry="4.5" fill="#000"/>' + twice("M48 37 Q53 31 58 37", 2.2)
             + twice("M28 51 Q42 60 56 51", 2.2) + twice("M44 55 Q46 63 51 60 Q52 55 49 53", 1.8)),
    "grin": (twice("M24 39 Q31 30 38 39 M46 39 Q53 30 60 39", 2.4)
             + twice("M25 48 L59 48 Q57 68 42 68 Q27 68 25 48 Z", 1.8, fill="#000")),
    "blush": (EYES + twice("M29 51 Q42 61 55 51", 2.4)
              + twice("M17 53 L20 48 M21 54 L24 49 M25 55 L28 50 M56 55 L59 50 M60 54 L63 49 M64 53 L67 48", 1.3, 0.3, 0.2)),
    "whistle": (EYES + twice("M25 28 Q31 24 37 28 M47 28 Q53 24 59 28", 1.8)
                + '<circle cx="47" cy="56" r="4" stroke-width="2.4"/>'
                + twice("M78.5 11 L78.5 1 Q84 3 82.5 8", 1.6)  # a music note
                + '<ellipse cx="75.5" cy="11.5" rx="3.5" ry="2.6" fill="#000" transform="rotate(-20 75.5 11.5)"/>'),
    "melty": ('<g transform="rotate(22 40 42)">'
              '<ellipse cx="29" cy="34" rx="3" ry="4.5" fill="#000"/><ellipse cx="51" cy="34" rx="3" ry="4.5" fill="#000"/>'
              + twice("M25 47 Q38 60 53 47", 2.4) + "</g>"
              + twice("M20 64 Q19 70 22 71", 1.8)),  # a drip
    "glasses": ('<circle cx="31" cy="36" r="2.6" fill="#000"/><circle cx="53" cy="36" r="2.6" fill="#000"/>'
                + twice("M40 36 A9 9 0 1 1 22 36 A9 9 0 1 1 40 36 Z M62 36 A9 9 0 1 1 44 36 A9 9 0 1 1 62 36 Z", 2.2)
                + twice("M40 34 Q42 32 44 34 M22 34 L10 31 M62 34 L74 31", 2)
                + twice("M27 53 Q42 66 57 53", 2.4) + twice("M38 57 L38 63 L46 63 L46 57 M42 58 L42 63", 1.6)),
    "hot": (twice("M24 35 Q31 39 38 35 M46 35 Q53 39 60 35", 2.2)
            + '<path d="M28 50 Q42 46 56 50 Q54 62 42 62 Q30 62 28 50 Z" fill="#000" stroke-width="2"/>'
            + twice("M36 56 Q36 71 42 71 Q48 71 48 56", 1.8, fill="#fff") + twice("M42 58 L42 66", 1.2)
            + twice("M15 46 L18 41 M19 47 L22 42 M62 47 L65 42 M66 46 L69 41", 1.3, 0.3, 0.2)
            + twice("M14 18 C16 22 17 24 17 26 A3.5 3.5 0 0 1 10 26 C10 24 12 22 14 18 Z", 1.6)
            + twice("M72 12 C74 16 75 18 75 20 A3.5 3.5 0 0 1 68 20 C68 18 70 16 72 12 Z", 1.6)),
    "party": ('<path d="M6 33 L33 6 L2 2 Z" fill="#fff" stroke-width="3"/>'  # hat on the head's top left
              + '<path d="M9 25 L25 9 L21 7.5 L7.5 21 Z M8 14 L14 8 L9.5 6 L6 9.5 Z" fill="#000" stroke-width="1"/>'
              + '<circle cx="3" cy="3" r="3" fill="#000"/>'
              + twice("M28 38 Q33 32 38 38 M48 38 Q53 32 58 38", 2.4)
              + '<path d="M44 53 L45 58 L21 72 L15 61 Z" fill="#fff" stroke-width="2.4"/>'  # party horn
              + '<path d="M33 57 L35 63 M26 60 L29 67" stroke-width="2"/>'
              + twice("M40 51 Q48 58 57 51", 2.2)
              + '<circle cx="70" cy="34" r="1.8" fill="#000"/><circle cx="77" cy="46" r="1.8" fill="#000"/>'
              '<circle cx="8" cy="48" r="1.8" fill="#000"/>'
              + twice("M62 14 L66 18 M70 24 L74 22 M6 76 L10 73", 1.8)),
    "hearts": (twice("M31 45 C24 40 20 36 21 31 C22 27 28 26 31 31 C34 26 40 27 41 31 C42 36 38 40 31 45 Z", 1.4, fill="#000")
               .replace('transform="translate(0.6 0.4)"', 'transform="translate(-0.6 0)"')
               + twice("M53 45 C46 40 42 36 43 31 C44 27 50 26 53 31 C56 26 62 27 63 31 C64 36 60 40 53 45 Z", 1.4, fill="#000")
               + twice("M27 53 L57 53 Q55 68 42 68 Q29 68 27 53 Z", 1.8, fill="#000")),
    "laugh": (twice("M24 30 L35 36 L24 42 M60 30 L49 36 L60 42", 2.4)
              + twice("M25 49 L59 49 Q57 69 42 69 Q27 69 25 49 Z", 1.8, fill="#000")
              + '<path d="M34 62 Q42 56 50 62 Q48 68 42 68 Q36 68 34 62 Z" fill="#fff" stroke-width="1.2"/>'),  # tongue
    "halo": (twice("M24 38 Q31 43 38 38 M46 38 Q53 43 60 38", 2.2) + twice("M27 52 Q42 64 57 52", 2.4)
             + '<ellipse cx="42" cy="6" rx="22" ry="5" fill="#fff" stroke-width="2.4"/>'
             + '<ellipse cx="42.5" cy="6.4" rx="21.4" ry="4.5" fill="none" stroke-width="1.1"/>'),
    "excited": (twice(star(28, 39, 10), 1.4, fill="#000") + twice(star(56, 39, 10), 1.4, fill="#000")
                + '<path d="M27 52 Q42 72 57 52 Z" fill="#000" stroke-width="2"/>' + twice("M6 10 L10 14 M78 10 L74 14", 2)),
    "proud": (twice("M19 30 L65 30 L63 38 Q60 45 52 45 Q45 45 44 36 L40 36 Q39 45 32 45 Q24 45 21 38 Z", 1.8, fill="#000")
              + twice("M30 56 Q44 62 56 50", 2.4) + twice("M74 12 L78 8 M76 14 L82 14", 1.8)),
    "worried": (EYES + twice("M24 26 L36 30 M60 26 L48 30", 2) + twice("M30 60 Q42 50 54 60", 2.4)
                + twice("M68 12 C71 17 73 20 73 22 A5 5 0 0 1 63 22 C63 20 65 17 68 12 Z", 1.8)),
    "sleepy": (twice("M25 37 Q31 42 37 37 M47 37 Q53 42 59 37", 2.2) + '<ellipse cx="42" cy="55" rx="3.5" ry="4.5" stroke-width="2"/>'
               + twice("M62 8 L70 8 L62 16 L70 16", 1.8) + twice("M72 2 L77 2 L72 7 L77 7", 1.4)),
}

# Name -> (width, height, SVG body, wobble). Small pieces wobble less so they
# stay legible.
ART = {
    "bubble": (184, 108, twice(BUBBLE, 3.4), 1.3),
    "bubble_loud": (184, 108, f'<path d="{BUBBLE}" fill="#000" stroke-width="3.4"/>', 1.3),
    # Bins, 34px.
    "bin_rubbish": (34, 34, twice("M5 9 L29 8") + twice("M13 8 L14 4 L21 4 L21 8") + twice("M8 10 L10 31 L25 31 L27 10")
                    + twice("M13.5 14 L14.5 27", 1.6) + twice("M18 14 L18 27", 1.6) + twice("M22 14 L21.5 27", 1.6), 1.0),
    "bin_recycling": (34, 34, twice("M10 14 Q14 5 20 6 L23 11") + twice("M23 11 L18 10 M23 11 L25 6", 2)
                      + twice("M27 15 Q32 23 27 27 L22 28") + twice("M22 28 L25 24 M22 28 L25 32", 2)
                      + twice("M14 29 Q5 28 6 21 L9 17") + twice("M9 17 L9 22 M9 17 L4 18", 2), 1.0),
    "bin_food": (34, 34, twice("M17 10 C10 5 3 9 5 18 C6 26 11 32 17 29 C23 32 28 26 29 18 C31 9 24 5 17 10 Z")
                 + twice("M17 10 Q17 6 19 3", 1.8) + twice("M19 6 Q25 2 27 6 Q22 9 19 6 Z", 1.6)
                 + twice("M10 15 Q9 19 11 23", 1.2), 1.0),
    # Weather, 84x70.
    "weather_clear": (84, 70, sun(40, 33, 14), 1.3),
    "weather_partly_cloudy": (84, 70, sun(52, 22, 11) + cloud(2, 14, 0.95), 1.3),
    "weather_cloudy": (84, 70, cloud(6, 4) + cloud(18, 16, 0.85), 1.3),
    "weather_fog": (84, 70, cloud(10, -4, 0.8) + "".join(
        twice(f"M{8 + k * 3} {44 + k * 8} Q24 {40 + k * 8} 40 {44 + k * 8} T{72 - k * 3} {44 + k * 8}", 2.2) for k in range(3)), 1.3),
    "weather_drizzle": (84, 70, rain_cloud() + "".join(
        f'<circle cx="{x}" cy="{y}" r="1.6" fill="#000"/>' for x, y in [(22, 52), (34, 58), (46, 52), (58, 58), (28, 62), (52, 63)]), 1.3),
    "weather_rain": (84, 70, rain_cloud() + "".join(
        twice(f"M{x} {y} L{x - 4} {y + 10}", 2.4) for x, y in [(24, 47), (36, 50), (48, 47), (60, 50)]), 1.3),
    "weather_snow": (84, 70, rain_cloud() + "".join(
        twice(f"M{x - 4} {y} L{x + 4} {y} M{x} {y - 4} L{x} {y + 4} M{x - 3} {y - 3} L{x + 3} {y + 3} M{x - 3} {y + 3} L{x + 3} {y - 3}", 1.4)
        for x, y in [(24, 52), (40, 58), (56, 52)]), 1.3),
    "weather_thunder": (84, 70, rain_cloud() + twice("M40 42 L33 54 L41 54 L35 65 L48 50 L40 50 L45 42 Z", 1.8, fill="#000"), 1.3),
    # Status icons, 22px, drawn on an 18px grid: offline (Wi-Fi with a slash
    # through it), low battery, and out of date (a clock turning back).
    "status_offline": (22, 22, STATUS_GRID + '<circle cx="9" cy="14.5" r="1.4" fill="#000"/>'
                       + twice("M5.2 11.2 Q9 7.8 12.8 11.2", 1.7, 0.3, 0.2) + twice("M2 8 Q9 1.5 16 8", 1.7, 0.3, 0.2)
                       + '<path d="M2.5 2.5 L15.5 15.5" stroke="#fff" stroke-width="4.5"/><path d="M2.5 2.5 L15.5 15.5" stroke-width="1.8"/></g>', 0.6),
    "status_battery": (22, 22, STATUS_GRID + twice("M1.5 5 L14.5 5 L14.5 13 L1.5 13 Z", 1.6, 0.3, 0.2) + twice("M16.5 7.5 L16.5 10.5", 1.8, 0.3, 0.2)
                       + '<path d="M3.5 7 L6 7 L6 11 L3.5 11 Z" fill="#000" stroke-width="1"/></g>', 0.6),
    "status_stale": (22, 22, STATUS_GRID + twice("M3.3 6 A7 7 0 1 1 2.4 11", 1.6, 0.3, 0.2) + twice("M1.2 2.8 L3.3 6.5 L7 4.8", 1.6, 0.3, 0.2)
                     + twice("M9.5 5.5 L9.5 9.5 L12.5 11.5", 1.6, 0.3, 0.2) + "</g>", 0.6),
}
for face, body in FACES.items():
    ART[f"face_{face}"] = (84, 84, (MELTY if face == "melty" else FACE) + body, 1.3)


def rasterise(svg_text, w, h, tmp):
    """Renders the SVG and returns rows of booleans (True = black), w x h."""
    src = os.path.join(tmp, "a.svg")
    with open(src, "w") as f:
        f.write(svg_text)
    subprocess.run(["qlmanage", "-t", "-s", str(CANVAS), "-o", tmp, src], capture_output=True, check=True)
    bmp = os.path.join(tmp, "a.bmp")
    subprocess.run(["sips", "-s", "format", "bmp", src + ".png", "--out", bmp], capture_output=True, check=True)
    d = open(bmp, "rb").read()
    off = struct.unpack_from("<I", d, 10)[0]
    bw, bh = struct.unpack_from("<ii", d, 18)
    bpp = struct.unpack_from("<H", d, 28)[0] // 8
    stride = (bw * bpp + 3) & ~3

    def grey(x, y):
        base = off + ((abs(bh) - 1 - y) if bh > 0 else y) * stride + x * bpp
        return 0.299 * d[base + 2] + 0.587 * d[base + 1] + 0.114 * d[base]

    f = bw / CANVAS * scale_for(w, h)  # bitmap pixels per screen pixel
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            x0, x1, y0, y1 = int(x * f), int((x + 1) * f), int(y * f), int((y + 1) * f)
            vals = [grey(xx, yy) for yy in range(y0, max(y1, y0 + 1)) for xx in range(x0, max(x1, x0 + 1))]
            row.append(sum(vals) / len(vals) < 150)  # a little generous, so thin pen lines survive
        rows.append(row)
    return rows


def main():
    lines = ["// Generated by tools/doodles/make_doodles.py; do not edit.", '#include "doodles.h"', ""]
    with tempfile.TemporaryDirectory() as tmp:
        for name, (w, h, body, wobble) in ART.items():
            seed = zlib.crc32(name.encode()) % 1000  # stable per piece
            bits = rasterise(svg(w, h, body, seed, wobble), w, h, tmp)
            stride = (w + 7) // 8
            # I1: a 2-colour ARGB8888 palette (index 0 transparent, 1 black),
            # then rows of 1-bit indices, most significant bit first.
            data = [0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF]
            for row in bits:
                for b in range(stride):
                    byte = 0
                    for bit in range(8):
                        x = b * 8 + bit
                        if x < w and row[x]:
                            byte |= 0x80 >> bit
                    data.append(byte)
            hexes = ", ".join(f"0x{v:02x}" for v in data)
            lines += [
                f"static const uint8_t {name}_data[] = {{{hexes}}};",
                f"const lv_image_dsc_t doodle_{name} = {{",
                f"    .header = {{.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_I1, .flags = 0,",
                f"               .w = {w}, .h = {h}, .stride = {stride}}},",
                f"    .data_size = sizeof({name}_data),",
                f"    .data = {name}_data,",
                "};",
                "",
            ]
    with open(os.path.join(OUT, "doodles.c"), "w") as f:
        f.write("\n".join(lines))
    print(f"wrote {len(ART)} doodles to {OUT}/doodles.c")


if __name__ == "__main__":
    main()
