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
// for each clock whether A / B rose during that clock.
struct Run {
	int a = 0, b = 0;
	std::vector<int> perA, perB;
};

static Run run(Channel& c, int mode, float p, int clocks, float period = 0.125f, bool gate = false, bool muted = false) {
	Run r;
	bool pa = false, pb = false;
	int samples = (int) (period * SR);
	for (int k = 0; k < clocks; k++) {
		int ca = 0, cb = 0;
		for (int s = 0; s < samples; s++) {
			bool oa, ob;
			c.process(DT, s == 0, false, mode, p, gate, muted, oa, ob);
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

static void testCoin() {
	Channel c;
	c.seed(1);
	Run r = run(c, COIN, 0.f, 200);
	CHECK(r.a == 200 && r.b == 0, "coin full left: a=%d b=%d", r.a, r.b);
	r = run(c, COIN, 1.f, 200);
	CHECK(r.a == 0 && r.b == 200, "coin full right: a=%d b=%d", r.a, r.b);
	r = run(c, COIN, 0.5f, 1000);
	CHECK(r.a + r.b == 1000, "coin: every clock goes somewhere");
	CHECK(r.a > 400 && r.a < 600, "coin noon roughly even: a=%d", r.a);
}

static void testDivide() {
	struct { float p; int la, lb; } cases[] = {{0.5f, 8, 8}, {0.f, 1, 15}, {1.f, 15, 1}, {3.f / 14.f, 4, 12}};
	for (auto& k : cases) {
		Channel c;
		CHECK(divideA(k.p) == k.la, "divideA(%g)=%d want %d", k.p, divideA(k.p), k.la);
		Run r = run(c, DIVIDE, k.p, 240);
		CHECK(r.a == 240 / k.la, "divide A by %d: %d", k.la, r.a);
		CHECK(r.b == 240 / k.lb, "divide B by %d: %d", k.lb, r.b);
		CHECK(r.perA[0] == 1 && r.perB[0] == 1, "divide fires on the first clock");
	}
}

static void testTuring() {
	Channel c;
	c.seed(7);
	// locked, length 5: the pattern repeats exactly every 5 clocks
	float p = turingLengthP(5);
	CHECK(turingLength(p) == 5, "turing length %d", turingLength(p));
	Run r = run(c, TURING, p, 50);
	for (int i = 5; i < 50; i++) CHECK(r.perA[i] == r.perA[i - 5], "turing locked loop repeats at %d", i);
	for (int i = 0; i < 50; i++) CHECK(r.perA[i] + r.perB[i] == 1, "turing: A or B each step");
	// full left: always A
	r = run(c, TURING, 0.f, 20);
	CHECK(r.a == 20 && r.b == 0, "turing full left always A");
	// full right: changes
	Channel d;
	d.seed(9);
	uint16_t before = d.reg;
	run(d, TURING, 1.f, 64);
	CHECK(d.reg != before, "turing full right mutates");
}

static void testSplit() {
	Channel c;
	Run r = run(c, SPLIT, 0.5f, 16);
	CHECK(r.a == 3 && r.b == 3, "split noon 3+3: a=%d b=%d", r.a, r.b);
	const int want[8] = {1, 0, 0, 1, 0, 0, 1, 0};   // 3:3:2
	for (int i = 0; i < 8; i++) CHECK(r.perA[i] == want[i], "split noon A step %d", i);
	r = run(c, SPLIT, 0.3f, 16);
	CHECK(r.a == 3 && r.b == 3, "split 0.3 still 3+3");
	CHECK(splitA(0.3f) == 6, "split 0.3: A over 6 (%d)", splitA(0.3f));
}

static void testClassic() {
	int last = 0;
	for (int i = 0; i < 32; i++) {
		CHECK(CLASSIC[i][0] >= last, "classic sorted");
		last = CLASSIC[i][0];
		int hits = 0;
		for (int s = 0; s < CLASSIC[i][0]; s++) hits += euclidHit(CLASSIC[i][1], CLASSIC[i][0], s);
		CHECK(hits == CLASSIC[i][1], "classic %d hits", i);
	}
	Channel c;
	float p = 14.5f / 32.f;   // index 14 = 3 in 8
	CHECK(classicIndex(p) == 14, "classic index");
	Run r = run(c, EUCLID, p, 16);
	CHECK(r.a == 6 && r.b == 10, "classic 3/8 twice: a=%d b=%d", r.a, r.b);
}

static void testRatchet() {
	Channel c;
	run(c, RATCHET, 0.5f, 2);   // learn the clock period
	Run r = run(c, RATCHET, 0.5f, 4);
	for (int i = 0; i < 4; i++) CHECK(r.perA[i] == 8 && r.perB[i] == 8, "ratchet noon 8+8: %d %d", r.perA[i], r.perB[i]);
	r = run(c, RATCHET, 0.f, 3);
	CHECK(r.perA[2] == 1 && r.perB[2] == 15, "ratchet left 1+15: %d %d", r.perA[2], r.perB[2]);
}

static void testRamp() {
	Channel c;
	run(c, RAMP, 0.f, 2);
	for (float p : {0.f, 0.5f, 1.f}) {
		Run r = run(c, RAMP, p, 3);
		CHECK(r.perA[2] == 8 && r.perB[2] == 8, "ramp %g gives 16 hits: %d %d", p, r.perA[2], r.perB[2]);
		float prev = 0.f;
		for (int k = 1; k <= 16; k++) {
			float t = rampTime(p, k / 16.f);
			CHECK(t > prev, "ramp times rise");
			prev = t;
		}
	}
	// speeding up: gaps shrink
	CHECK(rampTime(0.f, 2 / 16.f) - rampTime(0.f, 1 / 16.f) > rampTime(0.f, 15 / 16.f) - rampTime(0.f, 14 / 16.f), "ramp left speeds up");
	CHECK(rampTime(1.f, 2 / 16.f) - rampTime(1.f, 1 / 16.f) < rampTime(1.f, 15 / 16.f) - rampTime(1.f, 14 / 16.f), "ramp right slows down");
}

static void testGates() {
	// gate outputs, step mode: two A steps in a row = one long gate
	Channel c;
	Run r = run(c, COIN, 0.f, 10, 0.125f, true);
	CHECK(r.a == 1, "gate mode: A held across steps (%d rises)", r.a);
	// gate outputs, RAMP: one gate on A then one on B every clock
	Channel d;
	run(d, RAMP, 0.5f, 2, 0.125f, true);
	r = run(d, RAMP, 0.5f, 4, 0.125f, true);
	for (int i = 0; i < 4; i++) CHECK(r.perA[i] == 1 && r.perB[i] == 1, "ramp gate: one A + one B per clock");
	CHECK(std::fabs(gateFraction(RAMP, 0.5f) - 0.5f) < 1e-5f, "ramp gate noon is half");
	CHECK(gateFraction(RAMP, 0.f) < gateFraction(RAMP, 0.25f) && gateFraction(RAMP, 0.75f) < gateFraction(RAMP, 1.f), "gate fraction rises");
}

static void testMuteAndReset() {
	Channel c;
	Run r = run(c, DIVIDE, 0.5f, 16, 0.125f, false, true);
	CHECK(r.a == 0 && r.b == 0, "muted: silent");
	CHECK(c.divA == 0, "muted still counts (16 clocks wrap to 0): %d", c.divA);
	run(c, DIVIDE, 0.5f, 3);
	CHECK(c.divA == 3, "counting");
	bool oa, ob;
	c.process(DT, false, true, DIVIDE, 0.5f, false, false, oa, ob);
	CHECK(c.divA == 0, "reset input");
	run(c, DIVIDE, 0.5f, 3);
	for (int s = 0; s < (int) (3.2f * SR); s++) c.process(DT, false, false, DIVIDE, 0.5f, false, false, oa, ob);
	CHECK(c.divA == 0, "3 s without clock resets");
}

static void testPatterns() {
	for (int k = 0; k < 4; k++)
		for (int i = 0; i < PATTERN_COUNT; i++) {
			const DrumPattern& d = PATTERNS[k][i];
			CHECK(d.name && d.name[0], "pattern name");
		}
	CHECK(PATTERNS[0][0].a == 0x1111111111111111ull, "four floor kick");
	Channel c;
	c.kit = 0;
	Run r = run(c, PATTERN, 0.f, 64);
	CHECK(r.a == 16, "four floor: 16 kicks in 4 bars (%d)", r.a);
	CHECK(r.b == 10, "four floor: 6 claps + 4 in the fill (%d)", r.b);
}

static void testRandom() {
	Channel c;
	c.seed(3);
	for (int k = 0; k <= 16; k++) {
		Run r = run(c, RANDOM, k / 16.f, 32);
		CHECK(r.a == 2 * k && r.b == 32 - 2 * k, "random %d of 16: a=%d b=%d", k, r.a, r.b);
	}
	// one more hit keeps the old ones
	run(c, RANDOM, 5 / 16.f, 1);
	uint16_t before = c.rmask;
	run(c, RANDOM, 6 / 16.f, 1);
	CHECK((c.rmask & before) == before && popcount16(c.rmask) == 6, "random adds one hit");
	// the loop repeats while the dial stays put
	Run r = run(c, RANDOM, 6 / 16.f, 48);
	for (int i = 16; i < 48; i++) CHECK(r.perA[i] == r.perA[i - 16], "random loop repeats");
	c.reroll();
	CHECK(popcount16(c.rmask) == 6, "reroll keeps the count");
}

static void testSticky() {
	Channel c;
	c.seed(5);
	Run r = run(c, STICKY, 0.f, 20);
	for (int i = 1; i < 20; i++) CHECK(r.perA[i] != r.perA[i - 1], "sticky left alternates");
	// right: long runs, few changes
	r = run(c, STICKY, 1.f, 1000);
	int changes = 0;
	for (int i = 1; i < 1000; i++) changes += r.perA[i] != r.perA[i - 1];
	CHECK(changes > 5 && changes < 80, "sticky right: runs (%d changes)", changes);
	r = run(c, STICKY, 0.5f, 2000);
	CHECK(r.a > 800 && r.a < 1200, "sticky noon even: %d", r.a);
}

int main() {
	testSticky();
	testRandom();
	testCoin();
	testDivide();
	testTuring();
	testSplit();
	testClassic();
	testRatchet();
	testRamp();
	testGates();
	testMuteAndReset();
	testPatterns();
	if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
	std::printf("all engine tests passed\n");
	return 0;
}
