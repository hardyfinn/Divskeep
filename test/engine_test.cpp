// Tests for the Divskip engine. No Rack SDK needed:
//   c++ -std=c++11 -O2 -Wall -Wextra -o test/engine_test test/engine_test.cpp && test/engine_test
#include "../src/DivskipEngine.hpp"
#include <cstdio>
#include <vector>

using namespace divskip;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const float SR = 48000.f, DT = 1.f / SR;

// Run n clocks at the given period. Counts rising edges on A and B, and records
// for each clock how many times A / B rose during that clock.
struct Run {
	int a = 0, b = 0;
	std::vector<int> perA, perB;
};

struct Opts {
	float period = 0.125f;
	bool gate = false, gap = false, muted = false;
};

static Run run(Channel& c, int mode, float p, float q, int clocks, Opts o = Opts()) {
	Run r;
	bool pa = false, pb = false;
	int samples = (int) (o.period * SR);
	for (int k = 0; k < clocks; k++) {
		int ca = 0, cb = 0;
		for (int s = 0; s < samples; s++) {
			bool oa, ob;
			c.process(DT, s == 0, false, mode, p, q, o.gate, o.gap, o.muted, oa, ob);
			if (oa && !pa) ca++;
			if (ob && !pb) cb++;
			pa = oa;
			pb = ob;
		}
		r.a += ca;
		r.b += cb;
		r.perA.push_back(ca);
		r.perB.push_back(cb);
	}
	return r;
}

static void testDefaults() {
	CHECK(loopSteps(SHAPE_DEFAULTS[COIN]) == 16, "COIN default loop 16");
	CHECK(divideRotate(SHAPE_DEFAULTS[DIVIDE]) == 0, "DIVIDE default rotate 0");
	CHECK(maskLength(SHAPE_DEFAULTS[TURING]) == 0, "TURING default no mask");
	CHECK(euclidHits(1.f, SHAPE_DEFAULTS[EUCLID]) == 3, "EUCLID default 3 hits");
	CHECK(rampHits(SHAPE_DEFAULTS[RAMP]) == 16, "RAMP default 16");
	CHECK(ratchetDivide(SHAPE_DEFAULTS[RATCHET]) == 1, "RATCHET default no divide");
	CHECK(randomSteps(SHAPE_DEFAULTS[RANDOM]) == 16, "RANDOM default 16 steps (%d)", randomSteps(SHAPE_DEFAULTS[RANDOM]));
	CHECK(loopSteps(SHAPE_DEFAULTS[STICKY]) == 0, "STICKY default no loop");
	for (int m = 0; m < MODE_COUNT; m++) CHECK((SHAPE_NAMES[m] == nullptr) == (m == SPLIT || m == PATTERN), "shape names");
}

static void testCoin() {
	Channel c;
	c.seed(1);
	Run r = run(c, COIN, 0.f, 0.f, 200);
	CHECK(r.a == 200 && r.b == 0, "coin full left: a=%d b=%d", r.a, r.b);
	r = run(c, COIN, 1.f, 0.f, 200);
	CHECK(r.a == 0 && r.b == 200, "coin full right: a=%d b=%d", r.a, r.b);
	r = run(c, COIN, 0.5f, 0.f, 1000);
	CHECK(r.a + r.b == 1000, "coin: every clock goes somewhere");
	CHECK(r.a > 400 && r.a < 600, "coin noon roughly even: a=%d", r.a);
	// looped: repeats every L
	for (float q : {0.3f, 0.5f, 0.7f, 0.95f}) {
		int L = loopSteps(q);
		Channel d;
		d.seed(2);
		Run s = run(d, COIN, 0.5f, q, 4 * L);
		for (int i = L; i < 4 * L; i++) CHECK(s.perA[i] == s.perA[i - L], "coin loop %d repeats", L);
	}
	// new seed changes the loop
	Channel e;
	e.seed(3);
	Run s1 = run(e, COIN, 0.5f, 0.95f, 32);
	e.newSeed();
	e.restart();
	Run s2 = run(e, COIN, 0.5f, 0.95f, 32);
	CHECK(s1.perA != s2.perA, "coin: new seed, new loop");
}

static void testSticky() {
	Channel c;
	c.seed(5);
	Run r = run(c, STICKY, 0.f, 0.f, 20);
	for (int i = 1; i < 20; i++) CHECK(r.perA[i] != r.perA[i - 1], "sticky left alternates");
	r = run(c, STICKY, 1.f, 0.f, 1000);
	int changes = 0;
	for (int i = 1; i < 1000; i++) changes += r.perA[i] != r.perA[i - 1];
	CHECK(changes > 5 && changes < 80, "sticky right: runs (%d changes)", changes);
	r = run(c, STICKY, 0.5f, 0.f, 2000);
	CHECK(r.a > 800 && r.a < 1200, "sticky noon even: %d", r.a);
	// looped
	Channel d;
	d.seed(6);
	Run s = run(d, STICKY, 0.6f, 0.5f, 32);   // loop 8
	CHECK(loopSteps(0.5f) == 8, "loop 8");
	for (int i = 8; i < 32; i++) CHECK(s.perA[i] == s.perA[i - 8], "sticky loop repeats");
}

static void testDivide() {
	struct { float p; int la, lb; } cases[] = {{0.5f, 8, 8}, {0.f, 1, 15}, {1.f, 15, 1}, {3.f / 14.f, 4, 12}};
	for (auto& k : cases) {
		Channel c;
		CHECK(divideA(k.p) == k.la, "divideA(%g)=%d want %d", k.p, divideA(k.p), k.la);
		Run r = run(c, DIVIDE, k.p, 0.f, 240);
		CHECK(r.a == 240 / k.la, "divide A by %d: %d", k.la, r.a);
		CHECK(r.b == 240 / k.lb, "divide B by %d: %d", k.lb, r.b);
		CHECK(r.perA[0] == 1 && r.perB[0] == 1, "divide fires on the first clock");
	}
	// rotate by 3: A (/4) now fires on clock 4, 8, ...
	Channel c;
	Run r = run(c, DIVIDE, 3.f / 14.f, 3.f / 15.f, 16);
	CHECK(divideRotate(3.f / 15.f) == 3, "rotate 3");
	CHECK(r.perA[0] == 0 && r.perA[3] == 1 && r.perA[7] == 1, "divide rotated");
	CHECK(r.a == 4, "rotated still /4");
}

static void testTuring() {
	Channel c;
	c.seed(7);
	float p = turingLengthP(5);
	CHECK(turingLength(p) == 5, "turing length %d", turingLength(p));
	Run r = run(c, TURING, p, 0.f, 50);
	for (int i = 5; i < 50; i++) CHECK(r.perA[i] == r.perA[i - 5], "turing locked loop repeats at %d", i);
	for (int i = 0; i < 50; i++) CHECK(r.perA[i] + r.perB[i] == 1, "turing: A or B each step");
	r = run(c, TURING, 0.f, 0.f, 20);
	CHECK(r.a == 20 && r.b == 0, "turing full left always A");
	Channel d;
	d.seed(9);
	uint16_t before = d.reg;
	run(d, TURING, 1.f, 0.f, 64);
	CHECK(d.reg != before, "turing full right mutates");
	// mask of 3: steps silenced where the mask is off, pattern repeats every 3
	Channel e;
	e.seed(11);
	e.mask = 0x05;   // on, off, on
	float q = 3.f / 8.f;
	CHECK(maskLength(q) == 3, "mask 3");
	r = run(e, TURING, 0.f, q, 12);   // always A, masked
	for (int i = 0; i < 12; i++) CHECK(r.perA[i] == (i % 3 != 1), "mask step %d", i);
	CHECK(r.b == 0, "masked: no B");
}

static void testSplit() {
	Channel c;
	Run r = run(c, SPLIT, 0.5f, 0.f, 16);
	CHECK(r.a == 3 && r.b == 3, "split noon 3+3: a=%d b=%d", r.a, r.b);
	const int want[8] = {1, 0, 0, 1, 0, 0, 1, 0};
	for (int i = 0; i < 8; i++) CHECK(r.perA[i] == want[i], "split noon A step %d", i);
}

static void testEuclid() {
	Channel c;
	float p = 7.f / 15.f;   // length 8
	CHECK(euclidLength(p) == 8, "euclid length 8");
	Run r = run(c, EUCLID, p, SHAPE_DEFAULTS[EUCLID], 16);
	CHECK(r.a == 6 && r.b == 10, "3 in 8 twice: a=%d b=%d", r.a, r.b);
	r = run(c, EUCLID, 1.f, 5.f / 16.f, 16);
	CHECK(r.a == 5 && r.b == 11, "5 in 16: a=%d", r.a);
	CHECK(euclidHits(0.f, 1.f) == 1, "hits never more than the length");
}

static void testRatchet() {
	Channel c;
	run(c, RATCHET, 0.5f, 0.f, 2);
	Run r = run(c, RATCHET, 0.5f, 0.f, 4);
	for (int i = 0; i < 4; i++) CHECK(r.perA[i] == 8 && r.perB[i] == 8, "ratchet noon 8+8: %d %d", r.perA[i], r.perB[i]);
	r = run(c, RATCHET, 0.f, 0.f, 3);
	CHECK(r.perA[2] == 1 && r.perB[2] == 15, "ratchet left 1+15: %d %d", r.perA[2], r.perB[2]);
	// divide by 2 first: one burst every 2 clocks, spread over both
	Channel d;
	run(d, RATCHET, 0.5f, 0.3f, 2);
	CHECK(ratchetDivide(0.3f) == 2, "divide 2");
	r = run(d, RATCHET, 0.5f, 0.3f, 4);
	CHECK(r.a == 16 && r.b == 16, "ratchet /2: 2 bursts of 8+8 in 4 clocks (%d %d)", r.a, r.b);
	CHECK(r.perA[0] == 4 && r.perA[1] == 4, "burst spread over 2 clocks (%d %d)", r.perA[0], r.perA[1]);
}

static void testRamp() {
	Channel c;
	run(c, RAMP, 0.f, 1.f, 2);
	for (float p : {0.f, 0.5f, 1.f}) {
		Run r = run(c, RAMP, p, 1.f, 3);
		CHECK(r.perA[2] == 8 && r.perB[2] == 8, "ramp %g gives 16 hits: %d %d", p, r.perA[2], r.perB[2]);
	}
	Run r = run(c, RAMP, 0.f, 6.f / 15.f, 3);
	CHECK(rampHits(6.f / 15.f) == 7, "7 hits");
	CHECK(r.perA[2] == 4 && r.perB[2] == 3, "ramp 7 hits: %d %d", r.perA[2], r.perB[2]);
	CHECK(rampTime(0.f, 2 / 16.f) - rampTime(0.f, 1 / 16.f) > rampTime(0.f, 15 / 16.f) - rampTime(0.f, 14 / 16.f), "ramp left speeds up");
}

static void testRandom() {
	Channel c;
	c.seed(3);
	for (int k = 0; k <= 16; k++) {
		Run r = run(c, RANDOM, k / 16.f, SHAPE_DEFAULTS[RANDOM], 32);
		CHECK(r.a == 2 * k && r.b == 32 - 2 * k, "random %d of 16: a=%d b=%d", k, r.a, r.b);
	}
	float q16 = SHAPE_DEFAULTS[RANDOM];
	run(c, RANDOM, 5 / 16.f, q16, 1);
	uint32_t before = c.rmask & 0xFFFF;
	run(c, RANDOM, 6 / 16.f, q16, 1);
	CHECK(((c.rmask & 0xFFFF) & before) == before && popcount32(c.rmask & 0xFFFF) == 6, "random adds one hit");
	Run r = run(c, RANDOM, 6 / 16.f, q16, 48);
	for (int i = 16; i < 48; i++) CHECK(r.perA[i] == r.perA[i - 16], "random loop repeats");
	// 7 steps, about half hit
	Channel d;
	d.seed(4);
	float q7 = 6.f / 31.f;
	CHECK(randomSteps(q7) == 7, "7 steps");
	r = run(d, RANDOM, 0.5f, q7, 28);
	CHECK(r.a == 4 * randomHits(0.5f, q7), "random 7 steps loop (%d)", r.a);
	for (int i = 7; i < 28; i++) CHECK(r.perA[i] == r.perA[i - 7], "random 7 repeats");
	d.newSeed();
	CHECK(popcount32(d.rmask & 0x7F) == randomHits(0.5f, q7), "new seed keeps the count");
}

static void testGates() {
	Channel c;
	Opts o;
	o.gate = true;
	Run r = run(c, COIN, 0.f, 0.f, 10, o);
	CHECK(r.a == 1, "gate mode: A held across steps (%d rises)", r.a);
	// gate gap: every step is its own gate
	Channel g;
	o.gap = true;
	r = run(g, COIN, 0.f, 0.f, 10, o);
	CHECK(r.a == 10, "gate gap: separate gates (%d rises)", r.a);
	Channel d;
	o.gap = false;
	run(d, RAMP, 0.5f, 1.f, 2, o);
	r = run(d, RAMP, 0.5f, 1.f, 4, o);
	for (int i = 0; i < 4; i++) CHECK(r.perA[i] == 1 && r.perB[i] == 1, "ramp gate: one A + one B per clock");
}

static void testMuteAndReset() {
	Channel c;
	Opts o;
	o.muted = true;
	Run r = run(c, DIVIDE, 0.5f, 0.f, 16, o);
	CHECK(r.a == 0 && r.b == 0, "muted: silent");
	run(c, DIVIDE, 0.5f, 0.f, 3);
	CHECK(c.divA == 3, "counting");
	bool oa, ob;
	c.process(DT, false, true, DIVIDE, 0.5f, 0.f, false, false, false, oa, ob);
	CHECK(c.divA == 0, "reset input");
	run(c, DIVIDE, 0.5f, 0.f, 3);
	for (int s = 0; s < (int) (3.2f * SR); s++) c.process(DT, false, false, DIVIDE, 0.5f, 0.f, false, false, false, oa, ob);
	CHECK(c.divA == 0, "3 s without clock resets");
}

static void testPatterns() {
	CHECK(PATTERNS[0][0].a == 0x1111111111111111ull, "four floor kick");
	Channel c;
	c.kit = 0;
	Run r = run(c, PATTERN, 0.f, 0.f, 64);
	CHECK(r.a == 16, "four floor: 16 kicks in 4 bars (%d)", r.a);
	CHECK(r.b == 10, "four floor: 6 claps + 4 in the fill (%d)", r.b);
}

int main() {
	testDefaults();
	testCoin();
	testSticky();
	testDivide();
	testTuring();
	testSplit();
	testEuclid();
	testRatchet();
	testRamp();
	testRandom();
	testGates();
	testMuteAndReset();
	testPatterns();
	if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
	std::printf("all engine tests passed\n");
	return 0;
}
