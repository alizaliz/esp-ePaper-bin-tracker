#!/usr/bin/env python3
"""Builds the README images in docs/images/ from the host preview renders.

Run tools/preview/run.sh first, then this script, from the repo root:
    python3 tools/docs/make_images.py

Copies a few screen previews and generates diagram.svg, which labels the
screen elements and the board's controls around the real "upcoming" render.
Element positions match src/ui/bin_screen.cpp; update them if the layout
changes.
"""
import base64
import shutil
from pathlib import Path

PREVIEW = Path("tools/preview/out")
OUT = Path("docs/images")
PREVIEWS = ["upcoming", "tonight", "today", "stale", "low_battery", "offline"]

SCALE = 2                 # preview PNGs are rendered at 2x
SCREEN_X, SCREEN_Y = 280, 130  # where the screen sits in the diagram
HEIGHT = 860


def at(x, y):
    """Screen pixel (200x200 space) to diagram coordinates."""
    return SCREEN_X + x * SCALE, SCREEN_Y + y * SCALE


def label(x, y, title, sub="", anchor="start"):
    out = f'<text x="{x}" y="{y}" text-anchor="{anchor}" class="t">{title}</text>'
    if sub:
        out += f'<text x="{x}" y="{y + 17}" text-anchor="{anchor}" class="s">{sub}</text>'
    return out


def leader(x1, y1, x2, y2):
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" class="l"/>'
            f'<circle cx="{x2}" cy="{y2}" r="4" class="d"/>')


def build_diagram():
    png = base64.b64encode((PREVIEW / "upcoming.png").read_bytes()).decode()
    parts = []

    # Device: bezel and the rendered screen
    parts.append(f'<rect x="{SCREEN_X - 24}" y="{SCREEN_Y - 24}" width="448" height="448" '
                 f'rx="20" class="bezel"/>')
    parts.append(f'<image x="{SCREEN_X}" y="{SCREEN_Y}" width="400" height="400" '
                 f'href="data:image/png;base64,{png}"/>')

    # Top row callouts, above the device, on two staggered rows so the
    # captions of neighbouring items don't overlap
    for (x, y), row, title, sub in [
        (at(17, 17), 1, "Wi-Fi", "slashed when offline"),
        (at(67, 17), 0, "Temperature", "outdoor, or onboard sensor"),
        (at(133, 17), 1, "Humidity", "same source as temperature"),
        (at(183, 17), 0, "Battery", "fill shows charge"),
    ]:
        top = 26 + row * 42
        parts.append(leader(x, top + 24, x, y - 14))
        parts.append(label(x, top, title, sub, "middle"))

    # Left side
    x, y = at(8, 84)
    parts.append(leader(240, y, x - 4, y))
    parts.append(label(232, y - 4, "Bins", "rubbish, recycling, food scraps", "end"))

    x, y = at(37, 167)
    parts.append(leader(240, y, x - 6, y))
    parts.append(label(232, y - 4, "Next pickup day", "TONIGHT, then TODAY, on a black strip", "end"))

    # Right side: an elbow through the gap under the icons to the slash on
    # the recycling bin
    x, y = at(112, 100)
    _, gap_y = at(0, 128)
    parts.append(f'<polyline points="720,{gap_y} {x},{gap_y} {x},{y + 4}" fill="none" class="l"/>')
    parts.append(f'<circle cx="{x}" cy="{y + 4}" r="4" class="d"/>')
    parts.append(label(728, gap_y - 4, "Slashed bin", "not collected this pickup"))

    # Bin names under the columns
    for cx, name in [(33.5, "Rubbish"), (99.5, "Recycling"), (165.5, "Food scraps")]:
        parts.append(label(at(cx, 0)[0], SCREEN_Y + 446, name, "", "middle"))

    # Board controls panel. Shows what each part does; the drawings aren't
    # placed where the parts sit on the board.
    panel = SCREEN_Y + 508
    parts.append(f'<text x="40" y="{panel}" class="h">On the board</text>')
    parts.append(f'<line x1="40" y1="{panel + 12}" x2="920" y2="{panel + 12}" class="rule"/>')
    items = [
        ("led", "Green LED", ["Blinks for 10 s every", "10 min below 10% battery"]),
        ("pwr", "PWR button", ["Press to restart and", "refresh; powers on", "from battery"]),
        ("boot", "BOOT button", ["Hold while powering on", "for download mode"]),
        ("usb", "USB-C", ["Power, charging,", "flashing and logs"]),
        ("bat", "Battery header", ["MX1.25, 3.7V lithium", "cell, charged over USB"]),
    ]
    for i, (kind, title, lines) in enumerate(items):
        cx, cy = 96 + i * 176, panel + 62
        parts.append(icon(kind, cx, cy))
        parts.append(label(cx, cy + 50, title, "", "middle"))
        for j, line in enumerate(lines):
            parts.append(f'<text x="{cx}" y="{cy + 70 + j * 17}" text-anchor="middle" '
                         f'class="s">{line}</text>')

    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="960" height="{HEIGHT}" viewBox="0 0 960 {HEIGHT}">
<style>
  .t {{ font: 600 15px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif; fill: #1f2328; }}
  .s {{ font: 13px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif; fill: #59636e; }}
  .h {{ font: 700 18px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif; fill: #1f2328; }}
  .l {{ stroke: #0969da; stroke-width: 1.5; }}
  .d {{ fill: #0969da; }}
  .rule {{ stroke: #d1d9e0; stroke-width: 1; }}
  .bezel {{ fill: #e7e9ec; stroke: #9aa3ad; stroke-width: 2; }}
</style>
<rect width="960" height="{HEIGHT}" fill="#ffffff"/>
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
    for name in PREVIEWS:
        shutil.copy(PREVIEW / f"{name}.png", OUT / f"screen_{name}.png")
    (OUT / "diagram.svg").write_text(build_diagram())
    print(f"wrote {len(PREVIEWS)} previews and diagram.svg to {OUT}/")


if __name__ == "__main__":
    main()
