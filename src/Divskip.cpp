// Divskip.cpp
// Four channels. Each takes triggers (a clock, usually) and splits them between
// two outputs, A and B, in one of ten modes. The engine is in DivskipEngine.hpp.
#include "plugin.hpp"
#include "DivskipEngine.hpp"

using namespace divskip;

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float X_IN = 9.f, X_MODE = 20.f, X_CV = 33.f, X_KNOB = 47.f;
static const float X_CV2 = 62.f, X_SHAPE = 74.f;
static const float X_DISP0 = 81.f, X_DISP1 = 109.5f;
static const float X_SEEDIN = 116.f;
static const float X_MUTE = 129.5f, X_A = 142.5f, X_B = 154.5f;
static const float ROWS[4] = {33.f, 53.f, 73.f, 93.f};
static const float BOT_Y = 114.5f;
static const float X_RESET = 9.f, X_OUTSW = 36.f, X_MUTESW = 64.f;
static const float X_GSEEDIN = 89.f, X_GSEEDBTN = 101.f;
static const NVGcolor C_PANEL = nvgRGB(0x1D, 0x1F, 0x24);

// colours per mode (ring and display), and for B steps / mute sets
static const NVGcolor MODE_COLORS[MODE_COUNT] = {
	nvgRGB(0xF0, 0xA0, 0x4B),   // COIN
	nvgRGB(0x9B, 0xE0, 0x7A),   // DIVIDE
	nvgRGB(0xB4, 0x8C, 0xFF),   // TURING
	nvgRGB(0xFF, 0x8F, 0xD0),   // SPLIT
	nvgRGB(0xF7, 0xD3, 0x5C),   // EUCLID
	nvgRGB(0xFF, 0x6B, 0x5B),   // RAMP
	nvgRGB(0x7C, 0xA8, 0xFF),   // RATCHET
	nvgRGB(0xFF, 0x7A, 0xA8),   // PATTERN
	nvgRGB(0x6F, 0xE0, 0xC0),   // RANDOM
	nvgRGB(0xE8, 0xC0, 0x8A),   // STICKY
};
static const NVGcolor C_B = nvgRGB(0x5F, 0xD3, 0xE6);
static const NVGcolor C_M1 = nvgRGB(0x5F, 0xD3, 0xE6);
static const NVGcolor C_M2 = nvgRGB(0xF0, 0x8A, 0x3C);
static const NVGcolor C_OFF = nvgRGB(0x33, 0x36, 0x3D);
static const NVGcolor C_INK = nvgRGB(0xE6, 0xE3, 0xDA);
static const NVGcolor C_SOFT = nvgRGB(0x8E, 0x93, 0x9C);
static const NVGcolor C_SCREEN = nvgRGB(0x0D, 0x0E, 0x11);

struct Divskip;

// SHAPE knob: its meaning, default and tooltip follow the channel's mode
struct ShapeQuantity : ParamQuantity {
	int index = 0;
	int mode();
	float getDefaultValue() override { return SHAPE_DEFAULTS[mode()]; }
	std::string getDisplayValueString() override;
	std::string getLabel() override {
		const char* n = SHAPE_NAMES[mode()];
		return string::f("Channel %d shape: %s", index + 1, n ? n : "not used in this mode");
	}
};

// what SHAPE is set to, in words (display and tooltip)
static std::string shapeText(int mode, float p, float q) {
	switch (mode) {
		case COIN:
		case STICKY: return loopSteps(q) ? string::f("loop %d", loopSteps(q)) : "no loop";
		case DIVIDE: return string::f("rotate %d", divideRotate(q));
		case TURING: return maskLength(q) ? string::f("mask %d", maskLength(q)) : "no mask";
		case EUCLID: return string::f("%d hits", euclidHits(p, q));
		case RAMP: return string::f("%d hits", rampHits(q));
		case RATCHET: return ratchetDivide(q) > 1 ? string::f("divide /%d first", ratchetDivide(q)) : "no divide";
		case RANDOM: return string::f("%d steps", randomSteps(q));
		default: return "";
	}
}

struct Divskip : Module {
	enum ParamId {
		ENUMS(AMOUNT_PARAM, 4),
		ENUMS(MODE_PARAM, 4),
		ENUMS(MUTE_PARAM, 4),
		OUTMODE_PARAM,
		MUTESET_PARAM,
		ENUMS(SHAPE_PARAM, 4),
		NEWSEED_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(CLOCK_INPUT, 4),
		ENUMS(MODECV_INPUT, 4),
		ENUMS(CV_INPUT, 4),
		RESET_INPUT,
		ENUMS(SHAPECV_INPUT, 4),
		ENUMS(SEED_INPUT, 4),
		NEWSEED_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(A_OUTPUT, 4),
		ENUMS(B_OUTPUT, 4),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(MUTE_LIGHT, 4 * 3),
		ENUMS(A_LIGHT, 4),
		ENUMS(B_LIGHT, 4),
		NEWSEED_LIGHT,
		LIGHTS_LEN
	};

	Channel ch[4];
	dsp::SchmittTrigger clockTrig[4], resetTrig, seedTrig[4], newSeedTrig;
	dsp::BooleanTrigger newSeedButton;
	dsp::PulseGenerator newSeedFlash;
	bool gateGap = false;    // right-click option: gates end 2 ms early
	// SHAPE: each mode keeps its own knob position, per channel
	float shapeMem[4][MODE_COUNT];
	int shapeMode[4] = {-1, -1, -1, -1};   // the mode the knob is showing
	dsp::BooleanTrigger muteTrig[4];
	dsp::PulseGenerator flashA[4], flashB[4];
	bool mutes[2][4] = {};
	// CHAIN: chained[i] links channel i to the one above (i = 1..3). A chain
	// plays its channels' loops one after another as one long loop.
	bool chained[4] = {};
	int turn[4] = {};        // per chain leader: which member is playing
	int turnCount[4] = {};   // clocks the playing member has had
	bool turnOf[4] = {};     // for the display
	float chainSince[4] = {};  // per chain leader: time since the last clock
	bool chainIdle[4] = {};
	float chainPeriod[4] = {0.25f, 0.25f, 0.25f, 0.25f};
	bool lastOutA[4] = {}, lastOutB[4] = {};

	// for the display (written by the audio thread)
	int shownMode[4] = {};
	float shownAmount[4] = {};
	float shownShape[4] = {};

	Divskip() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> modeNames;
		for (int m = 0; m < MODE_COUNT; m++) modeNames.push_back(MODE_LONG_NAMES[m]);
		for (int i = 0; i < 4; i++) {
			std::string n = string::f("Channel %d", i + 1);
			configParam(AMOUNT_PARAM + i, 0.f, 1.f, 0.5f, n + " amount", "%", 0.f, 100.f);
			ShapeQuantity* sq = configParam<ShapeQuantity>(SHAPE_PARAM + i, 0.f, 1.f, 0.f, n + " shape");
			sq->index = i;
			sq->randomizeEnabled = false;
			for (int m = 0; m < MODE_COUNT; m++) shapeMem[i][m] = SHAPE_DEFAULTS[m];
			configInput(SHAPECV_INPUT + i, n + " shape CV (+-5 V, added to the knob)");
			configInput(SEED_INPUT + i, n + " new seed (trigger)");
			configSwitch(MODE_PARAM + i, 0.f, MODE_COUNT - 1, i == 3 ? PATTERN : DIVIDE, n + " mode", modeNames);
			configButton(MUTE_PARAM + i, n + " mute");
			configInput(CLOCK_INPUT + i, i == 0 ? n + " clock / triggers"
			            : n + " clock / triggers (normalled from the channel above)");
			configInput(MODECV_INPUT + i, n + " mode CV (each 1 V moves one mode on)");
			configInput(CV_INPUT + i, n + " amount CV (+-5 V, added to the knob)");
			configOutput(A_OUTPUT + i, n + " A");
			configOutput(B_OUTPUT + i, n + " B");
			ch[i].kit = i;
			ch[i].seed(0x1234567u * (i + 1) + (uint32_t) random::u32());
		}
		configSwitch(OUTMODE_PARAM, 0.f, 1.f, 0.f, "Outputs", {"Triggers", "Gates"});
		configSwitch(MUTESET_PARAM, 0.f, 1.f, 0.f, "Mute set", {"M1", "M2"});
		configInput(RESET_INPUT, "Reset (all channels back to step 1)");
		configButton(NEWSEED_PARAM, "New seed (all channels in COIN, TURING, RANDOM or STICKY)");
		configInput(NEWSEED_INPUT, "New seed, all channels (trigger)");
	}

	float shapeOf(int i) {
		float q = params[SHAPE_PARAM + i].getValue();
		if (inputs[SHAPECV_INPUT + i].isConnected()) q += inputs[SHAPECV_INPUT + i].getVoltage() / 5.f;
		return clamp(q, 0.f, 1.f);
	}

	// the SHAPE knob follows the mode: switching mode brings back that mode's setting
	void followShape(int i, int mode) {
		if (shapeMode[i] != mode) {
			params[SHAPE_PARAM + i].setValue(shapeMem[i][mode]);
			shapeMode[i] = mode;
		}
		else shapeMem[i][mode] = params[SHAPE_PARAM + i].getValue();
	}

	int set() { return params[MUTESET_PARAM].getValue() > 0.5f ? 1 : 0; }

	int modeOf(int i) {
		int m = (int) std::round(params[MODE_PARAM + i].getValue());
		if (inputs[MODECV_INPUT + i].isConnected()) m += (int) std::floor(inputs[MODECV_INPUT + i].getVoltage() + 0.5f);
		m %= MODE_COUNT;
		if (m < 0) m += MODE_COUNT;
		return m;
	}

	float amountOf(int i) {
		float p = params[AMOUNT_PARAM + i].getValue();
		if (inputs[CV_INPUT + i].isConnected()) p += inputs[CV_INPUT + i].getVoltage() / 5.f;
		return clamp(p, 0.f, 1.f);
	}

	int leaderOf(int i) {
		while (i > 0 && chained[i]) i--;
		return i;
	}

	void process(const ProcessArgs& args) override {
		bool reset = resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f);
		bool gate = params[OUTMODE_PARAM].getValue() > 0.5f;
		int s = set();
		bool newSeedAll = newSeedTrig.process(inputs[NEWSEED_INPUT].getVoltage(), 0.1f, 1.f);
		if (newSeedButton.process(params[NEWSEED_PARAM].getValue() > 0.5f)) newSeedAll = true;
		if (newSeedAll) newSeedFlash.trigger(0.15f);
		lights[NEWSEED_LIGHT].setBrightness(newSeedFlash.process(args.sampleTime) ? 1.f : 0.f);
		float clockV = 0.f;
		bool clocks[4];
		for (int i = 0; i < 4; i++) {
			// clock inputs are normalled down: an empty input uses the one above
			if (inputs[CLOCK_INPUT + i].isConnected()) clockV = inputs[CLOCK_INPUT + i].getVoltage();
			clocks[i] = clockTrig[i].process(clockV, 0.1f, 1.f);
		}
		// a chain with no clock for 3 s goes back to its first channel, step 1
		for (int i = 0; i < 4; i++) {
			bool leads = (i == 0 || !chained[i]) && i < 3 && chained[i + 1];
			if (!leads) continue;
			chainSince[i] += args.sampleTime;
			if (clocks[i]) {
				if (chainSince[i] < TIMEOUT) chainPeriod[i] = chainSince[i];
				chainSince[i] = 0.f;
				chainIdle[i] = false;
			}
			if ((chainSince[i] > TIMEOUT && !chainIdle[i]) || reset) {
				chainIdle[i] = chainSince[i] > TIMEOUT;
				turn[i] = turnCount[i] = 0;
				for (int k = i; k < 4 && (k == i || chained[k]); k++) ch[k].restart();
			}
		}
		bool outA[4], outB[4];
		for (int i = 0; i < 4; i++) {
			if (muteTrig[i].process(params[MUTE_PARAM + i].getValue() > 0.5f)) mutes[s][i] = !mutes[s][i];
			bool muted = mutes[s][i];
			int mode = modeOf(i);
			followShape(i, mode);
			float p = amountOf(i);
			float q = shapeOf(i);
			shownMode[i] = mode;
			shownAmount[i] = p;
			shownShape[i] = q;
			bool seedHere = seedTrig[i].process(inputs[SEED_INPUT + i].getVoltage(), 0.1f, 1.f);
			if ((newSeedAll || seedHere) && usesSeed(mode)) ch[i].newSeed();

			// in a chain, everyone runs on the leader's clock and only the
			// member whose turn it is gets it
			int lead = leaderOf(i);
			int pos = i - lead;
			bool inChain = lead != i || (i < 3 && chained[i + 1]);
			bool clock = clocks[lead];
			if (inChain) {
				bool mine = turn[lead] == pos;
				turnOf[i] = mine;
				ch[i].frozen = true;   // the chain does the 3 s reset (above)
				ch[i].fixedPeriod = chainPeriod[lead];
				if (!mine) clock = false;
				if (clock) {
					turnCount[lead]++;
					if (turnCount[lead] >= ch[i].loopLength(mode, p, q)) {
						turnCount[lead] = 0;
						int members = 1;
						while (lead + members < 4 && chained[lead + members]) members++;
						turn[lead] = (turn[lead] + 1) % members;
					}
				}
			}
			else {
				turnOf[i] = true;
				ch[i].frozen = false;
				ch[i].fixedPeriod = 0.f;
			}

			ch[i].process(args.sampleTime, clock, reset, mode, p, q, gate, gateGap, muted, outA[i], outB[i]);
		}
		// a chain's top channel also sends out the whole chain
		bool sendA[4], sendB[4];
		for (int i = 0; i < 4; i++) { sendA[i] = outA[i]; sendB[i] = outB[i]; }
		for (int i = 1; i < 4; i++)
			if (chained[i]) {
				int lead = leaderOf(i);
				sendA[lead] = sendA[lead] || outA[i];
				sendB[lead] = sendB[lead] || outB[i];
			}
		for (int i = 0; i < 4; i++) {
			bool a = sendA[i], b = sendB[i];
			outputs[A_OUTPUT + i].setVoltage(a ? 10.f : 0.f);
			outputs[B_OUTPUT + i].setVoltage(b ? 10.f : 0.f);
			if (outA[i] && !lastOutA[i]) flashA[i].trigger(0.06f);
			if (outB[i] && !lastOutB[i]) flashB[i].trigger(0.06f);
			lights[A_LIGHT + i].setBrightness(outA[i] || flashA[i].process(args.sampleTime) ? 1.f : 0.f);
			lights[B_LIGHT + i].setBrightness(outB[i] || flashB[i].process(args.sampleTime) ? 1.f : 0.f);
			lastOutA[i] = outA[i];
			lastOutB[i] = outB[i];

			// mute button: cyan for M1, orange for M2
			NVGcolor c = s ? C_M2 : C_M1;
			float on = mutes[s][i] ? 1.f : 0.f;
			lights[MUTE_LIGHT + 3 * i + 0].setBrightness(on * c.r);
			lights[MUTE_LIGHT + 3 * i + 1].setBrightness(on * c.g);
			lights[MUTE_LIGHT + 3 * i + 2].setBrightness(on * c.b);
		}
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int k = 0; k < 2; k++)
			for (int i = 0; i < 4; i++) mutes[k][i] = false;
		for (int i = 0; i < 4; i++) {
			ch[i].restart();
			chained[i] = false;
			turn[i] = turnCount[i] = 0;
			for (int m = 0; m < MODE_COUNT; m++) shapeMem[i][m] = SHAPE_DEFAULTS[m];
			shapeMode[i] = -1;
		}
		gateGap = false;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* m = json_array();
		for (int k = 0; k < 2; k++)
			for (int i = 0; i < 4; i++) json_array_append_new(m, json_boolean(mutes[k][i]));
		json_object_set_new(root, "mutes", m);
		json_t* t = json_array();
		for (int i = 0; i < 4; i++) json_array_append_new(t, json_integer(ch[i].reg));
		json_object_set_new(root, "turing", t);
		json_t* r = json_array();
		for (int i = 0; i < 4; i++) json_array_append_new(r, json_integer(ch[i].rmask));
		json_object_set_new(root, "random", r);
		json_t* c = json_array();
		for (int i = 0; i < 4; i++) json_array_append_new(c, json_boolean(chained[i]));
		json_object_set_new(root, "chained", c);
		json_object_set_new(root, "gateGap", json_boolean(gateGap));
		json_t* sh = json_array();
		for (int i = 0; i < 4; i++) {
			json_t* row = json_array();
			for (int m = 0; m < MODE_COUNT; m++) json_array_append_new(row, json_real(shapeMem[i][m]));
			json_array_append_new(sh, row);
		}
		json_object_set_new(root, "shapes", sh);
		json_t* sd = json_array();
		for (int i = 0; i < 4; i++) {
			json_t* row = json_array();
			json_array_append_new(row, json_integer(ch[i].mask));
			for (int k = 0; k < 32; k++) json_array_append_new(row, json_real(ch[i].coinU[k]));
			for (int k = 0; k < 32; k++) json_array_append_new(row, json_real(ch[i].stickU[k]));
			json_array_append_new(sd, row);
		}
		json_object_set_new(root, "seeds", sd);
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* m = json_object_get(root, "mutes");
		if (m)
			for (int k = 0; k < 2; k++)
				for (int i = 0; i < 4; i++) {
					json_t* v = json_array_get(m, k * 4 + i);
					if (v) mutes[k][i] = json_is_true(v);
				}
		json_t* t = json_object_get(root, "turing");
		if (t)
			for (int i = 0; i < 4; i++) {
				json_t* v = json_array_get(t, i);
				if (v) ch[i].reg = (uint16_t) json_integer_value(v);
			}
		json_t* r = json_object_get(root, "random");
		if (r)
			for (int i = 0; i < 4; i++) {
				json_t* v = json_array_get(r, i);
				if (v) ch[i].rmask = (uint32_t) json_integer_value(v);
			}
		json_t* c = json_object_get(root, "chained");
		if (c)
			for (int i = 1; i < 4; i++) {
				json_t* v = json_array_get(c, i);
				if (v) chained[i] = json_is_true(v);
			}
		json_t* g = json_object_get(root, "gateGap");
		if (g) gateGap = json_is_true(g);
		json_t* sh = json_object_get(root, "shapes");
		if (sh)
			for (int i = 0; i < 4; i++) {
				json_t* row = json_array_get(sh, i);
				for (int m = 0; row && m < MODE_COUNT; m++) {
					json_t* v = json_array_get(row, m);
					if (v) shapeMem[i][m] = (float) json_number_value(v);
				}
				// the knob's saved value belongs to the saved mode: start from it
				shapeMode[i] = (int) std::round(params[MODE_PARAM + i].getValue());
				if (shapeMode[i] >= 0 && shapeMode[i] < MODE_COUNT)
					shapeMem[i][shapeMode[i]] = params[SHAPE_PARAM + i].getValue();
			}
		json_t* sd = json_object_get(root, "seeds");
		if (sd)
			for (int i = 0; i < 4; i++) {
				json_t* row = json_array_get(sd, i);
				if (!row || json_array_size(row) < 65) continue;
				ch[i].mask = (uint8_t) json_integer_value(json_array_get(row, 0));
				for (int k = 0; k < 32; k++) ch[i].coinU[k] = (float) json_number_value(json_array_get(row, 1 + k));
				for (int k = 0; k < 32; k++) ch[i].stickU[k] = (float) json_number_value(json_array_get(row, 33 + k));
			}
	}
};

// ---- the light ring around each AMOUNT knob ---------------------------------
struct StepRing : Widget {
	Divskip* module = nullptr;
	int index = 0;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		NVGcontext* vg = args.vg;
		Vec c = box.size.div(2.f);
		float r = mm2px(7.3f), dot = mm2px(0.75f);
		bool a[16], b[16];
		int play = -1;
		NVGcolor col = MODE_COLORS[DIVIDE];
		bool pattern = false, muted = false;
		int mode = DIVIDE;
		float p = 0.5f, q = 0.f;
		NVGcolor muteCol = C_M1;
		if (module) {
			mode = module->shownMode[index];
			p = module->shownAmount[index];
			q = module->shownShape[index];
			col = MODE_COLORS[mode];
			int s = module->set();
			muted = module->mutes[s][index];
			muteCol = s ? C_M2 : C_M1;
			pattern = module->ch[index].ring(mode, p, q, a, b, play);
		}
		else {
			for (int i = 0; i < 16; i++) { a[i] = i % 4 == 0; b[i] = i % 8 == 6; }
			pattern = true;
		}
		for (int i = 0; i < 16; i++) {
			NVGcolor dc = C_OFF;
			if (muted) dc = nvgTransRGBA(muteCol, 150);
			else if (pattern) {
				if (i == play) dc = nvgRGB(0xFF, 0xFF, 0xFF);
				else if (a[i]) dc = col;
				else if (b[i]) dc = C_B;
			}
			else {
				// COIN, STICKY, RAMP, RATCHET: the knob position, A side in colour, B side cyan
				float pos = p * 15.f;
				if (mode == COIN || mode == STICKY) dc = (i <= pos) ? col : nvgTransRGBA(C_B, 200);
				else if (i <= pos + 0.5f) dc = col;
			}
			float ang = i * (2.f * M_PI / 16.f);
			nvgBeginPath(vg);
			nvgCircle(vg, c.x + r * std::sin(ang), c.y - r * std::cos(ang), dot);
			nvgFillColor(vg, dc);
			nvgFill(vg);
		}
	}
};

// ---- the display beside each knob: mode, setting, A/B flashes ---------------
struct ModeDisplay : Widget {
	Divskip* module = nullptr;
	int index = 0;

	ParamQuantity* modeQ() {
		return module ? module->paramQuantities[Divskip::MODE_PARAM + index] : nullptr;
	}

	std::string valueText(int mode, float p, bool gate) {
		switch (mode) {
			case COIN: {
				int pb = (int) std::round(p * 100.f);
				if (pb <= 0) return "always A";
				if (pb >= 100) return "always B";
				return string::f("A %d%%  B %d%%", 100 - pb, pb);
			}
			case DIVIDE: return string::f("A /%d  B /%d", divideA(p), 16 - divideA(p));
			case TURING:
				if (turingAlwaysOn(p)) return "always A";
				if (p < 0.5f) return string::f("loop %d  locked", turingLength(p));
				return string::f("16  flip %d%%", (int) std::round(turingFlip(p) * 100.f));
			case SPLIT: return string::f("A 3/%d  B 3/%d", splitA(p), 16 - splitA(p));
			case EUCLID: return string::f("length %d", euclidLength(p));
			case RAMP:
				if (gate) return string::f("A gate %d%%", (int) std::round(gateFraction(RAMP, p) * 100.f));
				if (p < 0.4f) return "speed up";
				if (p > 0.6f) return "slow down";
				return "up, then down";
			case RATCHET:
				if (gate) return string::f("A gate %d%%", (int) std::round(gateFraction(RATCHET, p) * 100.f));
				return string::f("A x%d  B x%d", ratchetA(p), 16 - ratchetA(p));
			case PATTERN: return PATTERNS[index & 3][patternIndex(p)].name;
			case RANDOM: return string::f("%d hits", randomHits(p, module ? module->shownShape[index] : SHAPE_DEFAULTS[RANDOM]));
			case STICKY: {
				if (p < 0.03f) return "swap every hit";
				return string::f("repeat %d%%", (int) std::round(stickyRepeat(p) * 100.f));
			}
		}
		return "";
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		NVGcontext* vg = args.vg;
		float W = box.size.x, H = box.size.y;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, W, H, mm2px(1.f));
		nvgFillColor(vg, C_SCREEN);
		nvgFill(vg);
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (font && font->handle >= 0) nvgFontFaceId(vg, font->handle);

		int mode = module ? module->shownMode[index] : (index == 3 ? PATTERN : DIVIDE);
		float p = module ? module->shownAmount[index] : 0.5f;
		bool gate = module && module->params[Divskip::OUTMODE_PARAM].getValue() > 0.5f;
		bool muted = module && module->mutes[module->set()][index];
		float pad = mm2px(1.6f);

		bool chainUp = module && index > 0 && module->chained[index];
		bool chainDown = module && index < 3 && module->chained[index + 1];
		bool waiting = module && (chainUp || chainDown) && !module->turnOf[index];
		if (chainUp || chainDown) {
			// a link bar down the left edge, joining the chained displays
			nvgBeginPath(vg);
			nvgRect(vg, 0, chainUp ? 0 : H * 0.5f, mm2px(0.7f), chainDown ? H - (chainUp ? 0 : H * 0.5f) : (chainUp ? H * 0.5f : 0));
			nvgFillColor(vg, C_INK);
			nvgFill(vg);
		}
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
		nvgFontSize(vg, mm2px(3.1f));
		nvgFillColor(vg, waiting ? nvgTransRGBA(MODE_COLORS[mode], 90) : MODE_COLORS[mode]);
		nvgText(vg, pad, mm2px(4.4f), string::f("%d %s", index + 1, MODE_NAMES[mode]).c_str(), NULL);

		std::string v = valueText(mode, p, gate);
		if (v.size() > 16) v = v.substr(0, 16);
		nvgFontSize(vg, mm2px(2.5f));
		nvgFillColor(vg, muted ? (module->set() ? C_M2 : C_M1) : C_INK);
		nvgText(vg, pad, mm2px(8.3f), muted ? (v.substr(0, 9) + " MUTED").c_str() : v.c_str(), NULL);
		std::string sh = shapeText(mode, p, module ? module->shownShape[index] : SHAPE_DEFAULTS[mode]);
		if (sh.size() > 16) sh = sh.substr(0, 16);
		nvgFillColor(vg, C_SOFT);
		nvgText(vg, pad, mm2px(11.8f), sh.c_str(), NULL);

		// A and B flash dots
		float la = module ? module->lights[Divskip::A_LIGHT + index].getBrightness() : 0.f;
		float lb = module ? module->lights[Divskip::B_LIGHT + index].getBrightness() : 0.f;
		const float dx[2] = {W - mm2px(4.6f), W - mm2px(1.9f)};
		for (int k = 0; k < 2; k++) {
			nvgBeginPath(vg);
			nvgCircle(vg, dx[k], mm2px(3.1f), mm2px(0.9f));
			NVGcolor on = k == 0 ? nvgRGB(0xF0, 0xA0, 0x4B) : C_B;
			nvgFillColor(vg, (k == 0 ? la : lb) > 0.5f ? on : nvgRGB(0x3A, 0x3D, 0x44));
			nvgFill(vg);
		}
	}

	void onButton(const ButtonEvent& e) override {
		ParamQuantity* q = modeQ();
		if (!q || e.action != GLFW_PRESS) return;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
			// ctrl/cmd-click: new seed for this channel
			reroll();
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			// click: next mode; shift-click: previous
			int m = (int) std::round(q->getValue());
			m += (e.mods & GLFW_MOD_SHIFT) ? MODE_COUNT - 1 : 1;
			setMode(m % MODE_COUNT);
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel(string::f("Channel %d mode", index + 1)));
			int cur = (int) std::round(q->getValue());
			for (int m = 0; m < MODE_COUNT; m++)
				menu->addChild(createCheckMenuItem(MODE_LONG_NAMES[m], "",
					[=]() { return cur == m; },
					[=]() { setMode(m); }));
			menu->addChild(new MenuSeparator);
			menu->addChild(createMenuItem("New seed for this channel", RACK_MOD_CTRL_NAME "+click", [=]() { reroll(); }));
			if (index > 0) {
				Divskip* m = module;
				int i = index;
				menu->addChild(createCheckMenuItem(string::f("Chain to channel %d (play after it)", i), "",
					[=]() { return m->chained[i]; },
					[=]() { m->chained[i] = !m->chained[i]; for (int k = 0; k < 4; k++) m->turn[k] = m->turnCount[k] = 0; }));
			}
			e.consume(this);
		}
	}

	void reroll() {
		if (!module) return;
		module->ch[index].newSeed();
	}

	void setMode(int m) {
		ParamQuantity* q = modeQ();
		if (!q) return;
		float old = q->getValue();
		q->setValue((float) m);
		// undo history
		history::ParamChange* h = new history::ParamChange;
		h->name = "change mode";
		h->moduleId = module->id;
		h->paramId = Divskip::MODE_PARAM + index;
		h->oldValue = old;
		h->newValue = (float) m;
		APP->history->push(h);
	}
};

// ---- dims the SHAPE knob in modes that don't use it -------------------------
struct ShapeDim : Widget {
	Divskip* module = nullptr;
	int index = 0;
	void draw(const DrawArgs& args) override {
		if (!module || SHAPE_NAMES[module->shownMode[index]]) return;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, box.size.x / 2, box.size.y / 2, box.size.x / 2);
		nvgFillColor(args.vg, nvgTransRGBA(C_PANEL, 170));
		nvgFill(args.vg);
	}
};

int ShapeQuantity::mode() {
	Divskip* d = dynamic_cast<Divskip*>(module);
	if (!d) return COIN;
	int m = d->shownMode[index];
	return (m >= 0 && m < MODE_COUNT) ? m : COIN;
}

std::string ShapeQuantity::getDisplayValueString() {
	Divskip* d = dynamic_cast<Divskip*>(module);
	int m = mode();
	if (!SHAPE_NAMES[m]) return "-";
	float p = d ? d->shownAmount[index] : 0.5f;
	return shapeText(m, p, getValue());
}

// ---- panel -----------------------------------------------------------------
struct DivskipWidget : ModuleWidget {
	DivskipWidget(Divskip* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Divskip.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		for (int i = 0; i < 4; i++) {
			float y = ROWS[i];
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_IN, y)), module, Divskip::CLOCK_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_MODE, y)), module, Divskip::MODECV_INPUT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_CV, y)), module, Divskip::CV_INPUT + i));

			StepRing* ring = createWidget<StepRing>(mm2px(Vec(X_KNOB - 8.5f, y - 8.5f)));
			ring->box.size = mm2px(Vec(17.f, 17.f));
			ring->module = module;
			ring->index = i;
			addChild(ring);
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(X_KNOB, y)), module, Divskip::AMOUNT_PARAM + i));

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_CV2, y)), module, Divskip::SHAPECV_INPUT + i));
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(X_SHAPE, y)), module, Divskip::SHAPE_PARAM + i));
			ShapeDim* dim = createWidget<ShapeDim>(mm2px(Vec(X_SHAPE - 5.6f, y - 5.6f)));
			dim->box.size = mm2px(Vec(11.2f, 11.2f));
			dim->module = module;
			dim->index = i;
			addChild(dim);

			ModeDisplay* d = createWidget<ModeDisplay>(mm2px(Vec(X_DISP0, y - 6.5f)));
			d->box.size = mm2px(Vec(X_DISP1 - X_DISP0, 13.f));
			d->module = module;
			d->index = i;
			addChild(d);

			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_SEEDIN, y)), module, Divskip::SEED_INPUT + i));
			addParam(createLightParamCentered<VCVLightBezel<RedGreenBlueLight>>(mm2px(Vec(X_MUTE, y)), module,
				Divskip::MUTE_PARAM + i, Divskip::MUTE_LIGHT + 3 * i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(X_A, y)), module, Divskip::A_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(X_B, y)), module, Divskip::B_OUTPUT + i));
		}
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_RESET, BOT_Y)), module, Divskip::RESET_INPUT));
		addParam(createParamCentered<CKSS>(mm2px(Vec(X_OUTSW, BOT_Y)), module, Divskip::OUTMODE_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(X_MUTESW, BOT_Y)), module, Divskip::MUTESET_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(X_GSEEDIN, BOT_Y)), module, Divskip::NEWSEED_INPUT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(X_GSEEDBTN, BOT_Y)), module,
			Divskip::NEWSEED_PARAM, Divskip::NEWSEED_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Divskip* m = dynamic_cast<Divskip*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		for (int i = 0; i < 4; i++) {
			menu->addChild(createIndexSubmenuItem(string::f("Channel %d mode", i + 1),
				std::vector<std::string>(MODE_LONG_NAMES, MODE_LONG_NAMES + MODE_COUNT),
				[=]() { return (size_t) std::round(m->params[Divskip::MODE_PARAM + i].getValue()); },
				[=](size_t v) { m->params[Divskip::MODE_PARAM + i].setValue((float) v); }));
		}
		for (int i = 1; i < 4; i++)
			menu->addChild(createCheckMenuItem(string::f("Chain channel %d after channel %d", i + 1, i), "",
				[=]() { return m->chained[i]; },
				[=]() { m->chained[i] = !m->chained[i]; for (int k = 0; k < 4; k++) m->turn[k] = m->turnCount[k] = 0; }));
		menu->addChild(createMenuItem("Clear mutes in both sets", "", [=]() {
			for (int k = 0; k < 2; k++)
				for (int i = 0; i < 4; i++) m->mutes[k][i] = false;
		}));
		menu->addChild(createBoolPtrMenuItem("Gate gap (gates end 2 ms early, so back-to-back gates stay separate)", "", &m->gateGap));
	}
};

Model* modelDivskip = createModel<Divskip, DivskipWidget>("Divskip");
