// DivskipEngine.hpp
// The logic of one Divskip channel, with no VCV Rack code in it, so it can be
// tested on its own (test/engine_test.cpp).
//
// Every channel takes a clock (or any triggers) and splits it between two
// outputs, A and B. What goes where depends on the mode and on the amount
// (the knob plus its CV, 0..1).
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "DivskipPatterns.hpp"

namespace divskip {

enum Mode { COIN, DIVIDE, TURING, SPLIT, EUCLID, RAMP, RATCHET, PATTERN, RANDOM, STICKY, MODE_COUNT };

static const char* const MODE_NAMES[MODE_COUNT] = {
	"COIN", "DIVIDE", "TURING", "SPLIT", "EUCLID", "RAMP", "RATCHET", "PATTERN", "RANDOM", "STICKY"};
static const char* const MODE_LONG_NAMES[MODE_COUNT] = {
	"Coin toss (random A or B)", "Clock divider", "Turing loop", "Euclid split",
	"Euclid classic", "Ramp (16 hits speeding up or slowing down)", "Ratchet bursts", "Drum patterns",
	"Random steps (the dial sets how many)",
	"Sticky coin (runs and clusters)"};

static const float TRIG_TIME = 0.005f;   // 5 ms output triggers
static const float TIMEOUT = 3.f;        // no clock for 3 s: back to step 1

// Euclid classic: the 32 patterns, shortest loop first (loop length, hits)
static const int CLASSIC[32][2] = {
	{4, 1}, {4, 2}, {4, 3}, {5, 1}, {5, 2}, {5, 3}, {6, 1}, {6, 2}, {6, 3}, {6, 5},
	{7, 1}, {7, 2}, {7, 3}, {8, 1}, {8, 3}, {8, 5}, {9, 1}, {9, 4}, {10, 3}, {10, 7},
	{11, 3}, {11, 4}, {12, 5}, {12, 7}, {13, 3}, {13, 4}, {14, 3}, {14, 5}, {15, 4}, {15, 7},
	{16, 3}, {16, 5}};

// k hits spread as evenly as possible over n steps, first step always a hit
inline bool euclidHit(int k, int n, int i) {
	if (n <= 0) return false;
	return ((i * k) % n) < k;
}

inline int roundi(float x) { return (int) std::floor(x + 0.5f); }
inline float clamp01(float x) { return std::min(1.f, std::max(0.f, x)); }

// What the amount means in each mode (shared with the display)
inline int divideA(float p) { return 1 + roundi(clamp01(p) * 14.f); }          // 1..15, B = 16 - A
inline int splitA(float p) { return 3 + roundi(clamp01(p) * 10.f); }           // 3..13, B = 16 - A
inline int ratchetA(float p) { return 1 + roundi(clamp01(p) * 14.f); }         // 1..15, B = 16 - A
inline int classicIndex(float p) { return std::min(31, (int) (clamp01(p) * 32.f)); }
inline int patternIndex(float p) { return std::min(PATTERN_COUNT - 1, (int) (clamp01(p) * PATTERN_COUNT)); }
// Turing: left half sets the loop length (locked), right half the chance a step flips
inline int turingLength(float p) { return p >= 0.5f ? 16 : 1 + roundi(clamp01(p) * 2.f * 15.f); }
inline float turingFlip(float p) { return p > 0.5f ? (clamp01(p) - 0.5f) : 0.f; }   // 0..0.5
inline bool turingAlwaysOn(float p) { return p < 0.02f; }
inline int randomHits(float p) { return roundi(clamp01(p) * 16.f); }   // 0..16 of 16 steps
// STICKY: chance that a hit goes to the same output as the last one.
// Left = always swaps (A B A B), noon = plain coin toss, right = long runs.
inline float stickyRepeat(float p) { return std::min(0.97f, clamp01(p)); }
inline int popcount16(uint16_t v) { int n = 0; for (; v; v &= v - 1) n++; return n; }
inline float turingLengthP(int L) { return (L - 1) / 30.f; }   // knob position for a loop of L

// Ramp shape: where hit k of 16 lands, as a fraction of the clock period.
// p = 0 speeds up, 0.5 speeds up then slows down, 1 slows down.
inline float rampTime(float p, float x) {
	float accel = 1.f - (1.f - x) * (1.f - x);
	float decel = x * x;
	float c = x - 0.5f;
	float both = 0.3f * x + 0.7f * (4.f * c * c * c + 0.5f);
	float s = (clamp01(p) - 0.5f) * 2.f;
	if (s < 0.f) return both + (accel - both) * (-s);
	return both + (decel - both) * s;
}

// Gate length of A as a fraction of the clock period.
// RAMP (coarse): exponential, 1/16 .. 1/2 .. 15/16. RATCHET (fine): linear 1/16 .. 15/16.
inline float gateFraction(int mode, float p) {
	p = clamp01(p);
	if (mode == RATCHET) return (1.f + p * 14.f) / 16.f;
	if (p < 0.5f) return 0.5f * std::pow(2.f, -(0.5f - p) * 2.f * 3.f);
	return 1.f - 0.5f * std::pow(2.f, -(p - 0.5f) * 2.f * 3.f);
}

struct Rng {
	uint32_t s = 0x9E3779B9u;
	uint32_t next() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return s;
	}
	float uniform() { return (next() >> 8) * (1.f / 16777216.f); }
};

struct Channel {
	int kit = 0;            // which drum kit PATTERN mode uses (0-3 = channel)
	Rng rng;

	// step counters
	int divA = 0, divB = 0; // DIVIDE
	int split = 0;          // SPLIT, 0..15
	int classic = 0;        // EUCLID
	int pattern = 0;        // PATTERN, 0..63
	uint16_t reg = 0xA5C3;  // TURING shift register (saved with the patch)
	int turPos = 0;
	uint16_t rmask = 0x0101; // RANDOM: which of the 16 steps hit (saved with the patch)
	int rpos = 0;
	bool stickB = false;     // STICKY: where the last hit went
	bool frozen = false;     // chained: the chain does the 3 s auto-reset instead
	float fixedPeriod = 0.f; // chained: clock period measured by the chain (0 = measure here)

	// outputs
	bool levelA = false, levelB = false;  // held gates (GATE outputs, step modes)
	float pulseA = 0.f, pulseB = 0.f;     // trigger time left
	bool lastA = false, lastB = false;    // where the last step went (display)

	// timed modes (RAMP, RATCHET)
	bool burst = false, window = false;
	float t = 0.f;
	float evA[16], evB[16];
	int nA = 0, nB = 0, iA = 0, iB = 0;
	float pw = TRIG_TIME;
	float gA1 = 0.f, gB0 = 0.f, gB1 = 0.f;

	// clock
	float period = 0.25f;   // last measured time between clocks
	float since = 1e9f;
	bool clocked = false, timedOut = false;
	int lastMode = -1;
	bool lastGate = false;

	void seed(uint32_t s) {
		rng.s = s ? s : 1u;
		reg = (uint16_t) (rng.next() & 0xFFFF);
	}

	// back to step 1 (RESET input, or 3 s without a clock)
	void restart() {
		divA = divB = split = classic = pattern = turPos = rpos = 0;
		burst = window = false;
		levelA = levelB = false;
	}

	// how many steps the current loop has, and where the playhead is (display)
	int loopLength(int mode, float p) const {
		switch (mode) {
			case DIVIDE: return 16;
			case TURING: return turingLength(p);
			case SPLIT: return 16;
			case EUCLID: return CLASSIC[classicIndex(p)][0];
			case PATTERN: return 16;
			default: return 16;
		}
	}

	// the 16 ring lights: which steps go to A, which to B, and the playhead.
	// Returns false when the mode has no step pattern to show.
	bool ring(int mode, float p, bool a[16], bool b[16], int& play) const {
		for (int i = 0; i < 16; i++) a[i] = b[i] = false;
		play = -1;
		switch (mode) {
			case DIVIDE: {
				int la = divideA(p), lb = 16 - la;
				// show the next 16 clocks from the start of A's cycle
				for (int i = 0; i < 16; i++) {
					a[i] = (i % la) == 0;
					b[i] = (i % lb) == 0;
				}
				play = (divA + 15) % la;
				return true;
			}
			case TURING: {
				int L = turingLength(p);
				for (int i = 0; i < L; i++) {
					bool bit = turingAlwaysOn(p) || ((reg >> i) & 1);
					a[i] = bit;
					b[i] = !bit;
				}
				play = (turPos + L - 1) % L;
				return true;
			}
			case SPLIT: {
				int la = splitA(p), lb = 16 - la;
				for (int i = 0; i < 16; i++) {
					if (i < la) a[i] = euclidHit(3, la, i);
					else b[i] = euclidHit(3, lb, i - la);
				}
				play = (split + 15) % 16;
				return true;
			}
			case EUCLID: {
				const int* c = CLASSIC[classicIndex(p)];
				for (int i = 0; i < c[0]; i++) {
					a[i] = euclidHit(c[1], c[0], i);
					b[i] = !a[i];
				}
				play = (classic + c[0] - 1) % c[0];
				return true;
			}
			case RANDOM: {
				for (int i = 0; i < 16; i++) {
					a[i] = (rmask >> i) & 1;
					b[i] = !a[i];
				}
				play = (rpos + 15) % 16;
				return true;
			}
			case PATTERN: {
				const DrumPattern& d = PATTERNS[kit & 3][patternIndex(p)];
				int cur = (pattern + 63) % 64;
				int bar = cur / 16;
				for (int i = 0; i < 16; i++) {
					a[i] = (d.a >> (bar * 16 + i)) & 1;
					b[i] = (d.b >> (bar * 16 + i)) & 1;
				}
				play = cur % 16;
				return true;
			}
			default: return false;
		}
	}

	// one clock: decide where it goes (step modes) or start a burst (timed modes)
	void step(int mode, float p, bool gateOut) {
		bool fa = false, fb = false;
		switch (mode) {
			case COIN: {
				fb = rng.uniform() < clamp01(p);
				fa = !fb;
			} break;
			case DIVIDE: {
				int la = divideA(p), lb = 16 - la;
				if (divA >= la) divA = 0;
				if (divB >= lb) divB = 0;
				fa = divA == 0;
				fb = divB == 0;
				divA = (divA + 1) % la;
				divB = (divB + 1) % lb;
			} break;
			case TURING: {
				int L = turingLength(p);
				if (turPos >= L) turPos = 0;
				bool bit = (reg >> turPos) & 1;
				if (rng.uniform() < turingFlip(p)) {
					bit = !bit;
					reg ^= (uint16_t) (1u << turPos);
				}
				if (turingAlwaysOn(p)) bit = true;
				fa = bit;
				fb = !bit;
				turPos = (turPos + 1) % L;
			} break;
			case SPLIT: {
				int la = splitA(p), lb = 16 - la;
				if (split < la) fa = euclidHit(3, la, split);
				else fb = euclidHit(3, lb, split - la);
				split = (split + 1) % 16;
			} break;
			case EUCLID: {
				const int* c = CLASSIC[classicIndex(p)];
				if (classic >= c[0]) classic = 0;
				fa = euclidHit(c[1], c[0], classic);
				fb = !fa;
				classic = (classic + 1) % c[0];
			} break;
			case PATTERN: {
				const DrumPattern& d = PATTERNS[kit & 3][patternIndex(p)];
				fa = (d.a >> pattern) & 1;
				fb = (d.b >> pattern) & 1;
				pattern = (pattern + 1) % 64;
			} break;
			case STICKY: {
				if (rng.uniform() >= stickyRepeat(p)) stickB = !stickB;
				fb = stickB;
				fa = !fb;
			} break;
			case RANDOM: {
				fitRandom(randomHits(p));
				fa = (rmask >> rpos) & 1;
				fb = !fa;
				rpos = (rpos + 1) % 16;
			} break;
			case RAMP:
			case RATCHET: {
				startTimed(mode, p, gateOut);
				return;
			}
		}
		lastA = fa;
		lastB = fb;
		if (gateOut) {
			levelA = fa;
			levelB = fb;
		}
		else {
			if (fa) pulseA = TRIG_TIME;
			if (fb) pulseB = TRIG_TIME;
		}
	}

	// RANDOM: add or remove single hits until there are k, so turning the dial
	// changes the pattern a step at a time instead of rolling a new one
	void fitRandom(int k) {
		k = std::min(16, std::max(0, k));
		int n = popcount16(rmask);
		while (n < k) {
			int i = (int) (rng.next() % 16u);
			if (!((rmask >> i) & 1)) { rmask |= (uint16_t) (1u << i); n++; }
		}
		while (n > k) {
			int i = (int) (rng.next() % 16u);
			if ((rmask >> i) & 1) { rmask &= (uint16_t) ~(1u << i); n--; }
		}
	}

	// RANDOM: a whole new pattern with the same number of hits
	void reroll() {
		int k = popcount16(rmask);
		rmask = 0;
		fitRandom(k);
	}

	void startTimed(int mode, float p, bool gateOut) {
		float T = period;
		t = 0.f;
		burst = window = false;
		levelA = levelB = false;
		if (gateOut) {
			// one gate on A, then one on B starting where A ends
			window = true;
			gA1 = gateFraction(mode, p) * T;
			gB0 = gA1;
			gB1 = T * 0.97f;
			lastA = lastB = true;
			return;
		}
		burst = true;
		iA = iB = 0;
		float minGap = T;
		if (mode == RAMP) {
			// 16 hits along the ramp, alternating A, B, A, B..., all inside 94% of
			// the clock period so the last hit is over before the next clock
			nA = nB = 0;
			for (int k = 0; k < 16; k++) {
				float tk = rampTime(p, k / 16.f) * T * 0.94f;
				if (k % 2 == 0) evA[nA++] = tk;
				else evB[nB++] = tk;
			}
			for (int k = 1; k < nA; k++) minGap = std::min(minGap, evA[k] - evA[k - 1]);
			for (int k = 1; k < nB; k++) minGap = std::min(minGap, evB[k] - evB[k - 1]);
		}
		else {
			// a burst of evenly spaced hits on each output, A + B = 16
			nA = ratchetA(p);
			nB = 16 - nA;
			for (int k = 0; k < nA; k++) evA[k] = T * k / nA;
			for (int k = 0; k < nB; k++) evB[k] = T * k / nB;
			minGap = T / std::max(nA, nB);
		}
		pw = std::min(TRIG_TIME, std::max(0.0001f, minGap * 0.5f));
		lastA = nA > 0;
		lastB = nB > 0;
	}

	// One sample. clock/reset are rising edges. Returns the two output levels.
	void process(float dt, bool clock, bool reset, int mode, float p, bool gateOut, bool muted,
	             bool& outA, bool& outB) {
		since += dt;
		if (reset) restart();
		if (mode != lastMode || gateOut != lastGate) {
			burst = window = false;
			levelA = levelB = false;
			lastMode = mode;
			lastGate = gateOut;
		}
		if (!clock && clocked && !timedOut && !frozen && since > TIMEOUT) {
			restart();
			timedOut = true;
		}
		if (mode == RANDOM) fitRandom(randomHits(p));   // keeps the ring up to date
		if (clock) {
			if (fixedPeriod > 0.f) period = fixedPeriod;
			else if (clocked && since < TIMEOUT) period = since;
			since = 0.f;
			clocked = true;
			timedOut = false;
			step(mode, p, gateOut);
		}
		bool wa = false, wb = false;
		if (burst) {
			while (iA < nA && evA[iA] <= t) { pulseA = pw; iA++; }
			while (iB < nB && evB[iB] <= t) { pulseB = pw; iB++; }
			if (iA >= nA && iB >= nB) burst = false;
		}
		if (window) {
			wa = t < gA1;
			wb = t >= gB0 && t < gB1;
			if (t >= gB1) window = false;
		}
		if (burst || window) t += dt;
		bool a = pulseA > 0.f || levelA || wa;
		bool b = pulseB > 0.f || levelB || wb;
		if (pulseA > 0.f) pulseA -= dt;
		if (pulseB > 0.f) pulseB -= dt;
		outA = a && !muted;
		outB = b && !muted;
	}
};

} // namespace divskip
