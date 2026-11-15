#!/usr/bin/env python3
# Builds the Divskip user guide (HTML), then prints it to A4 PDF with Playwright.
#   python3 docs/make_guide.py out.pdf
# The step diagrams are worked out here with the same rules as src/DivskipEngine.hpp.
import os, re, sys, subprocess, html, json

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "Divskip-Guide.pdf")

A_COL, B_COL = "#d9822b", "#1f8fa0"


def rnd(x):
    import math
    return int(math.floor(x + 0.5))


def eu(k, n, i):
    return (i * k) % n < k


# ---- patterns, from the generated header -------------------------------------
hdr = open(os.path.join(ROOT, "src", "DivskipPatterns.hpp")).read()
kits = []
for block in hdr.split("{ // ")[1:]:
    kits.append([(m.group(1), int(m.group(2), 16), int(m.group(3), 16))
                 for m in re.finditer(r'\{"([^"]+)", 0x([0-9A-F]+)ull, 0x([0-9A-F]+)ull\}', block)])
assert len(kits) == 4 and all(len(k) == 24 for k in kits)


def grid(a, b, n=16, label="", play=None, note=""):
    """Two rows of n cells: A on top, B underneath."""
    cw, ch, gap = 22, 16, 3
    w = 40 + n * (cw + gap)
    h = 2 * ch + gap + 18
    out = [f'<svg viewBox="0 0 {w} {h}" width="{w * 0.62:.0f}" height="{h * 0.62:.0f}" font-family="Inter, Helvetica" font-size="11">']
    for r, (row, col, name) in enumerate([(a, A_COL, "A"), (b, B_COL, "B")]):
        y = r * (ch + gap)
        out.append(f'<text x="2" y="{y + 12}" font-weight="700" fill="{col}">{name}</text>')
        for i in range(n):
            x = 20 + i * (cw + gap)
            on = row[i] if i < len(row) else False
            fill = col if on else ("#efe9de" if i % 4 else "#e3dccf")
            out.append(f'<rect x="{x}" y="{y}" width="{cw}" height="{ch}" rx="3" fill="{fill}"/>')
    for i in range(0, n, 4):
        out.append(f'<text x="{20 + i * (cw + gap) + cw / 2}" y="{2 * ch + gap + 14}" text-anchor="middle" fill="#8a847a" font-size="10">{i + 1}</text>')
    out.append("</svg>")
    cap = f'<div class="gcap"><b>{html.escape(label)}</b> {html.escape(note)}</div>' if label or note else ""
    return f'<div class="grid">{cap}{"".join(out)}</div>'


def coin_demo(p, seed):
    import random
    r = random.Random(seed)
    b = [r.random() < p for _ in range(16)]
    return [not x for x in b], b


def sticky_demo(p, seed):
    import random
    r = random.Random(seed)
    cur, b = False, []
    for _ in range(16):
        if r.random() >= min(0.97, p):
            cur = not cur
        b.append(cur)
    return [not x for x in b], b


def divide(p):
    la = 1 + rnd(p * 14); lb = 16 - la
    return [i % la == 0 for i in range(16)], [i % lb == 0 for i in range(16)], la, lb


def split(p):
    la = 3 + rnd(p * 10); lb = 16 - la
    a = [i < la and eu(3, la, i) for i in range(16)]
    b = [i >= la and eu(3, lb, i - la) for i in range(16)]
    return a, b, la, lb


CLASSIC = [(4, 1), (4, 2), (4, 3), (5, 1), (5, 2), (5, 3), (6, 1), (6, 2), (6, 3), (6, 5), (7, 1), (7, 2), (7, 3),
           (8, 1), (8, 3), (8, 5), (9, 1), (9, 4), (10, 3), (10, 7), (11, 3), (11, 4), (12, 5), (12, 7), (13, 3),
           (13, 4), (14, 3), (14, 5), (15, 4), (15, 7), (16, 3), (16, 5)]


def classic(n, k):
    a = [eu(k, n, i % n) for i in range(16)]
    return a, [not x for x in a]


def ramp_svg(p, gate=False):
    def f(x):
        acc = 1 - (1 - x) ** 2; dec = x * x; c = x - .5
        both = .3 * x + .7 * (4 * c ** 3 + .5)
        s = (p - .5) * 2
        return both + (acc - both) * (-s) if s < 0 else both + (dec - both) * s
    w, h = 420, 44
    out = [f'<svg viewBox="0 0 {w} {h}" width="{w * 0.62:.0f}" height="{h * 0.62:.0f}">',
           f'<rect x="0" y="0" width="{w}" height="{h}" rx="4" fill="#f4efe6"/>']
    for k in range(16):
        x = 8 + f(k / 16) * 0.94 * (w - 16)
        col = A_COL if k % 2 == 0 else B_COL
        y0 = 6 if k % 2 == 0 else 24
        out.append(f'<rect x="{x:.1f}" y="{y0}" width="3" height="14" fill="{col}"/>')
    out.append("</svg>")
    return "".join(out)


def ratchet_svg(na):
    nb = 16 - na
    w, h = 420, 44
    out = [f'<svg viewBox="0 0 {w} {h}" width="{w * 0.62:.0f}" height="{h * 0.62:.0f}">',
           f'<rect x="0" y="0" width="{w}" height="{h}" rx="4" fill="#f4efe6"/>']
    for k in range(na):
        out.append(f'<rect x="{8 + k / na * (w - 16):.1f}" y="6" width="3" height="14" fill="{A_COL}"/>')
    for k in range(nb):
        out.append(f'<rect x="{8 + k / nb * (w - 16):.1f}" y="24" width="3" height="14" fill="{B_COL}"/>')
    out.append("</svg>")
    return "".join(out)


def gate_svg(frac):
    w, h = 420, 44
    x1 = 8 + frac * (w - 16)
    x2 = 8 + 0.97 * (w - 16)
    return (f'<svg viewBox="0 0 {w} {h}" width="{w * 0.62:.0f}" height="{h * 0.62:.0f}">'
            f'<rect x="0" y="0" width="{w}" height="{h}" rx="4" fill="#f4efe6"/>'
            f'<rect x="8" y="6" width="{x1 - 8:.1f}" height="14" rx="2" fill="{A_COL}"/>'
            f'<rect x="{x1:.1f}" y="24" width="{x2 - x1:.1f}" height="14" rx="2" fill="{B_COL}"/></svg>')


def bits(v, bar=0):
    return [bool(v >> (bar * 16 + i) & 1) for i in range(16)]


def labelled(svg, label):
    return f'<div class="grid"><div class="gcap"><b>{label}</b></div>{svg}</div>'


# ---- the pages -------------------------------------------------------------------
P = []
add = P.append

add(f"""
<section class="page cover">
  <div>
    <div class="kicker">Haer · VCV Rack 2 module · user guide</div>
    <h1>Divskip</h1>
    <p class="lead">Four channels. Each takes a clock and splits it between two outputs:
    <b style="color:{A_COL}">A</b>, the hits that play, and <b style="color:{B_COL}">B</b>, the hits that are skipped.
    Ten ways of choosing, from plain division to self-mutating loops, and channels that chain into longer phrases.</p>
  </div>
  <div class="fig"><img src="panel.png" style="width:118mm"></div>
  <div class="small">Divskip 2.0.0 (plugin slug <i>Divskip</i>, 24 HP). Inspired by the DivSkip Eurorack module by DivKid and
  Making Sound Machines; this is an independent VCV version with its own panel, drum patterns and extra modes.
  The panel picture is a render, not a Rack screenshot. November 2026.</div>
</section>
""")

add(f"""
<section class="page">
  <h2>Five-minute start</h2>
  <div class="step"><span class="num">1</span><div>Add Divskip to a patch. Patch a <b>16th-note clock</b> (Clocked, or Live's clock through the disting NT) into <span class="jack">IN</span> on channel 1. Leave the other three INs empty: they pick up channel 1's clock (the little arrows on the panel show this).</div></div>
  <div class="step"><span class="num">2</span><div>Patch channel 1 <span class="k" style="color:{A_COL}">A</span> to a kick and <span class="k" style="color:{B_COL}">B</span> to a hi-hat. Channel 1 starts in <b>DIVIDE</b>: with the knob at noon both outputs play every 8th clock.</div></div>
  <div class="step"><span class="num">3</span><div>Turn <b>AMOUNT</b>. A gets faster and B slower (or the other way round). The light ring shows the 16 steps: A steps in the mode's colour, B steps cyan, white for the playhead.</div></div>
  <div class="step"><span class="num">4</span><div><b>Click the display</b> to go to the next mode. Try COIN, TURING and RANDOM. <b>Shift-click</b> goes back, <b>right-click</b> lists all ten.</div></div>
  <div class="step"><span class="num">5</span><div>Press <b>MUTE</b> on a channel, flip <b>MUTE SET</b> to M2, mute a different channel, and flip back and forth: two mute scenes on one switch.</div></div>

  <h2 style="margin-top:6mm">The panel</h2>
  <table>
    <tr><th>Part</th><th>What it does</th></tr>
    <tr><td>IN</td><td>Clock or any triggers. An empty IN takes the signal from the channel above, so one clock in channel 1 drives all four. Patch a different clock into any channel to break the chain of copies from there down.</td></tr>
    <tr><td>MODE (jack)</td><td>Moves the mode on from the one you picked: each +1 V is one mode further, −1 V one back, and it wraps round. A sequencer with 0, 1, 2 V steps switches modes in time with the music.</td></tr>
    <tr><td>CV (jack)</td><td>±5 V, added to the AMOUNT knob. The knob covers 0 to 5 V, so +2.5 V moves the knob's effect half way. Use an LFO here to sweep a mode's setting.</td></tr>
    <tr><td>AMOUNT</td><td>The one setting of the mode (see the mode pages). The light ring around it shows the steps.</td></tr>
    <tr><td>Display</td><td>Channel number, mode and its setting in words, and two dots that flash when A and B fire. It dims while a chained channel waits its turn.</td></tr>
    <tr><td>MUTE</td><td>Silences A and B. The channel keeps counting, so it comes back in time. Lit cyan in set M1, orange in set M2.</td></tr>
    <tr><td>A, B</td><td>The outputs, 10 V triggers (5 ms) or gates.</td></tr>
    <tr><td>RESET</td><td>All channels back to step 1. Patch Live's bar or song-start trigger here.</td></tr>
    <tr><td>OUTPUTS</td><td><b>trig</b>: 5 ms triggers. <b>gate</b>: each output stays high until the next clock, so two A steps in a row make one long gate. RAMP and RATCHET have their own gate behaviour (see their page).</td></tr>
    <tr><td>MUTE SET</td><td>Two complete sets of mutes, M1 and M2. The MUTE buttons change the set that's showing.</td></tr>
  </table>

  <div class="box">
  <h3>Mouse on the display</h3>
  <table style="margin:0">
    <tr><td>Click</td><td>next mode</td></tr>
    <tr><td>Shift-click</td><td>previous mode</td></tr>
    <tr><td>Ctrl-click (Cmd on Mac)</td><td>roll a new RANDOM pattern and a new TURING loop for this channel</td></tr>
    <tr><td>Right-click</td><td>mode list, "New random pattern", and "Chain to channel above"</td></tr>
  </table>
  </div>
  <p class="small">Good to know: after 3 seconds with no clock a channel goes back to step 1 by itself, so stopping and starting the transport lines everything up. Mode changes are undoable (Ctrl/Cmd-Z).</p>
</section>
""")

# ---- modes ------------------------------------------------------------------------
ca, cb = coin_demo(0.3, 4)
ca2, cb2 = coin_demo(0.7, 11)
da, db, la, lb = divide(3 / 14)
da2, db2, la2, lb2 = divide(0.5)
add(f"""
<section class="page">
  <h2>The modes, part 1</h2>
  <p>Every mode answers one question for each clock: does it go to <b style="color:{A_COL}">A</b>, to <b style="color:{B_COL}">B</b>, to both or to neither?
  The diagrams show 16 clocks; coloured cells are hits.</p>

  <h3>1 · COIN: a weighted coin toss</h3>
  <p>Each clock goes to A or B at random. AMOUNT tips the coin: full left always A, noon 50/50, full right always B.
  Nothing repeats, so this is the loosest mode. Put A on a closed hat and B on an open hat for hats that never settle.</p>
  <div class="two">{grid(ca, cb, label="AMOUNT at 30%:", note="mostly A")}{grid(ca2, cb2, label="AMOUNT at 70%:", note="mostly B")}</div>

  <h3>2 · DIVIDE: two clock dividers</h3>
  <p>A fires every <i>n</i>th clock and B every (16 − <i>n</i>)th. Noon is ÷8 and ÷8; turning left speeds A up and slows B down
  (÷4 and ÷12, ÷2 and ÷14...); turning right does the opposite. Both fire on the first clock. Odd pairs such as ÷5 and ÷11
  drift against the bar and only line up again after a long time.</p>
  <div class="two">{grid(da, db, label=f"÷{la} and ÷{lb}", note="(about 20%)")}{grid(da2, db2, label="÷8 and ÷8", note="(noon)")}</div>

  <h3>3 · TURING: a loop that can change itself</h3>
  <p>A 16-step loop of on and off steps: on steps go to A, off steps to B. The <b>left half</b> of the knob sets the loop length
  (1 to 16 steps) and <b>locks</b> it, so it repeats exactly. The <b>right half</b> keeps 16 steps and makes each step more and more
  likely to flip as it plays (up to 50%, fully random, at full right). Full left plays A on every clock.
  This is the Autechre-style "evolving" mode: let it run just right of noon until you hear something you like, then turn it
  left of noon to freeze it. The loop is saved with the patch; Ctrl/Cmd-click the display for a new one.</p>
  <div class="two">{grid(bits(0xA5C3)[:7] + [False]*9, [not x for x in bits(0xA5C3)[:7]] + [False]*9, label="locked, 7 steps", note="(repeats every 7)")}{grid(bits(0xA5C3), [not x for x in bits(0xA5C3)], label="16 steps", note="(right half: steps flip)")}</div>
</section>
""")

sa, sb, sla, slb = split(0.5)
sa2, sb2, sla2, slb2 = split(0.3)
e1 = classic(8, 3)
e2 = classic(16, 5)
add(f"""
<section class="page">
  <h2>The modes, part 2</h2>
  <h3>4 · SPLIT: Euclid split</h3>
  <p>The 16 steps are cut into two parts. A plays 3 hits spread as evenly as possible over the first part, then B plays 3 hits
  over the rest. At noon both parts are 8 steps, which gives the 3-3-2 "tresillo" feel on both. Turn the knob to move the cut:
  A over 6 and B over 10, A over 11 and B over 5, and so on. Good for call-and-response between two sounds.</p>
  <div class="two">{grid(sa, sb, label=f"A 3 over {sla}, B 3 over {slb}", note="(noon)")}{grid(sa2, sb2, label=f"A 3 over {sla2}, B 3 over {slb2}")}</div>

  <h3>5 · EUCLID: 32 classic rhythms</h3>
  <p>The knob picks one of 32 Euclidean rhythms, ordered from the shortest loop (1 in 4) to the longest (5 in 16). A plays the
  hits and B plays the gaps, so the two outputs always lock together. Full list on the last page.</p>
  <div class="two">{grid(*e1, label="3 in 8", note="(loop of 8)")}{grid(*e2, label="5 in 16")}</div>

  <h3>6 · RAMP: 16 hits that speed up or slow down</h3>
  <p>Each clock becomes 16 short hits fitted into the time until the next clock, alternating A, B, A, B. Full left speeds up
  (a bouncing-ball roll), noon speeds up then slows down, full right slows down. Use a slow clock (a bar or a beat) to get fills and
  rolls; send A and B to the same drum for all 16.</p>
  <div class="two">{labelled(ramp_svg(0.0), "full left: speeds up")}{labelled(ramp_svg(1.0), "full right: slows down")}</div>
  <p><b>In gate mode</b> RAMP plays one gate on A, then a gate on B that starts where A ends. The knob sets A's length on an
  exponential scale: 1/16 of the clock at full left, half at noon, 15/16 at full right. Great for timing two envelopes.</p>
  <div class="two">{labelled(gate_svg(0.125), "gate mode, left of noon")}{labelled(gate_svg(0.5), "gate mode, noon")}</div>
</section>
""")

r1, r2 = ratchet_svg(4), ratchet_svg(8)
pa = kits[0][16]
ra = [False, True, False, False, True, True, False, True, False, False, True, False, False, True, False, False]
add(f"""
<section class="page">
  <h2>The modes, part 3</h2>
  <h3>7 · RATCHET: bursts</h3>
  <p>Each clock becomes a burst: A gets <i>n</i> evenly spaced hits and B gets 16 − <i>n</i>, both fitted into one clock.
  Noon is 8 and 8; left is fewer on A and more on B. With a quarter-note clock, 4 on A and 12 on B is 16ths against triplet
  32nds, which makes good polyrhythmic rolls. In <b>gate mode</b> it works like RAMP's gate mode but with an even, finer scale.</p>
  <div class="two">{labelled(r1, "A ×4, B ×12")}{labelled(r2, "A ×8, B ×8 (noon)")}</div>

  <h3>8 · PATTERN: drum patterns</h3>
  <p>24 four-bar patterns per channel, played from a <b>16th-note clock</b> (patch RESET so bar 1 lands where you want it).
  Each channel has its own kit:</p>
  <table>
    <tr><th>Channel</th><th>A</th><th>B</th></tr>
    <tr><td>1</td><td>kick</td><td>snare / clap</td></tr>
    <tr><td>2</td><td>closed hat</td><td>open hat</td></tr>
    <tr><td>3</td><td>low percussion</td><td>high percussion</td></tr>
    <tr><td>4</td><td>ride / shaker</td><td>accents and crashes</td></tr>
  </table>
  <p>The knob picks the pattern (the name is on the display; the full list is on the last page). Patterns run from Detroit and
  electro to breaks, dub, footwork, and some broken "AE" ones, and several use 3s, 5s and 7s that drift across the bar.
  The ring shows the bar that is playing.</p>
  {grid(bits(pa[1]), bits(pa[2]), label="channel 1, “ae limp”", note="bar 1")}

  <h3>9 · RANDOM: <i>n</i> hits in random places</h3>
  <p>The knob sets how many of the 16 steps hit (0 to 16); where they fall is random. Hits go to A, empty steps to B.
  The pattern loops until you change it. Turning the knob adds or takes away <b>one hit at a time</b>, so the rest of
  the pattern stays put: turn it slowly to build a part up or strip it down. Ctrl/Cmd-click the display to roll a new pattern
  with the same number of hits. The pattern is saved with the patch.</p>
  {grid(ra, [not x for x in ra], label="6 of 16")}

  <h3>10 · STICKY: a coin with memory</h3>
  <p>Like COIN, but the knob sets how likely each hit is to go to the <b>same</b> output as the last one. Full left always swaps
  (A, B, A, B), noon is a plain coin toss, and to the right you get longer and longer runs on one side, then a switch.
  The result is runs and clusters rather than an even scatter.</p>
  <div class="two">{grid(*sticky_demo(0.5, 3), label="noon")}{grid(*sticky_demo(0.85, 8), label="85%:", note="long runs")}</div>
</section>
""")

add(f"""
<section class="page">
  <h2>Chain: longer loops from short ones</h2>
  <p>Right-click a channel's display and tick <b>Chain to channel above</b> (or use the module's right-click menu). Chained
  channels play <b>one after another</b>: channel 1 plays its loop once, then channel 2 plays its loop, and so on, then back to
  channel 1. Each channel keeps its own mode and setting, so one long phrase can be made of different kinds of part.</p>
  <ul>
    <li>A channel's loop is 16 clocks, or its TURING length or EUCLID loop length when that's shorter. A TURING loop of 7
    then a RANDOM 16 makes a 23-step phrase that drifts against the bar.</li>
    <li>The whole chain runs on the <b>top</b> channel's clock.</li>
    <li>The top channel's <b>A and B carry the whole chain</b>. The other channels' A and B still carry only their own turn, so you
    can play the whole phrase on one drum or give each part its own sound.</li>
    <li>A bar on the left edge of the displays joins chained channels; a channel's display dims while it waits for its turn.</li>
    <li>RESET, or 3 seconds without a clock, takes the chain back to its first channel, step 1.</li>
  </ul>
  <div class="box">
    <h3>Example: a 48-step hat phrase</h3>
    <p>Channel 1 DIVIDE at ÷2 (16 clocks), channel 2 RANDOM with 9 hits, channel 3 TURING unlocked a little right of noon.
    Chain 2 to 1 and 3 to 2. Take channel 1's A to the closed hat and B to the open hat. You hear a steady bar, then a fixed random
    bar, then a bar that keeps changing: three bars of structure, one of variation.</p>
  </div>

  <h2 style="margin-top:6mm">Arranging with Divskip</h2>
  <p>Loop fatigue usually comes from every part repeating at the same length. Divskip has four ways out:</p>
  <ol>
    <li><b>Mute sets as sections.</b> Set up M1 as the "verse" (say, hats and perc muted) and M2 as the "drop", then flip the switch on
    the bar line. Map the switch to a MIDI controller with VCV's MIDI-Map to play it from Live.</li>
    <li><b>Odd lengths.</b> TURING lengths of 5, 7 or 11, DIVIDE pairs like ÷5/÷11, and the 3s, 5s and 7s patterns all drift against
    the bar, so the parts line up in a new way each time round.</li>
    <li><b>Slow CV on AMOUNT.</b> A very slow LFO (one cycle every 32 or 64 bars) into CV on a RANDOM channel adds and removes hits
    gradually: the part builds up and strips down by itself.</li>
    <li><b>Sequenced modes.</b> A 4-step sequencer clocked once a bar into MODE (0 V, 0 V, 0 V, 2 V) jumps to a fill mode on bar 4.</li>
  </ol>
</section>
""")

add(f"""
<section class="page">
  <h2>Recipes</h2>
  <div class="box">
  <h3>1 · Detroit drums from one clock</h3>
  <p>16th clock into IN 1. Channel 1 PATTERN "detroit push" (kick, clap). Channel 2 PATTERN "detroit 16ths" (closed, open hat).
  Channel 3 EUCLID 3 in 8 → A to a rim, B unused. Channel 4 COIN at 20% → A to a shaker, B to a tambourine.
  Patch RESET from Live's bar-1 trigger. Variation: set channel 3 to SPLIT and turn it slowly for congas that answer each other.</p>
  </div>
  <div class="box">
  <h3>2 · Self-mutating Autechre-style percussion</h3>
  <p>16th clock. Channel 1 TURING just right of noon (flip about 5%): A to a metallic FM hit, B to a short noise click.
  Channel 2 STICKY at 85% into two pitches of the same percussion voice. Channel 3 RANDOM with 5 hits, a slow random LFO
  (Random module, 0.05 Hz, ±1 V) into CV. When channel 1 finds something good, turn it left of noon to lock it.
  Variation: chain 2 to 1 so the clusters only answer every other bar.</p>
  </div>
  <div class="box">
  <h3>3 · Rolls and fills</h3>
  <p>Take a quarter-note clock (or the bar clock) into channel 4's IN. RAMP, full left, both outputs into one snare through a
  mixer: a roll that speeds up into each beat. Mute it in M1 and unmute it in M2; flip to M2 for the last bar of a phrase.
  Variation: RATCHET with A ×3 and B ×4 into two toms.</p>
  </div>
  <div class="box">
  <h3>4 · Gates for envelopes and the rig</h3>
  <p>OUTPUTS to gate. Channel 1 RAMP: A and B gates into two envelopes (or two Just Friends channels on the hardware side), the knob
  slides the hand-over point between them; CV it from an LFO for a breathing pad. Sending A/B to the modular: patch them to the
  disting NT's outputs through VCV's Audio module, the same way as Live's clock goes the other way.</p>
  </div>
  <p class="small">Patch hints only, not .vcv files: tell me if you want any of these made into a ready-to-open patch.</p>
</section>
""")

# reference page
rows = []
for i in range(24):
    rows.append("<tr>" + "".join(f"<td>{html.escape(kits[k][i][0])}</td>" for k in range(4)) + f"<td class='n'>{i + 1}</td></tr>")
cl = ", ".join(f"{k} in {n}" for n, k in CLASSIC)
add(f"""
<section class="page">
  <h2>Reference</h2>
  <h3>PATTERN mode: the 24 patterns per channel</h3>
  <p class="small">Knob from left (1) to right (24). Names as shown on the display.</p>
  <table class="pat">
    <tr><th>Ch 1 kick / snare</th><th>Ch 2 closed / open hat</th><th>Ch 3 low / high perc</th><th>Ch 4 ride / accents</th><th>#</th></tr>
    {''.join(rows)}
  </table>
  <h3>EUCLID mode: the 32 rhythms, left to right</h3>
  <p class="small">{cl}</p>
  <h3>What AMOUNT does, at a glance</h3>
  <table>
    <tr><th>Mode</th><th>Full left</th><th>Noon</th><th>Full right</th></tr>
    <tr><td>COIN</td><td>always A</td><td>50/50</td><td>always B</td></tr>
    <tr><td>DIVIDE</td><td>A ÷1, B ÷15</td><td>÷8 and ÷8</td><td>A ÷15, B ÷1</td></tr>
    <tr><td>TURING</td><td>always A</td><td>16 steps, locked</td><td>16 steps, fully random</td></tr>
    <tr><td>SPLIT</td><td>A 3/3, B 3/13</td><td>3/8 and 3/8</td><td>A 3/13, B 3/3</td></tr>
    <tr><td>EUCLID</td><td>1 in 4</td><td>1 in 9</td><td>5 in 16</td></tr>
    <tr><td>RAMP</td><td>speeds up</td><td>up, then down</td><td>slows down</td></tr>
    <tr><td>RATCHET</td><td>A ×1, B ×15</td><td>×8 and ×8</td><td>A ×15, B ×1</td></tr>
    <tr><td>PATTERN</td><td>pattern 1</td><td>pattern 13</td><td>pattern 24</td></tr>
    <tr><td>RANDOM</td><td>no hits (all B)</td><td>8 of 16</td><td>all 16 (all A)</td></tr>
    <tr><td>STICKY</td><td>always swaps</td><td>coin toss</td><td>long runs</td></tr>
  </table>
</section>
""")

CSS = open(os.path.join(HERE, "guide.css")).read()
doc = f"<!doctype html><html><head><meta charset='utf-8'><title>Divskip user guide</title><style>{CSS}</style></head><body>{''.join(P)}</body></html>"
build = os.path.join(HERE, "build")
os.makedirs(build, exist_ok=True)
open(os.path.join(build, "guide.html"), "w").write(doc)
import shutil
shutil.copy(os.path.join(ROOT, "panel-preview.png"), os.path.join(build, "panel.png"))
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage();
await p.goto('file://{os.path.join(build, "guide.html")}'); await p.waitForTimeout(400);
await p.pdf({{path: {json.dumps(OUT)}, format: 'A4', printBackground: true, preferCSSPageSize: true}});
await b.close(); }})();"""
open(os.path.join(build, "pdf.js"), "w").write(js)
subprocess.run(["node", os.path.join(build, "pdf.js")], check=True, cwd="/tmp")
print("wrote", OUT)
