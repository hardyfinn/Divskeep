#!/usr/bin/env python3
# Writes src/DivskipPatterns.hpp: the drum patterns for PATTERN mode.
#
# Each channel has its own kit:
#   channel 1: A = kick,        B = snare / clap
#   channel 2: A = closed hat,  B = open hat
#   channel 3: A = low perc,    B = high perc
#   channel 4: A = ride/shaker, B = accent / crash-ish
# 24 patterns per channel, 64 steps each (4 bars of 16ths).
#
# A pattern is written as 16-step bars: "x" = hit, "." = rest.
# Give one bar (repeated 4 times), two bars (played twice), or four bars.
# A "fill" replaces bar 4 only. every(n, offset) and euclid(k, n) make
# patterns that run across all 64 steps, so odd cycles drift against the bar.
#   python3 tools/make_patterns.py
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def every(n, offset=0):
    return "".join("x" if i >= offset and (i - offset) % n == 0 else "." for i in range(64))


def euclid(k, n, rot=0):
    bar = "".join("x" if (i * k) % n < k else "." for i in range(n))
    bar = bar[rot:] + bar[:rot]
    return (bar * (64 // n + 1))[:64]


def expand(p, fill=None):
    p = p.replace(" ", "")
    if len(p) == 16:
        s = p * 4
    elif len(p) == 32:
        s = p * 2
    elif len(p) == 64:
        s = p
    else:
        raise ValueError(f"pattern length {len(p)}: {p}")
    if fill:
        fill = fill.replace(" ", "")
        assert len(fill) == 16, fill
        s = s[:48] + fill
    assert set(s) <= set("x."), s
    return s


# (name, A, A fill or None, B, B fill or None)
KICK_SNARE = [
    ("four floor",     "x...x...x...x...", None,               "....x.......x...", "....x.......x.xx"),
    ("detroit push",   "x...x...x...x..x", None,               "....x.......x...", "....x.....x.x..x"),
    ("motor city",     "x...x...x...x...", "x...x...x.x.x.x.", "....x..x....x...", None),
    ("electro",        "x......x..x.....", None,               "....x.......x...", "....x.......xxxx"),
    ("electro two",    "x.....x.x.....x.", None,               "....x.......x...", None),
    ("breakbeat",      "x.........x..... x.x.......x.....", None, "....x.......x..x ....x.......x...", None),
    ("half time",      "x.........x.....", None,               "........x.......", "........x.....xx"),
    ("two step",       "x.........x...x.", None,               "....x.......x...", None),
    ("broken techno",  "x..x..x...x..x..", None,               "....x.......x...", None),
    ("dub techno",     "x...x...x...x...", None,               "......x.......x.", None),
    ("boom bap",       "x.......x.x..... x..x....x.x..x..", None, "....x.......x...", None),
    ("funk",           "x.x...x...x..x..", None,               "....x..x.x..x..x", None),
    ("one drop",       "........x.......", None,               "........x.......", "........x...x.xx"),
    ("steppers",       "x...x...x...x...", None,               "........x.......", None),
    ("jungle",         "x.........x..... ..x.......x.....", None, "....x..x.x..x... ....x.......x..x", None),
    ("footwork",       "x..x..x.x..x..x.", None,               "............x...", None),
    ("ae limp",        "x..x.x....x..x.x x....x.x..x.....", None, "...x....x..x.... .....x..x...x...", None),
    ("sparse",         "x...............", None,               "........x.......", None),
    ("gallop",         "x...x...x...x.x.", None,               "....x.......x.x.", None),
    ("ghetto tech",    "x.x.x.x.x.x.x.x.", None,               "....x.......x...", None),
    ("shuffle",        "x...x..xx...x...", None,               "....x.......x...", None),
    ("industrial",     "x.x.x...x.x.x...", None,               "....x.......x...", "....x...x.x.xxxx"),
    ("kick in 3s",     every(3),            None,               "....x.......x...", None),
    ("5 over 7",       every(5),            None,               every(7, 4),        None),
]

HATS = [
    ("offbeat open",   "x...x...x...x...", None,               "..x...x...x...x.", None),
    ("16ths + open",   "xx.xxx.xxx.xxx.x", None,               "..x...x...x...x.", None),
    ("8ths",           "x.x.x.x.x.x.x.x.", None,               "................", "..............x."),
    ("16ths",          "xxxxxxxxxxxxxxxx", None,               "................", None),
    ("offbeat 8ths",   "..x...x...x...x.", None,               "................", None),
    ("detroit 16ths",  "xx.xxx.xxx.xxx.x", "xx.xxx.xxx.xx.xx", "..x...x...x...x.", "..x...x...x.x.x."),
    ("skippy",         "x.xx.xx.x.xx.x.x", None,               "...........x....", None),
    ("open on 4",      "x.x.x.x.x.x.x.x.", None,               "..............x.", None),
    ("triplet feel",   every(3),            None,               every(6, 5),        None),
    ("garage",         ".xx..xx..xx..xx.", None,               "..x.......x.....", None),
    ("funk 16ths",     "xxxx.xxxxxx.xx.x", None,               "....x.......x...", None),
    ("sparse ticks",   "x.......x.......", None,               "......x.......x.", None),
    ("reggae",         "..x...x...x...x.", None,               "......x.......x.", None),
    ("electro",        "x.x.x.x.x.x.xxx.", None,               "..x.......x.....", None),
    ("ae dust",        "x..x.xx..x...x.x .x..x...xx..x...", None, "..........x..... ....x.........x.", None),
    ("rolling",        "xxx.xxx.xxx.xxx.", None,               "...x...x...x...x", None),
    ("gate open",      "x...............", None,               "..x...x...x...x.", None),
    ("5s",             every(5),            None,               every(10, 7),       None),
    ("7 in 16",        euclid(7, 16),       None,               euclid(2, 16, 3),   None),
    ("9 in 16",        euclid(9, 16),       None,               euclid(3, 16, 2),   None),
    ("11 in 16",       euclid(11, 16),      None,               "..x.......x.....", None),
    ("breaks",         "x.x.x.x.x.x.x.x. x.x.x.x.x.x.x.xx", None, "..............x. ......x.......x.", None),
    ("ride bell",      "x.x.x.x.x.x.x.x.", None,               "x...............", None),
    ("silence + open", "................", None,               "..x...x...x...x.", None),
]

PERC = [
    ("conga 3:3:2",    euclid(3, 8),        None,               euclid(2, 8, 2),    None),
    ("tom roll",       "x.....x.........", "x.....x...x.x.xx", "...x......x.....", None),
    ("cowbell",        "......x.......x.", None,               "..x...x...x..x..", None),
    ("clave son",      "x..x..x...x.x...", None,               "................", None),
    ("rumba",          "x..x...x..x.x...", None,               "....x.......x...", None),
    ("tresillo",       euclid(3, 8),        None,               "................", None),
    ("5 in 16",        euclid(5, 16),       None,               euclid(3, 16, 4),   None),
    ("7 in 16",        euclid(7, 16),       None,               euclid(4, 16, 2),   None),
    ("bongo",          "x.x...x.x.x...x.", None,               ".x...x.x.x...x..", None),
    ("rim shots",      "...x.....x....x.", None,               ".x......x.......", None),
    ("detroit perc",   "......x.......x.", None,               "...x.......x....", None),
    ("electro zaps",   "x.......x.x.....", None,               "...x......x...x.", None),
    ("ae clatter",     "x..x.x.......x.. ...x..x.x.x.....", None, ".x.......x.x...x ......x...x..x..", None),
    ("3 against 4",    every(3),            None,               every(4, 2),        None),
    ("5 against 4",    every(5),            None,               every(4, 3),        None),
    ("7 against 16",   every(7),            None,               every(9, 4),        None),
    ("dub echoes",     "x.....x.....x...", None,               "....x.....x.....", None),
    ("afro 12/8 feel", euclid(5, 12),       None,               euclid(7, 12, 1),   None),
    ("bembe",          euclid(7, 12),       None,               "................", None),
    ("sparse low",     "x...............", None,               "..........x.....", None),
    ("call + answer",  "x.x.x........... ................", None, "................ x.x.x...........", None),
    ("gamelan 4",      "x...x...x...x...", None,               "..x...x..x....x.", None),
    ("shaker drive",   "................", None,               "xxxxxxxxxxxxxxxx", None),
    ("13 in 16",       euclid(13, 16),      None,               euclid(3, 16),      None),
]

RIDE = [
    ("ride 8ths",      "x.x.x.x.x.x.x.x.", None,               "x...............", None),
    ("ride 4s",        "x...x...x...x...", None,               "................", "............x..."),
    ("shaker 16ths",   "xxxxxxxxxxxxxxxx", None,               "x...x...x...x...", None),
    ("offbeat ride",   "..x...x...x...x.", None,               "................", None),
    ("jazz ride",      "x...x.x.x...x.x.", None,               "................", None),
    ("tambourine",     "....x.......x...", None,               "..............x.", None),
    ("accents 3",      "x.x.x.x.x.x.x.x.", None,               every(3),           None),
    ("crash bar 1",    "x.x.x.x.x.x.x.x.", None,               "x............... ................", None),
    ("crash bar 4",    "x...x...x...x...", None,               "................", "x..............."),
    ("16ths accents",  "xxxxxxxxxxxxxxxx", None,               "x..x..x...x..x..", None),
    ("skip ride",      "x.xx.x.xx.x.x.xx", None,               "................", None),
    ("ae glitch",      "x..x.x..xx...x.. .x...xx..x.x....", None, "......x......... .............x..", None),
    ("ticks 5",        every(5),            None,               every(15),          None),
    ("ticks 7",        every(7),            None,               "x...............", None),
    ("7 in 16",        euclid(7, 16),       None,               euclid(2, 16),      None),
    ("9 in 16",        euclid(9, 16, 1),    None,               "........x.......", None),
    ("garage shuffle", "x.xx.xx.x.xx.xx.", None,               "................", None),
    ("detroit ride",   "..x...x...x...x.", None,               "x...............", None),
    ("cymbal swell",   "................", None,               "x.......x.......", None),
    ("half bar",       "x.x.x.x.........", None,               "........x.x.x.x.", None),
    ("triplets",       every(3),            None,               every(12),          None),
    ("dub delay",      "x.....x.....x...", None,               "......x.........", None),
    ("busy shaker",    "xx.xxx.xxx.xxx.x", None,               "..x.......x.....", None),
    ("silence",        "................", None,               "................", None),
]

KITS = [("KICK_SNARE", KICK_SNARE), ("HATS", HATS), ("PERC", PERC), ("RIDE", RIDE)]


def mask(s):
    v = 0
    for i, c in enumerate(s):
        if c == "x":
            v |= 1 << i
    return v


out = [
    "// Generated by tools/make_patterns.py. Do not edit by hand.",
    "// Bit i of a mask = step i (0-63) plays.",
    "#pragma once",
    "#include <cstdint>",
    "",
    "namespace divskip {",
    "",
    "static const int PATTERN_COUNT = 24;",
    "static const int PATTERN_STEPS = 64;",
    "",
    "struct DrumPattern { const char* name; uint64_t a, b; };",
    "",
    "// [channel][pattern]",
    "static const DrumPattern PATTERNS[4][24] = {",
]
for kit, pats in KITS:
    assert len(pats) == 24, (kit, len(pats))
    out.append(f"\t{{ // {kit}")
    for (name, a, af, b, bf) in pats:
        A, B = expand(a, af), expand(b, bf)
        out.append(f'\t\t{{"{name}", 0x{mask(A):016X}ull, 0x{mask(B):016X}ull}},')
    out.append("\t},")
out += ["};", "", "} // namespace divskip", ""]
open(os.path.join(HERE, "..", "src", "DivskipPatterns.hpp"), "w").write("\n".join(out))
print("ok")
