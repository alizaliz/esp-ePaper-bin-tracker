#!/usr/bin/env python3
"""Draws the README's battery chart from a battery log CSV.

The CSV is the one the settings page downloads (Battery log > Download CSV):
    date,battery_mv,battery_percent,wakes,awake_ms,wifi_ms
Run from the repo root:
    python3 tools/docs/make_battery_chart.py docs/data/battery-log-waveshare.csv

Writes docs/images/battery_log.svg: the daily battery level as a line, with
gaps where there are no readings, and the discharge periods marked. The SVG
follows the reader's light or dark mode.
"""
import csv
import sys
from datetime import date, timedelta
from pathlib import Path

OUT = Path("docs/images/battery_log.svg")

# Periods worth calling out: (first day, last day, label). Days are inclusive.
PERIODS = [
    ("2026-10-07", "2026-10-17", "On USB, kept charged"),
    ("2026-10-27", "2026-10-31", "92% to 10% in 4 days"),
    ("2026-11-12", "2026-11-17", "88% to empty in 5 days (v1.4.2)"),
]

W, H = 960, 380
LEFT, RIGHT, TOP, BOTTOM = 56, 24, 64, 44


def load(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            if int(r["battery_mv"]) > 0:  # days with no reading stay a gap
                rows.append((date.fromisoformat(r["date"]), int(r["battery_percent"]), int(r["battery_mv"])))
    return rows


def main():
    rows = load(sys.argv[1] if len(sys.argv) > 1 else "docs/data/battery-log-waveshare.csv")
    first, last = rows[0][0], rows[-1][0]
    span = (last - first).days or 1
    plot_w, plot_h = W - LEFT - RIGHT, H - TOP - BOTTOM

    def x(d):
        return LEFT + (d - first).days / span * plot_w

    def y(pct):
        return TOP + (100 - pct) / 100 * plot_h

    parts = []
    # Discharge periods: a light band behind the line, labelled at the top.
    for start, end, label in PERIODS:
        x0 = x(date.fromisoformat(start)) - 6
        x1 = x(date.fromisoformat(end)) + 6
        parts.append(f'<rect x="{x0:.1f}" y="{TOP}" width="{x1 - x0:.1f}" height="{plot_h}" class="band"/>')
        parts.append(f'<text x="{(x0 + x1) / 2:.1f}" y="{TOP - 8}" text-anchor="middle" class="note">{label}</text>')
    # Gridlines and y-axis ticks.
    for pct in (0, 25, 50, 75, 100):
        parts.append(f'<line x1="{LEFT}" x2="{W - RIGHT}" y1="{y(pct):.1f}" y2="{y(pct):.1f}" '
                     f'class="{"axis" if pct == 0 else "grid"}"/>')
        parts.append(f'<text x="{LEFT - 8}" y="{y(pct) + 4:.1f}" text-anchor="end" class="tick">{pct}%</text>')
    # Weekly date ticks, on Mondays.
    d = first + timedelta(days=(7 - first.weekday()) % 7)
    while d <= last:
        parts.append(f'<text x="{x(d):.1f}" y="{H - BOTTOM + 20}" text-anchor="middle" class="tick">'
                     f'{d.day} {d.strftime("%b")}</text>')
        d += timedelta(days=7)
    # The line, broken wherever a day has no reading.
    runs, run = [], [rows[0]]
    for prev, cur in zip(rows, rows[1:]):
        if (cur[0] - prev[0]).days == 1:
            run.append(cur)
        else:
            runs.append(run)
            run = [cur]
    runs.append(run)
    for run in runs:
        if len(run) > 1:
            pts = " ".join(f"{x(d):.1f},{y(p):.1f}" for d, p, _ in run)
            parts.append(f'<polyline points="{pts}" class="line"/>')
    for d, p, mv in rows:
        parts.append(f'<circle cx="{x(d):.1f}" cy="{y(p):.1f}" r="4" class="dot">'
                     f'<title>{d.isoformat()}: {p}% ({mv / 1000:.2f} V)</title></circle>')

    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-labelledby="t d">
<title id="t">Battery level, Waveshare board</title>
<desc id="d">Daily battery percentage from the board's own log, {first.isoformat()} to {last.isoformat()}. Each full charge of the 500mAh battery lasted 4 to 5 days.</desc>
<style>
  svg {{ --surface: #fcfcfb; --ink: #0b0b0b; --ink2: #52514e; --muted: #898781; --grid: #e1e0d9; --axis: #c3c2b7; --band: #f0efec; --series: #2a78d6; }}
  @media (prefers-color-scheme: dark) {{
    svg {{ --surface: #1a1a19; --ink: #f0efec; --ink2: #c3c2b7; --muted: #898781; --grid: #2c2c2a; --axis: #383835; --band: #262624; --series: #3987e5; }}
  }}
  text {{ font-family: system-ui, -apple-system, "Segoe UI", sans-serif; }}
  .bg {{ fill: var(--surface); }}
  .title {{ font-size: 16px; font-weight: 600; fill: var(--ink); }}
  .note {{ font-size: 12px; fill: var(--ink2); }}
  .tick {{ font-size: 12px; fill: var(--muted); font-variant-numeric: tabular-nums; }}
  .grid {{ stroke: var(--grid); stroke-width: 1; }}
  .axis {{ stroke: var(--axis); stroke-width: 1; }}
  .band {{ fill: var(--band); }}
  .line {{ fill: none; stroke: var(--series); stroke-width: 2; stroke-linejoin: round; stroke-linecap: round; }}
  .dot {{ fill: var(--series); stroke: var(--surface); stroke-width: 2; }}
</style>
<rect width="{W}" height="{H}" rx="8" class="bg"/>
<text x="{LEFT}" y="26" class="title">Battery level per day, 500mAh LiPo on the Waveshare board</text>
{"".join(parts)}
</svg>
'''
    OUT.write_text(svg)
    print(f"wrote {OUT} ({len(rows)} readings)")


if __name__ == "__main__":
    main()
