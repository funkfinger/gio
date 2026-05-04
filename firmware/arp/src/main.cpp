#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <cstdlib>
#include <Adafruit_NeoPixel.h>
#include "arp.h"
#include "tempo.h"
#include "scales.h"
#include "cv.h"
#include "oled_ui.h"
#include "encoder_input.h"
#include "outputs.h"
#include "inputs.h"

// Platform-pivot migration (decisions.md §19–§25): firmware now drives the
// hardware via the SPI HAL (lib/outputs/ + lib/inputs/) instead of the old
// PWM-DAC + analog-CV + digital-gate paths. See bench-wiring.md for the
// breadboard topology this code targets.
//
// Jack roles after the pivot (per decisions.md §23 — generic IN/OUT labels):
//   J1 (MCP3208 CH1) — CV transpose in (replaces old PIN_CV/A1 analog read)
//   J2 (MCP3208 CH2) — second CV in (deferred — input scaling stage ch B not yet wired)
//   J3 (DAC ch A)    — V/Oct out via inverting bipolar output stage (replaces PIN_DAC_PWM)
//   J4 (DAC ch B)    — gate out via DAC ch B (replaces digital PIN_GATE on D6)
//
// External clock-in on J2 is dormant until input ch B is wired (a Schmitt on
// inputs::readVolts(ADC_CH_CV_IN_2) replaces the old digitalRead(D3) edge detector). Until
// then, the arp runs from the internal tempo pot only.

// ------------------------------ pins -------------------------------
// MCP3208 channel layout (decisions.md §26, finalized 2026-05-03):
//   CH0 = primary pot (tempo today; "primary pot" generically)
//   CH1 = J1 input (CV / trigger / audio — interpretation per app)
//   CH2 = J2 input (same flexibility)
//   CH3–CH7 = expansion / daughterboard
// Conventionally: pots/internal controls at low channels, jacks contiguous
// after, expansion at high channels.
static const uint8_t  ADC_CH_POT     = 0;    // MCP3208 CH0 — primary pot wiper (tempo in this app)
static const uint8_t  ADC_CH_CV_IN_1 = 1;    // MCP3208 CH1 — J1 input
static const uint8_t  ADC_CH_CV_IN_2 = 2;    // MCP3208 CH2 — J2 input (when scaling stage built)
// SPI bus: SCK=D8/GP2, MISO=D9/GP4, MOSI=D10/GP3 (handled by SPI.begin())
static const uint8_t  PIN_CS_DAC     = D3;   // GP5 — DAC8552 /SYNC
static const uint8_t  PIN_CS_ADC     = D6;   // GP0 — MCP3208 /CS
// Encoder pins migrated from D8/D9/D10 (now SPI bus) to D1/D2/D7. The A/B
// firmware-swap from 2026-05-02 was REVERTED on 2026-05-04: the breadboard
// wires were physically swapped so the schematic's natural assignment (A→D1,
// B→D2) reads CW as positive, instead of the firmware fighting the wiring.
// Wiring convention (matches schematic): encoder pin A → XIAO D1, B → XIAO D2.
static const uint8_t  PIN_ENC_A      = D1;   // GP27 (was D9/GP3 pre-pivot)
static const uint8_t  PIN_ENC_B      = D2;   // GP28 (was D8/GP2 pre-pivot)
static const uint8_t  PIN_ENC_CLICK  = D7;   // GP1  (was D10/GP4 pre-pivot)
// I²C: D4/D5 — handled by Wire.begin().
// PIN_LED — GPIO25, active LOW.

// Onboard RGB NeoPixel:
static const uint8_t PIN_NEOPIXEL_DATA   = 22;
static const uint8_t PIN_NEOPIXEL_POWER  = 23;
static const uint8_t NEOPIXEL_BRIGHTNESS = 64;

// ------------------------------ Analog reference -------------------
// Bench: pot+TL072 stand-in dialled to 4.096 V. Production: REF3040 at 4.096 V
// (decisions.md §25). Both DAC8552 and MCP3208 share this VREF so the system
// is ratiometric — VREF drift cancels across the loop.
static const float    BENCH_VREF_V  = 4.096f;
static const uint16_t ADC_MAX       = 4095;     // MCP3208 12-bit full-scale

// Music range
static const uint8_t MIDI_NOTE_MIN = 24;   // C1
static const uint8_t MIDI_NOTE_MAX = 96;   // C7

// ------------------------------ Clock-in ---------------------------
// External clock detection on J2 is deferred until the input ch B scaling
// stage is built on the bench (Story 015 channel B). When it lands, this
// stage will read inputs::readVolts(ADC_CH_CV_IN_2) and feed it to a Schmitt trigger
// (lib/trigger_in/) for edge detection. For now the arp runs internally only.
static const uint32_t EXT_CLOCK_TIMEOUT_MS = 2000;

// ----------------------------- music -------------------------------
// Chord stored as semitone INTERVALS from the active key. Key tonic is
// added at play-time. Default chord shape: major triad + octave.
static const int8_t CHORD_INTERVALS[] = {0, 4, 7, 12};
static const uint8_t CHORD_LEN        = 4;
static const uint8_t SUBDIV           = 4;       // 16th notes
static const float   GATE_FRAC        = 0.5f;

// Pitch class names for the KEY menu.
static const char* PITCH_CLASS_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// MIDI note number for KEY pitch-class index 0 (= C). The encoder KEY param
// pins the chord/scale tonic inside MIDI octave 3 (C3=48 .. B3=59).
static const uint8_t KEY_OCTAVE_BASE_MIDI = 48;

// ----------------------------- menu --------------------------------
enum class Param : uint8_t { Scale = 0, Order, Key, Trigger, COUNT };

static const char* paramName(Param p) {
    switch (p) {
        case Param::Scale:   return "SCALE";
        case Param::Order:   return "ORDER";
        case Param::Key:     return "KEY";
        case Param::Trigger: return "TRIG";
        default:             return "?";
    }
}
static const char* scaleName(Scale s) {
    switch (s) {
        case Scale::Major:           return "Major";
        case Scale::NaturalMinor:    return "Nat Min";
        case Scale::PentatonicMajor: return "Pent Maj";
        case Scale::PentatonicMinor: return "Pent Min";
        case Scale::Dorian:          return "Dorian";
        case Scale::Chromatic:       return "Chrom";
        default:                     return "?";
    }
}
static const char* orderName(ArpOrder o) {
    switch (o) {
        case ArpOrder::Up:           return "Up";
        case ArpOrder::Down:         return "Down";
        case ArpOrder::UpDownClosed: return "UpDn-C";
        case ArpOrder::UpDownOpen:   return "UpDn-O";
        case ArpOrder::SkipUp:       return "SkipUp";
        case ArpOrder::Random:       return "Random";
        default:                     return "?";
    }
}
static const char* rootName(uint8_t pitchClass) {
    return PITCH_CLASS_NAMES[pitchClass % 12];
}

static inline uint8_t wrap(int v, uint8_t count) {
    int n = (int)count;
    int r = ((v % n) + n) % n;
    return (uint8_t)r;
}
static inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ----------------------------- state -------------------------------
static Arp           arp;
static Scale         scale       = Scale::Major;
static ArpOrder      order       = ArpOrder::Up;
static uint8_t       keyPC       = 0;             // 0=C, 1=C#, ... 11=B
static Param         active      = Param::Scale;

static OledUI        ui;
static EncoderInput  enc;
static Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL_DATA, NEO_GRBW + NEO_KHZ800);

static uint32_t lastStepMs       = 0;             // start of current note (note advance)
static uint32_t stepMs           = 125;           // total note length
static bool     gateOn           = false;
static uint32_t resetFlashUntilMs = 0;
static int      currentCvTranspose = 0;          // updated each step; used by display

// Trigger ratcheting (Story 019) — N gates per note. ratchet=1 = legacy
// behaviour; 2..4 fire sub-gates evenly within the note (same pitch, same
// step length). Composes with the external-clock multiplier — multiplier
// controls notes-per-pulse, ratchet controls gates-per-note.
static uint8_t  ratchet          = 1;             // 1..4
static uint32_t subStepMs        = 125;           // = stepMs / ratchet
static uint32_t subGateMs        = 62;            // = subStepMs * GATE_FRAC
static uint32_t lastSubGateMs    = 0;             // start of current sub-gate
static uint8_t  subGatesFired    = 0;             // sub-gates fired in current note

// External clock state
static bool     extClockHigh             = false; // current Schmitt-trigger output
static uint32_t extLastEdgeMs            = 0;     // millis() of last rising edge (0 = never)
static uint32_t extPrevEdgeMs            = 0;     // millis() of edge before that
static uint8_t  extMultiplier            = 2;     // 1, 2, or 4 steps per external pulse (from tempo pot)
static uint8_t  extStepsRemaining        = 0;     // interpolated steps still to fire before next pulse
static uint32_t extInterpolatedInterval  = 0;     // ms between interpolated steps (period / multiplier)
static bool     wasExtModeLast           = false; // for detecting mode transitions

// ---------------------- voltage ↔ note -----------------------------
static float midiToVolts(uint8_t midi) {
    return ((float)midi - 48.0f) / 12.0f;
}

// --------------------------- HAL helpers ---------------------------
// Gate on jack J4 (DAC ch B). outputs::gate() writes VREF for HIGH and 0 V
// for LOW; with the channel-B output stage's calibration that produces ~+4 V
// at the jack for HIGH and ~0 V for LOW — well within Eurorack gate spec.
static inline void gateWrite(bool on) {
    outputs::gate(1, on);
}
static inline void ledWrite(bool on) {
    digitalWrite(PIN_LED, on ? LOW : HIGH);
}

// --------------------------- CV reader -----------------------------
// inputs::readVolts(ADC_CH_CV_IN_1) returns calibrated jack-side volts on J1.
// The cv lib's volts-to-transpose function clamps negatives to 0 and >8 V to
// 8 V, so we can pass through the full ±10 V span without extra defensive
// code here.
static float readCvVolts() {
    return inputs::readVolts(ADC_CH_CV_IN_1);
}

// Hysteresis-wrapped transpose: only accept a new snapped value once voltage
// has moved more than ~half a semitone from the anchor. Kills ADC-noise
// flutter at the boundary between two scale-snapped values.
static int readCvTransposeWithHysteresis() {
    static int   held       = 0;
    static float heldVolts  = -1.0f;             // sentinel for first call
    const  float HYSTERESIS = 1.0f / 24.0f;       // ≈ 42 mV = ½ semitone
    float volts = readCvVolts();
    if (heldVolts < 0.0f) {
        held = cvVoltsToTranspose(volts, scale);
        heldVolts = volts;
        return held;
    }
    if (fabsf(volts - heldVolts) < HYSTERESIS) return held;
    held = cvVoltsToTranspose(volts, scale);
    heldVolts = volts;
    return held;
}

// --------------------------- engine glue ---------------------------
// Recompute sub-gate timing from a note length (called when stepMs or
// ratchet changes). Each sub-gate takes subStepMs from start to start;
// each gate stays HIGH for subGateMs (= subStepMs * GATE_FRAC).
static void recalcSubTiming(uint32_t noteMs) {
    if (ratchet < 1) ratchet = 1;
    subStepMs = noteMs / ratchet;
    if (subStepMs < 5) subStepMs = 5;             // sanity floor
    subGateMs = (uint32_t)((float)subStepMs * GATE_FRAC + 0.5f);
}

static void refreshTempoFromPot() {
    uint16_t raw = inputs::readRaw(ADC_CH_POT);
    if (raw > ADC_MAX) raw = ADC_MAX;
    float pot = (float)raw / (float)ADC_MAX;
    float bpm = tempo::potToBpm(pot);
    stepMs = tempo::bpmToStepMs(bpm, SUBDIV);
    recalcSubTiming(stepMs);
}

// Fire a sub-gate WITHOUT changing the DAC value — same pitch, just another
// trigger. Called between fireStep() invocations when ratchet > 1.
static void fireSubGate() {
    gateWrite(true);
    ledWrite(true);
    gateOn        = true;
    lastSubGateMs = millis();
    subGatesFired++;
}

// Read the tempo pot in external-clock mode and map it to a clock MULTIPLIER —
// how many arp steps to fire per external pulse. Three zones with hysteresis:
// bottom 1/3 → ×1 (one step per pulse), middle → ×2 (insert one step at the
// midpoint), top → ×4 (insert three at quarter-period spacing).
//
// The multiplier uses the previously-measured period to schedule interpolated
// steps — so it's a one-pulse-late approximation. If the source clock slows
// down between pulses, the interpolated steps still fire on the OLD interval
// and might overshoot. Acceptable for typical musical clock changes; would
// need rework if used with very wide tempo modulation.
static uint8_t externalMultiplierFromPot() {
    static uint8_t held = 2;                              // default ×2 — feels best at Eurorack rates
    static const uint16_t LOW_BOUND  = ADC_MAX / 3;       // ~1365
    static const uint16_t HIGH_BOUND = (2 * ADC_MAX) / 3; // ~2730
    static const uint16_t HYST       = 100;
    uint16_t raw = inputs::readRaw(ADC_CH_POT);
    if (raw > ADC_MAX) raw = ADC_MAX;
    if (held == 1 && raw > LOW_BOUND  + HYST) held = 2;
    if (held == 2 && raw < LOW_BOUND  - HYST) held = 1;
    if (held == 2 && raw > HIGH_BOUND + HYST) held = 4;
    if (held == 4 && raw < HIGH_BOUND - HYST) held = 2;
    return held;
}

// External clock edge detection — DEFERRED until input ch B (J2) scaling
// stage is built. Once it lands, this function will read inputs::readVolts(ADC_CH_CV_IN_2)
// through a Schmitt trigger (lib/trigger_in/) for edge detection. For now,
// always returns false — the arp runs from internal tempo only.
static bool pollExternalClockEdge() {
    return false;
}

static bool externalClockActive() {
    if (extLastEdgeMs == 0) return false;
    return (millis() - extLastEdgeMs) < EXT_CLOCK_TIMEOUT_MS;
}

static void renderMenu();              // fwd decl: defined below fireStep()
static void drawRandomOrderScreen();   // fwd decl: defined alongside renderMenu()

static void fireStep() {
    if (externalClockActive()) {
        // External mode: derive sub-step timing from the interpolated
        // interval (one external-clock note's duration). Composes ratchet
        // on top of the multiplier — multiplier controls how many notes per
        // pulse, ratchet controls how many gates per note.
        if (extInterpolatedInterval > 0) {
            recalcSubTiming(extInterpolatedInterval);
            // sanity-clamp the per-gate length so a stale interval can't
            // strand the gate
            if (subGateMs < 5)    subGateMs = 5;
            if (subGateMs > 5000) subGateMs = 50;
        }
    } else {
        refreshTempoFromPot();
    }

    // Sample CV (with hysteresis) and detect transpose change for display refresh.
    int newCvTranspose = readCvTransposeWithHysteresis();
    bool cvChanged     = (newCvTranspose != currentCvTranspose);
    currentCvTranspose = newCvTranspose;

    // keyMidi = encoder key + CV transpose (semitone-snap + scale-snap baked in).
    int  encKeyMidi = (int)KEY_OCTAVE_BASE_MIDI + (int)keyPC;
    int  keyMidi    = encKeyMidi + currentCvTranspose;

    // arp.nextNote() returns one of CHORD_INTERVALS — we registered intervals
    // as the arp's "notes" so we get an interval offset directly.
    int  intervalIdx = arp.nextNote();
    int  played      = keyMidi + intervalIdx;
    played           = (int)quantize((uint8_t)clampi(played, 0, 127), scale);
    played           = clampi(played, MIDI_NOTE_MIN, MIDI_NOTE_MAX);

    float volts = midiToVolts((uint8_t)played);

    outputs::write(0, volts);          // V/Oct → jack J3
    gateWrite(true);                    // gate → jack J4 (DAC ch B)
    ledWrite(true);
    gateOn        = true;
    lastStepMs    = millis();
    lastSubGateMs = lastStepMs;
    subGatesFired = 1;             // this fireStep() counts as the first sub-gate

    // Live KEY display: re-render menu if the effective key changed and
    // KEY is the active param. (Only KEY depends on CV — Scale/Order don't.)
    if (cvChanged && active == Param::Key) renderMenu();

    // Live ORDER:Random display: redraw the random bars on every step so
    // the visual updates per note advance — strong cue that the playback
    // is unpredictable. Only fires when the user is currently viewing it.
    if (active == Param::Order && order == ArpOrder::Random) drawRandomOrderScreen();
}

// --------------------------- UI helpers ----------------------------
static void updateNeoPixel() {
    uint8_t r = 0, g = 0, b = 0;
    if      (active == Param::Scale)   g = 255;
    else if (active == Param::Order)   b = 255;
    else if (active == Param::Key)     { r = 255; b = 255; } // magenta
    else if (active == Param::Trigger) { r = 255; g = 200; } // yellow/amber
    pixel.setPixelColor(0, pixel.Color(r, g, b));
    pixel.show();
}

static const char* activeValueName() {
    static char buf[8];
    switch (active) {
        case Param::Scale: return scaleName(scale);
        case Param::Order: return orderName(order);
        case Param::Key: {
            // Show the EFFECTIVE key: encoder keyPC + CV transpose, mod 12.
            // Trailing '*' indicates CV is contributing (so encoder rotation
            // alone doesn't fully account for what's playing).
            int eff = ((int)keyPC + currentCvTranspose);
            eff = ((eff % 12) + 12) % 12;
            const char* name = rootName((uint8_t)eff);
            if (currentCvTranspose != 0) {
                snprintf(buf, sizeof(buf), "%s*", name);
                return buf;
            }
            return name;
        }
        case Param::Trigger: {
            snprintf(buf, sizeof(buf), "%u", (unsigned)ratchet);
            return buf;
        }
        default: return "?";
    }
}

// Return the natural-note bitmap that anchors pitch class `pc`. For
// naturals, that's the screen for pc itself; for sharps, the screen for
// the natural just below (e.g. C# uses C, F# uses F). Returns nullptr if
// no art exists for the underlying natural — currently only G (pc 7) and
// therefore also G# (pc 8). The sharp glyph is composed on top by the
// caller when pcIsSharp() is true.
static const screens::Screen* keyNaturalScreen(uint8_t pc) {
    switch (pc) {
        case 0:  case 1:  return &screens::key_C;  // C, C#
        case 2:  case 3:  return &screens::key_D;  // D, D#
        case 4:           return &screens::key_E;  // E
        case 5:  case 6:  return &screens::key_F;  // F, F#
        case 7:  case 8:  return &screens::key_G;  // G, G#
        case 9:  case 10: return &screens::key_A;  // A, A#
        case 11:          return &screens::key_B;  // B
        default: return nullptr;
    }
}

// True for the five sharp pitch classes (C#, D#, F#, G#, A#).
static bool pcIsSharp(uint8_t pc) {
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

// Map an arp order to the bitmap that visualizes its playback pattern.
// Returns nullptr for ArpOrder::Random — that order is rendered procedurally
// (a fresh set of random vertical bars on every step) by drawRandomOrderScreen().
static const screens::Screen* orderScreen(ArpOrder o) {
    switch (o) {
        case ArpOrder::Up:           return &screens::order_up;
        case ArpOrder::Down:         return &screens::order_down;
        case ArpOrder::UpDownClosed: return &screens::order_up_down_closed;
        case ArpOrder::UpDownOpen:   return &screens::order_up_down_open;
        case ArpOrder::SkipUp:       return &screens::order_skip_up;
        case ArpOrder::Random:       return nullptr;     // procedural
        default:                     return nullptr;
    }
}

// Procedural visualization for ArpOrder::Random. Draws screens::order_random
// as the static backdrop, then overlays 4 vertical bars at random heights in
// the inset rectangle defined by:
//   top margin    19 px (leaves room for header text/logo in the backdrop)
//   left margin    3 px
//   right margin   3 px
//   bottom margin  3 px
// On a 32×64 portrait canvas this gives a 26×42 drawing region (x: 3..28,
// y: 19..60). Bars: 5 px wide, 2 px gap → 4·5 + 3·2 = 26 ✓. Heights
// quantized in 8 levels for a "musical" feel rather than continuous noise.
// Bench-tuned 2026-04-26 (Story 020). Communicates "this is unpredictable"
// much better than a static screen could.
static void drawRandomOrderScreen() {
    if (!ui.ready()) return;
    ui.clear();
    ui.drawScreen(screens::order_random, 0, 0);

    const int X_LEFT   = 3;
    const int Y_TOP    = 19;
    const int Y_BOTTOM = 60;        // 64 - 3 - 1 (last lit row)
    const int H_AVAIL  = Y_BOTTOM - Y_TOP + 1;  // 42
    const int N_BARS   = 4;
    const int BAR_W    = 5;
    const int GAP      = 2;
    const int LEVELS   = 8;
    for (int i = 0; i < N_BARS; ++i) {
        int level = (std::rand() % LEVELS) + 1;     // 1..LEVELS
        int h = (level * H_AVAIL) / LEVELS;
        int x = X_LEFT + i * (BAR_W + GAP);
        ui.raw().fillRect(x, Y_BOTTOM - h + 1, BAR_W, h, SSD1306_WHITE);
    }
    ui.show();
}

static void renderMenu() {
    if ((int32_t)(millis() - resetFlashUntilMs) < 0) {
        ui.showLabel("RESET");
        ui.show();
        return;
    }

    // ORDER param: graphic rendering. Static bitmap for Up/Down/Skip and the
    // two palindromic variants; procedural random-bars frame for Random,
    // updated on every step (see fireStep).
    if (active == Param::Order) {
        if (order == ArpOrder::Random) {
            drawRandomOrderScreen();
            return;
        }
        const screens::Screen* s = orderScreen(order);
        if (s) {
            ui.clear();
            ui.drawScreen(*s, 0, 0);
            ui.show();
            return;
        }
    }

    // KEY param: graphic rendering. When KEY is active and we have art for
    // the current effective key, draw the natural-note bitmap full-screen,
    // optionally overlaid with the sharp glyph. Loses the INT/EXT badge
    // while looking at KEY — acceptable; switch params to re-check clock state.
    if (active == Param::Key) {
        int eff = ((int)keyPC + currentCvTranspose);
        eff = ((eff % 12) + 12) % 12;
        const screens::Screen* natural = keyNaturalScreen((uint8_t)eff);
        if (natural) {
            ui.clear();
            ui.drawScreen(*natural, 0, 0);
            if (pcIsSharp((uint8_t)eff)) {
                // sharp.png is 8×8. Top-left at (24, 18) → top-right at
                // (31, 18), flush with the right edge of the 32-wide
                // portrait panel, ~quarter of the way down. Bench-tuned
                // 2026-04-26.
                ui.drawScreen(screens::sharp, 24, 18);
            }
            ui.show();
            return;
        }
    }

    ui.clear();
    ui.raw().setCursor(0, 0);
    ui.raw().setTextSize(1);
    ui.raw().print(paramName(active));
    {
        // Top-right tag: EXT when external clock active, INT otherwise.
        int16_t w = ui.raw().width();
        ui.raw().setCursor(w - 18, 0);
        ui.raw().print(externalClockActive() ? "EXT" : "INT");
    }
    ui.raw().println();
    ui.raw().setTextSize(2);
    ui.raw().println(activeValueName());
    ui.show();
}

// ------------------------------ setup ------------------------------
//
// Bench-fitted calibration constants (2026-04-30) for the breadboard prototype.
// These are approximate (E12 resistors + pot+TL072 VREF stand-in + cascaded
// op-amp offsets — musical-grade but not lab-grade). Story 022 will replace
// them with proper EEPROM-stored fits once Rev 0.1 PCB is in hand.
//
// outputs::setCalibration(channel, gain, offset)
//   actual_dac_volts = gain * target_jack_volts + offset
//
//   Channel A (J3 = V/Oct out, R_fb = 44 kΩ):
//     bench transfer: V_jack ≈ 8.79 - 4.4·V_dac
//     inverse:        V_dac  ≈ 1.998 - 0.227·V_jack
//
//   Channel B (J4 = gate out, R_fb = 47 kΩ):
//     bench transfer: V_jack ≈ 9.28 - 4.7·V_dac
//     inverse:        V_dac  ≈ 1.974 - 0.213·V_jack
//
// inputs::setCalibration(channel, gain, offset)
//   jack_volts = gain * adc_volts + offset
//
//   Input ch 0 (J1 = transpose CV in):
//     bench transfer: adc_v ≈ 1.96 - 0.180·V_jack
//     inverse:        V_jack ≈ 10.89 - 5.56·adc_v
static const float CAL_OUT_A_GAIN  = -0.227f;
static const float CAL_OUT_A_OFFSET = +1.998f;
static const float CAL_OUT_B_GAIN  = -0.213f;
static const float CAL_OUT_B_OFFSET = +1.974f;
static const float CAL_IN_0_GAIN   = -5.56f;
static const float CAL_IN_0_OFFSET = +10.89f;

void setup() {
    pinMode(PIN_LED, OUTPUT);
    ledWrite(false);

    Wire.begin();
    Wire.setClock(400000);

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* wait briefly for USB */ }

    // SPI bus shared between DAC8552 and MCP3208. Both Rob Tillaart libs
    // call SPI.begin() internally on first transaction; explicit call here
    // makes the clock pin active early so a scope sees something on power-up.
    SPI.begin();

    // outputs:: HAL — DAC8552 dual SPI DAC. setVRef + per-channel calibration
    // must come AFTER begin() so they apply to the right object instance.
    if (!outputs::begin(PIN_CS_DAC)) Serial.println("outputs::begin failed!");
    outputs::setVRef(BENCH_VREF_V);
    outputs::setCalibration(0, CAL_OUT_A_GAIN, CAL_OUT_A_OFFSET);
    outputs::setCalibration(1, CAL_OUT_B_GAIN, CAL_OUT_B_OFFSET);
    gateWrite(false);                       // park gate at jack J4 = 0 V

    // inputs:: HAL — MCP3208 12-bit 8-channel SPI ADC.
    if (!inputs::begin(PIN_CS_ADC)) Serial.println("inputs::begin failed!");
    inputs::setVRef(BENCH_VREF_V);
    // CH0 = pot wiper (no calibration; raw 0..VREF is what the pot delivers).
    // CH1 = J1 input scaling stage. CH2 = J2 input scaling stage (calibration
    // deferred until ch B scaling is wired).
    inputs::setCalibration(ADC_CH_CV_IN_1, CAL_IN_0_GAIN, CAL_IN_0_OFFSET);

    // Tempo pot moved to MCP3208 CH1 on 2026-05-02 (decisions.md §26) — all
    // analog inputs now share the precision REF3040 reference, and D0 is freed
    // for J1 clock/gate digital edge detection. analogReadResolution() is no
    // longer needed for the pot, but keep it set for any future native-ADC use.
    analogReadResolution(12);

    // Seed std::rand() — used by ArpOrder::Random and the procedural
    // random-bars renderer. Mix millis() with a noisy ADC read for a slightly
    // less predictable seed. Story 020 / decisions.md §24.
    {
        uint32_t seed = millis();
        seed ^= (uint32_t)inputs::readRaw(ADC_CH_POT) << 16;
        std::srand(seed);
    }

    Serial.println();
    Serial.println("=== gio post-pivot firmware (SPI HAL) ===");
    Serial.println("Click=cycle  Rotate=value  Hold=reset");
    Serial.println("J3=V/Oct out, J4=gate out, J1=CV transpose in (J2 deferred)");

    if (!ui.begin()) Serial.println("OLED init failed!");
    // Boot splash: frame 1 holds for 3 s, then 100 ms per subsequent frame.
    // Total run time ≈ 3.0 + 8 × 0.1 = 3.8 s. Blocks setup(); fine here.
    ui.playAnimation(screens::splash_screen_animation, 3000, 100);
    enc.begin(PIN_ENC_A, PIN_ENC_B, PIN_ENC_CLICK);

    pinMode(PIN_NEOPIXEL_POWER, OUTPUT);
    digitalWrite(PIN_NEOPIXEL_POWER, HIGH);
    delay(20);
    pixel.begin();
    pixel.setBrightness(NEOPIXEL_BRIGHTNESS);
    updateNeoPixel();

    // Register intervals as the arp's "notes". nextNote() then returns an
    // interval offset directly. Cleaner than maintaining two parallel state.
    arp.setNotes((const uint8_t*)CHORD_INTERVALS, CHORD_LEN);
    arp.setOrder(order);
    renderMenu();

    fireStep();
}

// ------------------------------ loop -------------------------------
void loop() {
    enc.poll();
    uint32_t now = millis();

    int8_t d = enc.delta();
    if (d != 0) {
        switch (active) {
            case Param::Scale:
                scale = (Scale)wrap((int)scale + (int)d, (uint8_t)Scale::COUNT);
                Serial.printf("scale=%s\n", scaleName(scale));
                break;
            case Param::Order:
                order = (ArpOrder)wrap((int)order + (int)d, (uint8_t)ArpOrder::COUNT);
                arp.setOrder(order);
                Serial.printf("order=%s\n", orderName(order));
                break;
            case Param::Key:
                keyPC = wrap((int)keyPC + (int)d, 12);
                Serial.printf("key=%s\n", rootName(keyPC));
                break;
            case Param::Trigger: {
                // Wrap 1..4 by mapping to 0..3, wrapping, then back.
                int next = wrap((int)ratchet - 1 + (int)d, 4) + 1;
                ratchet = (uint8_t)next;
                recalcSubTiming(stepMs);   // immediate effect
                Serial.printf("ratchet=%u\n", (unsigned)ratchet);
                break;
            }
            default: break;
        }
        renderMenu();
    }

    if (enc.pressed()) {
        active = (Param)wrap((int)active + 1, (uint8_t)Param::COUNT);
        Serial.printf("active=%s\n", paramName(active));
        renderMenu();
        updateNeoPixel();
    }

    if (enc.pressedLong()) {
        arp.reset();
        resetFlashUntilMs = now + 600;
        Serial.println("LONG PRESS — arp reset");
        renderMenu();
    }

    // Gate-off check: each sub-gate stays HIGH for subGateMs.
    if (gateOn && (now - lastSubGateMs) >= subGateMs) {
        gateWrite(false);
        ledWrite(false);
        gateOn = false;
    }

    // Sub-gate fire (ratchet > 1): if more sub-gates remain in this note
    // and we're past one subStepMs from the last sub-gate's start, fire.
    if (!gateOn && subGatesFired < ratchet
        && (now - lastSubGateMs) >= subStepMs) {
        fireSubGate();
    }

    // ---- clock source selection: external if recent edges, else internal ----
    bool extEdge = pollExternalClockEdge();
    bool extNow  = externalClockActive();

    // Detect mode transition (for OLED refresh + clean fallback to internal)
    if (extNow != wasExtModeLast) {
        wasExtModeLast = extNow;
        if (!extNow) {
            // Just timed out → internal: re-anchor timing from pot
            refreshTempoFromPot();
            lastStepMs    = millis();
            extStepsRemaining = 0;
            extInterpolatedInterval = 0;
        }
        renderMenu(); // toggles the INT/EXT badge
        Serial.printf("clock=%s\n", extNow ? "EXTERNAL" : "INTERNAL");
    }

    if (extNow) {
        // External: each pulse fires a step + N-1 interpolated steps before
        // the next pulse. The interpolated interval is computed once per
        // pulse from the previously-measured period (so the very first pulse
        // doesn't interpolate — extPrevEdgeMs is 0).
        if (extEdge) {
            extMultiplier = externalMultiplierFromPot();
            if (extPrevEdgeMs > 0 && extMultiplier > 0) {
                uint32_t period = extLastEdgeMs - extPrevEdgeMs;
                extInterpolatedInterval = period / extMultiplier;
            }
            extStepsRemaining = (extMultiplier > 0) ? (uint8_t)(extMultiplier - 1) : 0;
            fireStep();
        }
        // Fire any pending interpolated steps on schedule.
        if (extStepsRemaining > 0 && extInterpolatedInterval > 0
            && (now - lastStepMs) >= extInterpolatedInterval) {
            fireStep();
            extStepsRemaining--;
        }
    } else {
        // Internal: existing time-based step advance.
        if ((now - lastStepMs) >= stepMs) {
            fireStep();
            if (resetFlashUntilMs && (int32_t)(now - resetFlashUntilMs) >= 0) {
                resetFlashUntilMs = 0;
                renderMenu();
            }
        }
    }
}
