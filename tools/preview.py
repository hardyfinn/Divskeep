#!/usr/bin/env python3
# Mock-up picture of the panel with knobs, jacks, rings and displays drawn in,
# so the layout can be checked without Rack.  python3 tools/preview.py out.png
import os, re, sys, math, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "res"))
import importlib.util
svg = open(os.path.join(HERE, "..", "res", "Divskip.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
block = src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1]
exec(block, L)
g = []
def jack(x, y):
    g.append(f'<circle cx="{x}" cy="{y}" r="4.1" fill="#C9CCD1"/><circle cx="{x}" cy="{y}" r="3.1" fill="#7C8088"/><circle cx="{x}" cy="{y}" r="1.9" fill="#0B0B0C"/>')
def knob(x, y, a):
    g.append(f'<circle cx="{x}" cy="{y}" r="4.9" fill="#26272B" stroke="#3A3C42" stroke-width="0.5"/>')
    ang = math.radians(-135 + 270 * a)
    g.append(f'<line x1="{x}" y1="{y}" x2="{x + 4*math.sin(ang):.2f}" y2="{y - 4*math.cos(ang):.2f}" stroke="#EEE" stroke-width="0.7" stroke-linecap="round"/>')
def ring(x, y, a, b, play, col):
    for i in range(16):
        ang = math.radians(i * 22.5)
        cx, cy = x + 7.3 * math.sin(ang), y - 7.3 * math.cos(ang)
        c = "#FFFFFF" if i == play else (col if a[i] else ("#5FD3E6" if b[i] else "#33363D"))
        g.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="0.75" fill="{c}"/>')
def disp(y, name, val, col, shape=""):
    x0, x1 = L["X_DISP0"], L["X_DISP1"]
    g.append(f'<rect x="{x0}" y="{y-6.5}" width="{x1-x0}" height="13" rx="1" fill="#0D0E11"/>')
    g.append(f'<text x="{x0+1.6}" y="{y-2.4}" font-family="monospace" font-size="3.1" fill="{col}">{name}</text>')
    g.append(f'<text x="{x0+1.6}" y="{y+1.6}" font-family="monospace" font-size="2.5" fill="#C9CCD1">{val}</text>')
    g.append(f'<text x="{x0+1.6}" y="{y+5.2}" font-family="monospace" font-size="2.5" fill="#8E939C">{shape}</text>')
    g.append(f'<circle cx="{x1-4.6}" cy="{y-3.5}" r="0.9" fill="#F0A04B"/><circle cx="{x1-1.9}" cy="{y-3.5}" r="0.9" fill="#3A3D44"/>')
def button(x, y, on):
    g.append(f'<rect x="{x-3}" y="{y-3}" width="6" height="6" rx="1" fill="#2B2D33"/><circle cx="{x}" cy="{y}" r="2.1" fill="{"#5FD3E6" if on else "#1A1B1F"}"/>')
def switch(x, y):
    g.append(f'<rect x="{x-1.6}" y="{y-3.4}" width="3.2" height="6.8" rx="0.6" fill="#B9BCC2"/><rect x="{x-1.2}" y="{y+0.2}" width="2.4" height="2.8" fill="#444"/>')
def eu(k, n): return [((i*k) % n) < k for i in range(n)] + [False]*(16-n)
shapes = ["rotate 0", "mask 5", "steps 16", "(no shape)"]
dimmed = [False, False, False, True]
rows = [
    ("1 DIVIDE", "A /4    B /12", "#9BE07A", [i % 4 == 0 for i in range(16)], [i % 12 == 0 for i in range(16)], 5, 0.21, False),
    ("2 TURING", "flip 18%", "#B48CFF", [bool(0xA5C3 >> i & 1) for i in range(16)], [not (0xA5C3 >> i & 1) for i in range(16)], 9, 0.68, False),
    ("3 RANDOM", "7 of 16 hit", "#6FE0C0", [bool(0x4A93 >> i & 1) for i in range(16)], [not (0x4A93 >> i & 1) for i in range(16)], 2, 0.44, False),
    ("4 PATTERN", "ae limp", "#FF7AA8", [c == "x" for c in "x..x.x....x..x.x"], [c == "x" for c in "...x....x..x...."], 7, 0.68, False),
]
for (name, val, col, a, b, play, amt, muted), y, sh, dim in zip(rows, L["ROWS"], shapes, dimmed):
    for x in (L["X_IN"], L["X_MODE"], L["X_CV"], L["X_CV2"], L["X_SEEDIN"], L["X_A"], L["X_B"]): jack(x, y)
    knob(L["X_SHAPE"], y, 0.3)
    if dim: g.append(f'<circle cx="{L["X_SHAPE"]}" cy="{y}" r="5.4" fill="#1D1F24" fill-opacity="0.65"/>')
    if muted: ring(L["X_KNOB"], y, [True]*16, [False]*16, -1, "#2E7F8C")
    else: ring(L["X_KNOB"], y, a, b, play, col)
    knob(L["X_KNOB"], y, amt)
    disp(y, name, val + ("  MUTED" if muted else ""), col, sh)
    button(L["X_MUTE"], y, muted)
jack(L["X_RESET"], L["BOT_Y"]); jack(L["X_GSEEDIN"], L["BOT_Y"]); button(L["X_GSEEDBTN"], L["BOT_Y"], False); switch(L["X_OUTSW"], L["BOT_Y"]); switch(L["X_MUTESW"], L["BOT_Y"])
for x in (6.5, 162.56-6.5):
    for y in (3, 125.5): g.append(f'<circle cx="{x}" cy="{y}" r="1.6" fill="#8A8D93"/>')
svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
tmp = out + ".svg"; open(tmp, "w").write(svg)
js = f"""
const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: 815, height: 644}}, deviceScaleFactor: 2}});
await p.goto('file://{os.path.abspath(tmp)}.html');
await p.waitForTimeout(300);
await p.screenshot({{path: '{os.path.abspath(out)}'}}); await b.close(); }})();
"""
open(tmp + ".html", "w").write('<html><body style="margin:0;background:#111"><img src="' + os.path.basename(tmp) + '" style="width:815px;height:644px"></body></html>')
open(out + ".js", "w").write(js)
subprocess.run(["node", out + ".js"], check=True, cwd="/tmp")
os.remove(out + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", out)
