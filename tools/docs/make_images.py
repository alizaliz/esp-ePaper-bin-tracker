#!/usr/bin/env python3
"""Builds the README images in docs/images/ from the host preview renders.

Run tools/preview/run.sh first, then this script, from the repo root:
    python3 tools/docs/make_images.py

Copies a few screen previews and generates diagram.svg, which labels the
screen around the real renders: the main screen, the bottom of the screen
when something's wrong, the mascot's moods, and the board's controls.
Callout positions match src/ui/bin_screen.cpp; update them if the layout
changes.
"""
import base64
import shutil
from pathlib import Path

PREVIEW = Path("tools/preview/out")
OUT = Path("docs/images")
PREVIEWS = ["upcoming", "tonight", "today", "stale", "low_battery", "offline"]

WIDTH = 960
SCALE = 2                       # preview PNGs are rendered at 2x
SCREEN_X, SCREEN_Y = 280, 40    # where the main screen sits in the diagram


def png(name):
    return base64.b64encode((PREVIEW / f"{name}.png").read_bytes()).decode()


def at(x, y, top=SCREEN_Y):
    """Screen pixel (200x200 space) to diagram coordinates."""
    return SCREEN_X + x * SCALE, top + y * SCALE


def label(x, y, title, sub="", anchor="start"):
    out = f'<text x="{x}" y="{y}" text-anchor="{anchor}" class="t">{title}</text>'
    if sub:
        out += f'<text x="{x}" y="{y + 17}" text-anchor="{anchor}" class="s">{sub}</text>'
    return out


def leader(x1, y1, x2, y2):
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" class="l"/>'
            f'<circle cx="{x2}" cy="{y2}" r="4" class="d"/>')


def left_callout(target, title, sub):
    x, y = target
    return leader(240, y, x - 6, y) + label(232, y - 4, title, sub, "end")


def right_callout(target, title, sub):
    x, y = target
    return leader(720, y, x + 6, y) + label(728, y - 4, title, sub)


def bezel(x, y, w, h):
    return f'<rect x="{x - 24}" y="{y - 24}" width="{w + 48}" height="{h + 48}" rx="20" class="bezel"/>'


def cropped(name, x, y, top, height):
    """Screen rows top..top+height of a preview, at diagram scale."""
    return (f'<svg x="{x}" y="{y}" width="400" height="{height * SCALE}" '
            f'viewBox="0 {top * SCALE} 400 {height * SCALE}">'
            f'<image width="400" height="400" href="data:image/png;base64,{png(name)}"/></svg>')


def build_diagram():
    parts = []

    # Main screen: an ordinary day.
    parts.append(bezel(SCREEN_X, SCREEN_Y, 400, 400))
    parts.append(f'<image x="{SCREEN_X}" y="{SCREEN_Y}" width="400" height="400" '
                 f'href="data:image/png;base64,{png("upcoming")}"/>')
    parts.append(right_callout(at(150, 46), "Next pickup day",
                               "TONIGHT, then TODAY, in a black bubble"))
    parts.append(left_callout(at(68, 74), "Bins going out", "only the ones collected"))
    parts.append(left_callout(at(12, 152), "Binny", "face changes with the day"))
    parts.append(right_callout(at(190, 143), "Temperature", "today's average"))
    parts.append(right_callout(at(190, 169), "Humidity", "today's average"))

    # When something's wrong: the bottom of the screen with every problem.
    top = SCREEN_Y + 400 + 100
    strip_top, strip_h = 119, 81  # just below the speech bubble's tail
    parts.append(f'<text x="{SCREEN_X + 200}" y="{top - 38}" text-anchor="middle" class="h">'
                 f'When something needs attention</text>')
    parts.append(bezel(SCREEN_X, top, 400, strip_h * SCALE))
    parts.append(cropped("offline", SCREEN_X, top, strip_top, strip_h))
    shift = top - strip_top * SCALE  # maps screen rows in the strip to diagram y
    parts.append(left_callout(at(12, 152, shift), "Worried Binny", "offline or out of date"))
    parts.append(right_callout(at(186, 185, shift), "Status icons",
                               "offline, low battery, out of date"))
    parts.append(f'<text x="{SCREEN_X + 200}" y="{top + strip_h * SCALE + 46}" text-anchor="middle" class="s">'
                 f'Icons appear only when needed. The bubble says "Last known" when the dates may be old.</text>')

    # Moods.
    moods_y = top + strip_h * SCALE + 110
    parts.append(f'<text x="40" y="{moods_y}" class="h">Binny\'s moods</text>')
    parts.append(f'<line x1="40" y1="{moods_y + 12}" x2="920" y2="{moods_y + 12}" class="rule"/>')
    moods = [("happy", "Happy", "ordinary days"), ("wink", "Cheeky", "alternate days"),
             ("excited", "Star-struck", "bin night"), ("proud", "Cool", "collection day"),
             ("worried", "Worried", "offline or old data"), ("sleepy", "Sleepy", "low battery")]
    size = 126  # 84px faces at 1.5x
    for i, (name, title, sub) in enumerate(moods):
        cx = 100 + i * 152
        y = moods_y + 32
        parts.append(f'<svg x="{cx - size // 2}" y="{y}" width="{size}" height="{size}" viewBox="0 0 168 168">'
                     f'<image width="400" height="400" href="data:image/png;base64,{png("face_" + name)}"/></svg>')
        parts.append(label(cx, y + size + 22, title, sub, "middle"))

    # Board controls. Shows what each part does; the drawings aren't placed
    # where the parts sit on the board.
    panel = moods_y + 32 + size + 100
    parts.append(f'<text x="40" y="{panel}" class="h">On the board</text>')
    parts.append(f'<line x1="40" y1="{panel + 12}" x2="920" y2="{panel + 12}" class="rule"/>')
    items = [
        ("led", "Green LED", ["Slow blink on bin night;", "double-blink below", "10% battery"]),
        ("pwr", "PWR button", ["Press to restart and", "refresh; powers on", "from battery"]),
        ("boot", "BOOT button", ["Hold while powering on", "for download mode"]),
        ("usb", "USB-C", ["Power, charging,", "flashing and settings"]),
        ("bat", "Battery header", ["MX1.25, 3.7V lithium", "cell, charged over USB"]),
    ]
    for i, (kind, title, lines) in enumerate(items):
        cx, cy = 96 + i * 176, panel + 62
        parts.append(icon(kind, cx, cy))
        parts.append(label(cx, cy + 50, title, "", "middle"))
        for j, line in enumerate(lines):
            parts.append(f'<text x="{cx}" y="{cy + 70 + j * 17}" text-anchor="middle" class="s">{line}</text>')
    height = panel + 62 + 70 + 3 * 17 + 30

    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{height}" viewBox="0 0 {WIDTH} {height}">
<style>
  .t {{ font: 600 15px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif; fill: #1f2328; }}
  .s {{ font: 13px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif; fill: #59636e; }}
  .h {{ font: 700 18px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif; fill: #1f2328; }}
  .l {{ stroke: #0969da; stroke-width: 1.5; }}
  .d {{ fill: #0969da; }}
  .rule {{ stroke: #d1d9e0; stroke-width: 1; }}
  .bezel {{ fill: #e7e9ec; stroke: #9aa3ad; stroke-width: 2; }}
  image {{ image-rendering: pixelated; }}
</style>
<rect width="{WIDTH}" height="{height}" fill="#ffffff"/>
{"".join(parts)}
</svg>
'''


def icon(kind, cx, cy):
    """Simple drawings of the board parts. Not to scale or position."""
    if kind == "led":
        rays = "".join(
            f'<line x1="{cx + dx * 22}" y1="{cy + dy * 22}" x2="{cx + dx * 30}" y2="{cy + dy * 30}" '
            f'stroke="#1a7f37" stroke-width="2.5" stroke-linecap="round"/>'
            for dx, dy in [(0, -1), (0.71, -0.71), (1, 0), (0.71, 0.71), (0, 1),
                           (-0.71, 0.71), (-1, 0), (-0.71, -0.71)])
        return (rays + f'<circle cx="{cx}" cy="{cy}" r="14" fill="#2da44e" stroke="#1a7f37" stroke-width="2"/>'
                f'<circle cx="{cx - 4}" cy="{cy - 4}" r="4" fill="#aceebb"/>')
    if kind in ("pwr", "boot"):
        return (f'<rect x="{cx - 26}" y="{cy - 18}" width="52" height="36" rx="6" fill="#d0d7de" stroke="#6e7781" stroke-width="2"/>'
                f'<circle cx="{cx}" cy="{cy}" r="11" fill="#424a53"/>')
    if kind == "usb":
        return (f'<rect x="{cx - 26}" y="{cy - 11}" width="52" height="22" rx="11" fill="#d0d7de" stroke="#6e7781" stroke-width="2"/>'
                f'<rect x="{cx - 14}" y="{cy - 3}" width="28" height="6" rx="3" fill="#424a53"/>')
    if kind == "bat":
        return (f'<rect x="{cx - 24}" y="{cy - 13}" width="44" height="26" rx="4" fill="none" stroke="#424a53" stroke-width="3"/>'
                f'<rect x="{cx + 20}" y="{cy - 6}" width="5" height="12" fill="#424a53"/>'
                f'<rect x="{cx - 19}" y="{cy - 8}" width="24" height="16" fill="#424a53"/>')
    raise ValueError(kind)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("screen_*.png"):
        old.unlink()
    for name in PREVIEWS:
        shutil.copy(PREVIEW / f"{name}.png", OUT / f"screen_{name}.png")
    (OUT / "diagram.svg").write_text(build_diagram())
    print(f"wrote {len(PREVIEWS)} previews and diagram.svg to {OUT}/")


if __name__ == "__main__":
    main()
