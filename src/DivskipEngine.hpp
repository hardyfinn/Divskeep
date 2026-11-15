// DivskipEngine.hpp
// The logic of one Divskip channel, with no VCV Rack code in it, so it can be
// tested on its own (test/engine_test.cpp).
//
// Every channel takes a clock (or any triggers) and splits it between two
// outputs, A and B. What goes where depends on the mode and on two settings,
// AMOUNT (p) and SHAPE (q), each 0..1 (knob plus CV).
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
	"Euclid", "Ramp (hits speeding up or slowing down)", "Ratchet bursts", "Drum patterns",
	"Random steps", "Sticky coin (runs and clusters)"};

// what SHAPE does in each mode (nullptr = nothing; the knob is dimmed)
static const char* const SHAPE_NAMES[MODE_COUNT] = {
	"loop length", "rotate", "mask length", nullptr, "hits",
	"hits", "divide first", nullptr, "steps", "loop length"};

// where SHAPE starts in each mode (the knob remembers its own position per mode)
static const float SHAPE_DEFAULTS[MODE_COUNT] = {
	0.7f,            // COIN: loop 16
	0.f,             // DIVIDE: rotate 0
	0.f,             // TURING: no mask
	0.f,             // SPLIT: -
	3.f / 16.f,      // EUCLID: 3 hits
	1.f,             // RAMP: 16 hits
	0.f,             // RATCHET: no divider
	0.f,             // PATTERN: -
	15.f / 31.f,     // RANDOM: 16 steps
	0.f};            // STICKY: no loop

static const float TRIG_TIME = 0.005f;   // 5 ms output triggers
static const float TIMEOUT = 3.f;        // no clock for 3 s: back to step 1
static const float GATE_GAP = 0.002f;    // "gate gap" option: gates end 2 ms early

// k hits spread as evenly as possible over n steps, first step always a hit
inline bool euclidHit(int k, int n, int i) {
	if (n <= 0) return false;
	return ((i * k) % n) < k;
}

inline int roundi(float x) { return (int) std::floor(x + 0.5f); }
inline float clamp01(float x) { return std::min(1.f, std::max(0.f, x)); }
inline int popcount32(uint32_t v) { int n = 0; for (; v; v &= v - 1) n++; return n; }

// ---- what AMOUNT (p) means in each mode (shared with the display) ----
inline int divideA(float p) { return 1 + roundi(clamp01(p) * 14.f); }          // 1..15, B = 16 - A
inline int splitA(float p) { return 3 + roundi(clamp01(p) * 10.f); }           // 3..13, B = 16 - A
inline int ratchetA(float p) { return 1 + roundi(clamp01(p) * 14.f); }         // 1..15, B = 16 - A
inline int euclidLength(float p) { return 1 + roundi(clamp01(p) * 15.f); }     // 1..16
inline int patternIndex(float p) { return std::min(PATTERN_COUNT - 1, (int) (clamp01(p) * PATTERN_COUNT)); }
// Turing: left half sets the loop length (locked), right half the chance a step flips
inline int turingLength(float p) { return p >= 0.5f ? 16 : 1 + roundi(clamp01(p) * 2.f * 15.f); }
inline float turingFlip(float p) { return p > 0.5f ? (clamp01(p) - 0.5f) : 0.f; }   // 0..0.5
inline bool turingAlwaysOn(float p) { return p < 0.02f; }
inline float turingLengthP(int L) { return (L - 1) / 30.f; }   // knob position for a loop of L
// STICKY: chance that a hit goes to the same output as the last one.
// Left = always swaps (A B A B), noon = plain coin toss, right = long runs.
inline float stickyRepeat(float p) { return std::min(0.97f, clamp01(p)); }

// ---- what SHAPE (q) means in each mode ----
// COIN and STICKY: loop off, 4, 8, 16 or 32 steps
inline int loopSteps(float q) {
	static const int L[5] = {0, 4, 8, 16, 32};
	return L[std::min(4, (int) (clamp01(q) * 5.f))];
}
inline int divideRotate(float q) { return roundi(clamp01(q) * 15.f); }        // 0..15
inline int maskLength(float q) { return roundi(clamp01(q) * 8.f); }           // 0 (off)..8
inline int euclidHits(float p, float q) { return std::min(euclidLength(p), roundi(clamp01(q) * 16.f)); }
inline int rampHits(float q) { return 1 + roundi(clamp01(q) * 15.f); }        // 1..16
inline int ratchetDivide(float q) {                                           // 1 (off), 2, 4, 8
	static const int D[4] = {1, 2, 4, 8};
	return D[std::min(3, (int) (clamp01(q) * 4.f))];
}
inline int randomSteps(float q) { return 1 + roundi(clamp01(q) * 31.f); }     // 1..32
inline int randomHits(float p, float q) { return roundi(clamp01(p) * randomSteps(q)); }
inline bool usesSeed(int mode) { return mode == COIN || mode == TURING || mode == RANDOM || mode == STICKY; }

// Ramp shape: where hit x (0..1) lands, as a fraction of the clock period.
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
	int euc = 0;            // EUCLID
	int pattern = 0;        // PATTERN, 0..63
	int turPos = 0, maskPos = 0;
	int rpos = 0;           // RANDOM
	int loopPos = 0;        // COIN / STICKY loops
	int ratchetCount = 0;   // RATCHET divider
	bool stickB = false;    // STICKY: where the last hit went

	// the seed: everything random that loops (saved with the patch)
	uint16_t reg = 0xA5C3;      // TURING shift register
	uint8_t mask = 0xB5;        // TURING mask
	uint32_t rmask = 0x0101;    // RANDOM: which of the 32 steps hit
	int rsteps = 16;            // RANDOM: steps in use (for a new seed)
	float coinU[32];            // COIN loop: one random number per step
	float stickU[32];           // STICKY loop

	bool frozen = false;        // chained: the chain does the 3 s auto-reset instead
	float fixedPeriod = 0.f;    // chained: clock period measured by the chain (0 = measure here)

	// outputs
	bool levelA = false, levelB = false;  // held gates (GATE outputs, step modes)
	float pulseA = 0.f, pulseB = 0.f;     // trigger time left
	bool lastA = false, lastB = false;    // where the last step went (display)
	bool gapLow = false;                  // gate gap: one low sample on a new clock

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

	Channel() {
		for (int i = 0; i < 32; i++) coinU[i] = stickU[i] = rng.uniform();
	}

	void seed(uint32_t s) {
		rng.s = s ? s : 1u;
		newSeed();
	}

	// NEW SEED: new Turing loop and mask, new coin and sticky loops, new
	// random pattern (same number of hits)
	void newSeed() {
		reg = (uint16_t) (rng.next() & 0xFFFF);
		do mask = (uint8_t) (rng.next() & 0xFF); while (popcount32(mask) < 2);
		for (int i = 0; i < 32; i++) {
			coinU[i] = rng.uniform();
			stickU[i] = rng.uniform();
		}
		int L = rsteps;
		uint32_t used = L >= 32 ? 0xFFFFFFFFu : ((1u << L) - 1u);
		int k = popcount32(rmask & used);
		rmask = 0;
		fitRandom(k, L);
	}

	// back to step 1 (RESET input, or 3 s without a clock)
	void restart() {
		divA = divB = split = euc = pattern = turPos = maskPos = rpos = loopPos = ratchetCount = 0;
		stickB = false;
		burst = window = false;
		levelA = levelB = false;
	}

	// how many clocks the current loop has (CHAIN uses this for each turn)
	int loopLength(int mode, float p, float q) const {
		switch (mode) {
			case TURING: return turingLength(p);
			case EUCLID: return euclidLength(p);
			case RANDOM: return randomSteps(q);
			case COIN:
			case STICKY: return loopSteps(q) ? loopSteps(q) : 16;
			default: return 16;
		}
	}

	// RANDOM: add or remove single hits among the first `steps` until there
	// are k, so turning the knob changes the pattern a step at a time
	void fitRandom(int k, int steps) {
		steps = std::min(32, std::max(1, steps));
		k = std::min(steps, std::max(0, k));
		uint32_t used = steps == 32 ? 0xFFFFFFFFu : ((1u << steps) - 1u);
		int n = popcount32(rmask & used);
		while (n < k) {
			int i = (int) (rng.next() % (uint32_t) steps);
			if (!((rmask >> i) & 1u)) { rmask |= 1u << i; n++; }
		}
		while (n > k) {
			int i = (int) (rng.next() % (uint32_t) steps);
			if ((rmask >> i) & 1u) { rmask &= ~(1u << i); n--; }
		}
	}

	// the 16 ring lights: which steps go to A, which to B, and the playhead.
	// Returns false when the mode has no step pattern to show.
	bool ring(int mode, float p, float q, bool a[16], bool b[16], int& play) const {
		for (int i = 0; i < 16; i++) a[i] = b[i] = false;
		play = -1;
		switch (mode) {
			case DIVIDE: {
				int la = divideA(p), lb = 16 - la, r = divideRotate(q);
				for (int i = 0; i < 16; i++) {
					a[i] = (i % la) == (r % la);
					b[i] = (i % lb) == (r % lb);
				}
				play = (divA + la - 1) % la;
				return true;
			}
			case TURING: {
				int L = turingLength(p), M = maskLength(q);
				for (int i = 0; i < L; i++) {
					bool bit = turingAlwaysOn(p) || ((reg >> i) & 1);
					bool open = M == 0 || ((mask >> (i % M)) & 1);
					a[i] = bit && open;
					b[i] = !bit && open;
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
				int n = euclidLength(p), k = euclidHits(p, q);
				for (int i = 0; i < n; i++) {
					a[i] = euclidHit(k, n, i);
					b[i] = !a[i];
				}
				play = (euc + n - 1) % n;
				return true;
			}
			case RANDOM: {
				int L = randomSteps(q);
				int cur = (rpos + L - 1) % L;
				int page = cur / 16;
				for (int i = 0; i < 16; i++) {
					int s = page * 16 + i;
					if (s >= L) break;
					a[i] = (rmask >> s) & 1u;
					b[i] = !a[i];
				}
				play = cur % 16;
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
	void step(int mode, float p, float q, bool gateOut) {
		bool fa = false, fb = false;
		switch (mode) {
			case COIN: {
				int L = loopSteps(q);
				float u;
				if (L) {
					if (loopPos >= L) loopPos = 0;
					u = coinU[loopPos];
					loopPos = (loopPos + 1) % L;
				}
				else u = rng.uniform();
				fb = u < clamp01(p);
				fa = !fb;
			} break;
			case STICKY: {
				int L = loopSteps(q);
				float u;
				if (L) {
					if (loopPos >= L) loopPos = 0;
					if (loopPos == 0) stickB = false;   // every pass starts the same way
					u = stickU[loopPos];
					loopPos = (loopPos + 1) % L;
				}
				else u = rng.uniform();
				if (u >= stickyRepeat(p)) stickB = !stickB;
				fb = stickB;
				fa = !fb;
			} break;
			case DIVIDE: {
				int la = divideA(p), lb = 16 - la, r = divideRotate(q);
				if (divA >= la) divA = 0;
				if (divB >= lb) divB = 0;
				fa = divA == r % la;
				fb = divB == r % lb;
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
				// the mask: a second, shorter loop that lets steps through or not
				int M = maskLength(q);
				if (M > 0) {
					if (maskPos >= M) maskPos = 0;
					if (!((mask >> maskPos) & 1)) fa = fb = false;
					maskPos = (maskPos + 1) % M;
				}
			} break;
			case SPLIT: {
				int la = splitA(p), lb = 16 - la;
				if (split < la) fa = euclidHit(3, la, split);
				else fb = euclidHit(3, lb, split - la);
				split = (split + 1) % 16;
			} break;
			case EUCLID: {
				int n = euclidLength(p), k = euclidHits(p, q);
				if (euc >= n) euc = 0;
				fa = euclidHit(k, n, euc);
				fb = !fa;
				euc = (euc + 1) % n;
			} break;
			case PATTERN: {
				const DrumPattern& d = PATTERNS[kit & 3][patternIndex(p)];
				fa = (d.a >> pattern) & 1;
				fb = (d.b >> pattern) & 1;
				pattern = (pattern + 1) % 64;
			} break;
			case RANDOM: {
				int L = randomSteps(q);
				fitRandom(randomHits(p, q), L);
				if (rpos >= L) rpos = 0;
				fa = (rmask >> rpos) & 1u;
				fb = !fa;
				rpos = (rpos + 1) % L;
			} break;
			case RAMP: {
				startTimed(mode, p, q, gateOut, period);
				return;
			}
			case RATCHET: {
				int d = ratchetDivide(q);
				if (ratchetCount % d == 0) startTimed(mode, p, q, gateOut, period * d);
				ratchetCount = (ratchetCount + 1) % d;
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

	void startTimed(int mode, float p, float q, bool gateOut, float T) {
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
			// n hits along the ramp, alternating A, B, A, B..., all inside 94% of
			// the clock period so the last hit is over before the next clock
			int n = rampHits(q);
			nA = nB = 0;
			for (int k = 0; k < n; k++) {
				float tk = rampTime(p, (float) k / n) * T * 0.94f;
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

	// One sample. clock/reset are rising edges. gap = the "gate gap" option.
	// Returns the two output levels.
	void process(float dt, bool clock, bool reset, int mode, float p, float q, bool gateOut, bool gap,
	             bool muted, bool& outA, bool& outB) {
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
		if (mode == RANDOM) {
			rsteps = randomSteps(q);
			fitRandom(randomHits(p, q), rsteps);   // keeps the ring up to date
		}
		gapLow = false;
		if (clock) {
			if (fixedPeriod > 0.f) period = fixedPeriod;
			else if (clocked && since < TIMEOUT) period = since;
			since = 0.f;
			clocked = true;
			timedOut = false;
			bool wasA = levelA, wasB = levelB;
			step(mode, p, q, gateOut);
			// gate gap: if a gate carries on into the next step, drop it for one
			// sample so the next module sees a new gate
			gapLow = gap && ((wasA && levelA) || (wasB && levelB));
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
		// gate gap: held gates end 2 ms before the next clock is due
		bool held = !gap || since < std::max(0.001f, period - GATE_GAP);
		bool a = pulseA > 0.f || (levelA && held && !gapLow) || wa;
		bool b = pulseB > 0.f || (levelB && held && !gapLow) || wb;
		if (pulseA > 0.f) pulseA -= dt;
		if (pulseB > 0.f) pulseB -= dt;
		outA = a && !muted;
		outB = b && !muted;
	}
};

} // namespace divskip
