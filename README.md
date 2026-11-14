# Divskip

A VCV Rack module by Haer (24 HP). Four channels. Each one takes a clock (or
any triggers) and splits it between two outputs, **A** and **B**: the hits that
play and the hits that are skipped. Inspired by the DivSkip Eurorack module by
DivKid and Making Sound Machines; this is an independent VCV version with its
own panel, drum patterns and a few extras.

![panel](panel-preview.png)

## Each channel

| Part | What it does |
|---|---|
| **IN** | Clock or triggers. An empty IN uses the channel above, so one clock in channel 1 drives all four. |
| **MODE** (jack) | Moves the mode on: each +1 V is one mode further (wraps round). Good for a sequencer. |
| **CV** (jack) | ±5 V, added to the AMOUNT knob (the knob spans 0 to 5 V). |
| **AMOUNT** | What it does depends on the mode (below). The light ring shows the steps: A steps in the mode's colour, B steps cyan, playhead white. |
| **Display** | Mode and setting. **Click** = next mode, **Shift-click** = previous, **Ctrl/Cmd-click** = roll a new random pattern / Turing loop, **right-click** = mode list. |
| **MUTE** | Silences A and B (the channel keeps counting). |
| **A / B** | The two outputs, 10 V. |

## Modes

| # | Mode | AMOUNT knob |
|---|---|---|
| 1 | **COIN** | Each hit goes to A or B at random. Left = always A, noon = 50/50, right = always B. |
| 2 | **DIVIDE** | Clock divider on A and B, lengths add up to 16. Noon = /8 and /8, left = /1 and /15, right = /15 and /1. |
| 3 | **TURING** | A looping random pattern (set steps → A, unset → B). Left half: loop length 1–16, locked. Right half: 16 steps, more and more likely to change. Full left = always A. |
| 4 | **SPLIT** | Euclid split: A plays 3 hits spread over its length, then B plays 3 over the rest of the 16. Noon = 3:3:2 on both. |
| 5 | **EUCLID** | 32 classic Euclid rhythms, from 1 in 4 up to 5 in 16. A plays the hits, B the gaps. |
| 6 | **RAMP** | Each clock becomes 16 hits (alternating A, B) that speed up (left), speed up then slow (noon) or slow down (right). In gate mode: a gate on A, then one on B, length set exponentially. |
| 7 | **RATCHET** | Each clock becomes a burst: A gets *n*, B gets 16 − *n* evenly spaced hits. In gate mode: A/B gate lengths in fine, even steps. |
| 8 | **PATTERN** | 24 four-bar drum patterns per channel, from a 16th-note clock. Ch 1 kick/snare, ch 2 closed/open hat, ch 3 low/high perc, ch 4 ride/accents. |
| 9 | **RANDOM** | The knob sets how many of 16 steps hit (0–16); the hits sit in random places. Turning it adds or removes one hit at a time; Ctrl/Cmd-click rolls a new pattern. |
| 10 | **STICKY** | Sticky coin: how likely each hit is to go to the same output as the last. Left = swaps every hit (A B A B), noon = plain coin toss, right = long runs on one side. |

RAMP and RATCHET fit their hits into the time between your last two clocks.

## Chain

Right-click a channel's display (or the module) and tick **Chain to channel above**.
Chained channels play one after another as one long loop: channel 1 plays its loop
once, then channel 2 plays its loop, and so on, then back to channel 1. Each
channel keeps its own mode and setting, so a chain can be 16 RANDOM steps then a
TURING loop of 7, for instance. A loop is 16 clocks, or the Turing / Euclid
length when that's shorter.

- The whole chain runs on the top channel's clock.
- The **top channel's A and B send the whole chain**. Each other channel's A and B
  send only its own turn, so you can still give each part its own sound.
- A display linked by a bar on its left is chained; it dims while waiting its turn.

## Global

- **OUTPUTS**: *trig* sends 5 ms triggers. *gate* holds each output high until the next clock, so two A steps in a row give one long gate.
- **MUTE SET**: two sets of mutes, M1 (cyan) and M2 (orange). Flip the switch to jump between two mute scenes.
- **RESET**: all channels back to step 1. A channel also goes back to step 1 by itself after 3 seconds without a clock.

## Building

```
python3 tools/make_patterns.py   # drum patterns -> src/DivskipPatterns.hpp
python3 res/make_panel.py        # panel -> res/Divskip.svg (needs fonttools + Inter)
c++ -std=c++11 -O2 -Wall -Wextra -o test/engine_test test/engine_test.cpp && test/engine_test
make dist RACK_DIR=...           # or let GitHub Actions build it
```

The version stays 2.x: Rack 2 skips plugins numbered 3.x.
