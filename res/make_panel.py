#!/usr/bin/env python3
# Draws res/Divskip.svg. VCV Rack can't draw SVG <text>, so every label is
# turned into outline paths with fontTools. Positions are in mm and must match
# src/Divskip.cpp (the numbers in the LAYOUT block are the same in both).
#   pip install fonttools
#   python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 121.92, 128.5  # 24 HP

BG = "#1D1F24"
BG_DARK = "#15171B"
LINE = "#2C2F36"
INK = "#EFE8DA"
SOFT = "#9EA3AE"
HOT = "#F0A04B"    # A
COOL = "#5FD3E6"   # B
OUTBOX = "#121317"

# ---- LAYOUT (same numbers in src/Divskip.cpp) ----
X_IN, X_MODE, X_CV, X_KNOB = 9.0, 20.0, 31.0, 45.0
X_DISP0, X_DISP1 = 55.5, 82.5
X_MUTE, X_A, X_B = 90.0, 101.5, 113.0
ROWS = [33.0, 53.0, 73.0, 93.0]
HEAD_Y = 22.6
BOT_Y = 114.5
X_RESET, X_OUTSW, X_MUTESW = 9.0, 38.0, 68.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)

fonts = {}


def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    """SVG path for string s; (x, y) is the baseline point in mm."""
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    scale = size / upm
    track = spacing * upm
    names = [cmap[ord(c)] for c in s]
    width = sum(gs[n].width + track for n in names) - track
    x0 = {"middle": x - width * scale / 2, "end": x - width * scale}.get(anchor, x)
    pen = SVGPathPen(gs)
    cx = 0.0
    for n in names:
        gs[n].draw(TransformPen(pen, (scale, 0, 0, -scale, x0 + cx * scale, y)))
        cx += gs[n].width + track
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append

add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

# title: the name, and a little fork (one path splitting into A and B)
add(text("DIVSKIP", 6.0, 11.2, 4.6, FONT_BOLD, INK, anchor="start", spacing=0.14))
fx, fy = 36.5, 9.6
add(f'<path d="M{fx} {fy} h4 l3 -2.4 h3.2 M{fx+4} {fy} l3 2.4 h3.2" stroke="{SOFT}" stroke-width="0.6" fill="none" stroke-linecap="round" stroke-linejoin="round"/>')
add(f'<circle cx="{fx+10.6}" cy="{fy-2.4}" r="0.9" fill="{HOT}"/>')
add(f'<circle cx="{fx+10.6}" cy="{fy+2.4}" r="0.9" fill="{COOL}"/>')
add(text("clock divider + skipper", W - 6.0, 11.0, 2.6, FONT_MED, SOFT, anchor="end"))

# output column box
add(f'<rect x="{X_A - 6.0}" y="16.0" width="{X_B - X_A + 12.0}" height="{ROWS[-1] + 8.6 - 16.0}" rx="1.4" fill="{OUTBOX}"/>')

# column heads
add(text("IN", X_IN, HEAD_Y, JACK))
add(text("MODE", X_MODE, HEAD_Y, JACK))
add(text("CV", X_CV, HEAD_Y, JACK))
add(text("AMOUNT", X_KNOB, HEAD_Y, CONTROL))
add(text("MUTE", X_MUTE, HEAD_Y, CONTROL))
add(text("A", X_A, HEAD_Y, JACK, fill=HOT))
add(text("B", X_B, HEAD_Y, JACK, fill=COOL))

# row separators and the "input feeds the next row down" arrows
for i, y in enumerate(ROWS):
    if i < len(ROWS) - 1:
        yy = (y + ROWS[i + 1]) / 2
        add(f'<line x1="3.5" y1="{yy}" x2="{X_A - 7.5}" y2="{yy}" stroke="{LINE}" stroke-width="0.35"/>')
        # normalling arrow under each input
        add(f'<path d="M{X_IN} {y + 5.2} V{ROWS[i+1] - 5.4} M{X_IN - 1.1} {ROWS[i+1] - 6.6} L{X_IN} {ROWS[i+1] - 5.4} L{X_IN + 1.1} {ROWS[i+1] - 6.6}" stroke="{SOFT}" stroke-width="0.4" fill="none" stroke-linecap="round" stroke-linejoin="round"/>')

# bottom row
add(f'<line x1="3.5" y1="103.2" x2="{W - 3.5}" y2="103.2" stroke="{LINE}" stroke-width="0.35"/>')
add(text("RESET", X_RESET, 108.2, JACK))
add(text("OUTPUTS", X_OUTSW, 108.2, CONTROL))
add(text("gate", X_OUTSW + 3.4, 113.9, 2.6, FONT_MED, SOFT, anchor="start"))
add(text("trig", X_OUTSW + 3.4, 117.5, 2.6, FONT_MED, SOFT, anchor="start"))
add(text("MUTE SET", X_MUTESW, 108.2, CONTROL))
add(text("M2", X_MUTESW + 3.4, 113.9, 2.6, FONT_MED, "#F08A3C", anchor="start"))
add(text("M1", X_MUTESW + 3.4, 117.5, 2.6, FONT_MED, COOL, anchor="start"))
# mini legend
LX = 90.0
add(text("click display: mode", LX, 108.2, 2.6, FONT_MED, SOFT, anchor="start"))
add(text("ring: A steps", LX, 111.9, 2.6, FONT_MED, HOT, anchor="start"))
add(text("B steps", LX + 6.95, 115.6, 2.6, FONT_MED, COOL, anchor="start"))
add(text("playhead", LX + 6.95, 119.3, 2.6, FONT_MED, INK, anchor="start"))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
open(os.path.join(HERE, "Divskip.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Divskip.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
