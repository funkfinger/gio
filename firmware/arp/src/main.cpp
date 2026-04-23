#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "arp.h"
#include "tempo.h"
#include "scales.h"
#include "cv.h"
#include "oled_ui.h"
#include "encoder_input.h"

// Story 010: external clock input on J1, auto-detect mode.
// Story 009: CV input for V/oct transpose (α-summing).
//
// When a clock signal is detected on J1 (rising edges within the last 2 s),
// the arp advances on each edge instead of running on the internal tempo.
// When external clock stops, the module falls back to the tempo pot.
// Tempo pot acts as a clock divider when external is active (÷1, ÷2, ÷4).

// ------------------------------ pins -------------------------------
static const uint8_t  PIN_DAC_PWM    = D2;   // GP28
static const uint8_t  PIN_GATE       = D6;   // GP0
static const uint8_t  PIN_TEMPO      = A0;   // D0/GP26
static const uint8_t  PIN_CV         = A1;   // D1/GP27 — buffered CV in (op-amp B)
static const uint8_t  PIN_J1         = D3;   // GP5 — clock in (digital). Spec called for
                                             // GP29/A3 but the XIAO RP2350 doesn't expose
                                             // GP29. ADC-capable pins are all taken (A0/A1/A2),
                                             // so J1 is digital-only for now; pitch mode on
                                             // J1 deferred until an ADC pin is freed.
static const uint8_t  PIN_ENC_A      = D8;   // GP2
static const uint8_t  PIN_ENC_B      = D9;   // GP3
static const uint8_t  PIN_ENC_CLICK  = D10;  // GP4
// PIN_LED — GPIO25, active LOW.

// Onboard RGB NeoPixel:
static const uint8_t PIN_NEOPIXEL_DATA   = 22;
static const uint8_t PIN_NEOPIXEL_POWER  = 23;
static const uint8_t NEOPIXEL_BRIGHTNESS = 64;

// ------------------------------ DAC --------------------------------
static const uint32_t PWM_FREQ_HZ = 36621;
static const uint16_t PWM_MAX     = 4095;
static const float    VREF        = 3.3f;
static const float    GAIN        = 1.26f;     // calibration.md, 2026-04-22
static const uint16_t ADC_MAX     = 4095;

// ------------------------------ CV ---------------------------------
// Voltage divider on the CV input: 100 kΩ series + (47 kΩ + 22 kΩ) to GND.
// Maps 0..8 V at jack → 0..3.27 V at the buffered ADC.
static const float CV_DIVIDER_RATIO = 69.0f / 169.0f; // = 0.4083
static const uint8_t MIDI_NOTE_MIN = 24;   // C1 — clamp lower bound for played notes
static const uint8_t MIDI_NOTE_MAX = 96;   // C7 — clamp upper bound (matches DAC range)

// ------------------------------ Clock-in ---------------------------
// J1 is wired through a 100 kΩ + 100 kΩ divider (1:2 ratio) to GP5.
// Eurorack 5..10 V clock pulses → 2.5..5 V at the GPIO. Above ~3.3 V the
// RP2350's internal clamp + 100 kΩ series limit current safely. The GPIO
// detects HIGH above ~1.6 V which corresponds to ~3.2 V at the jack — well
// inside any standard Eurorack gate level (≥4 V).
static const uint32_t EXT_CLOCK_TIMEOUT_MS = 2000;   // no edges this long → internal

// ----------------------------- music -------------------------------
// Chord stored as semitone INTERVALS from the active root. Root is added
// at play-time. Default chord shape: major triad + octave (= power chord-y).
static const int8_t CHORD_INTERVALS[] = {0, 4, 7, 12};
static const uint8_t CHORD_LEN        = 4;
static const uint8_t SUBDIV           = 4;       // 16th notes
static const float   GATE_FRAC        = 0.5f;

// Pitch class names for the ROOT menu.
static const char* PITCH_CLASS_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// MIDI note number for ROOT pitch-class index 0 (= C). Encoder ROOT pins
// the chord root inside MIDI octave 3 (C3=48 .. B3=59).
static const uint8_t ROOT_OCTAVE_BASE_MIDI = 48;

// ----------------------------- menu --------------------------------
enum class Param : uint8_t { Scale = 0, Order, Root, COUNT };

static const char* paramName(Param p) {
    switch (p) {
        case Param::Scale: return "SCALE";
        case Param::Order: return "ORDER";
        case Param::Root:  return "ROOT";
        default:           return "?";
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
        case ArpOrder::Up:     return "Up";
        case ArpOrder::Down:   return "Down";
        case ArpOrder::UpDown: return "Up-Down";
        case ArpOrder::Skip:   return "Skip";
        default:               return "?";
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
static uint8_t       rootPC      = 0;             // 0=C, 1=C#, ... 11=B
static Param         active      = Param::Scale;

static OledUI        ui;
static EncoderInput  enc;
static Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL_DATA, NEO_GRBW + NEO_KHZ800);

static uint32_t lastStepMs       = 0;
static uint32_t stepMs           = 125;
static uint32_t gateMs           = 62;
static bool     gateOn           = false;
static uint32_t resetFlashUntilMs = 0;
static int      currentCvTranspose = 0;          // updated each step; used by display

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
static uint16_t voltsToPwmCount(float v) {
    float vp = v / GAIN;
    if (vp < 0.0f) vp = 0.0f;
    if (vp > VREF) vp = VREF;
    return (uint16_t)((vp / VREF) * (float)PWM_MAX + 0.5f);
}

// --------------------------- HAL helpers ---------------------------
static const bool GATE_INVERTED = true;
static inline void gateWrite(bool on) {
    digitalWrite(PIN_GATE, (on ^ GATE_INVERTED) ? HIGH : LOW);
}
static inline void ledWrite(bool on) {
    digitalWrite(PIN_LED, on ? LOW : HIGH);
}

// --------------------------- CV reader -----------------------------
// Read the CV jack node voltage by reading the ADC on the buffered side and
// reversing the divider. Returns volts at the jack node (~0..8 V).
static float readCvVolts() {
    uint16_t raw = analogRead(PIN_CV);
    if (raw > ADC_MAX) raw = ADC_MAX;
    float adc_volts = (float)raw / (float)ADC_MAX * VREF;
    return adc_volts / CV_DIVIDER_RATIO;
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
static void refreshTempoFromPot() {
    uint16_t raw = analogRead(PIN_TEMPO);
    if (raw > ADC_MAX) raw = ADC_MAX;
    float pot = (float)raw / (float)ADC_MAX;
    float bpm = tempo::potToBpm(pot);
    stepMs = tempo::bpmToStepMs(bpm, SUBDIV);
    gateMs = (uint32_t)((float)stepMs * GATE_FRAC + 0.5f);
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
    uint16_t raw = analogRead(PIN_TEMPO);
    if (raw > ADC_MAX) raw = ADC_MAX;
    if (held == 1 && raw > LOW_BOUND  + HYST) held = 2;
    if (held == 2 && raw < LOW_BOUND  - HYST) held = 1;
    if (held == 2 && raw > HIGH_BOUND + HYST) held = 4;
    if (held == 4 && raw < HIGH_BOUND - HYST) held = 2;
    return held;
}

// Edge detector on PIN_J1 (digitalRead). Returns true on a low→high transition.
// Debounced: rejects edges within MIN_EDGE_INTERVAL_MS of the previous accepted
// one. 10 ms = up to 6000 BPM rejected as bounce, well above musical territory.
static bool pollExternalClockEdge() {
    static const uint32_t MIN_EDGE_INTERVAL_MS = 10;
    bool now = (digitalRead(PIN_J1) == HIGH);
    if (extClockHigh) {
        if (!now) extClockHigh = false;
        return false;
    }
    if (now) {
        uint32_t now_ms = millis();
        if (extLastEdgeMs != 0 && (now_ms - extLastEdgeMs) < MIN_EDGE_INTERVAL_MS) {
            extClockHigh = true;     // accept the new state, but don't fire
            return false;
        }
        extClockHigh   = true;
        extPrevEdgeMs  = extLastEdgeMs;
        extLastEdgeMs  = now_ms;
        return true;
    }
    return false;
}

static bool externalClockActive() {
    if (extLastEdgeMs == 0) return false;
    return (millis() - extLastEdgeMs) < EXT_CLOCK_TIMEOUT_MS;
}

static void renderMenu();   // fwd decl: defined below fireStep()

static void fireStep() {
    if (externalClockActive()) {
        // External mode: gate length = half of the interpolated step interval.
        // The interpolated interval has already been computed from the last
        // measured period in the loop's edge handler; we just use it here.
        if (extInterpolatedInterval > 0) {
            gateMs = extInterpolatedInterval / 2;
            // sanity-clamp so a stale interval can't strand the gate
            if (gateMs < 5)    gateMs = 5;
            if (gateMs > 5000) gateMs = 50;
        }
    } else {
        refreshTempoFromPot();
    }

    // Sample CV (with hysteresis) and detect transpose change for display refresh.
    int newCvTranspose = readCvTransposeWithHysteresis();
    bool cvChanged     = (newCvTranspose != currentCvTranspose);
    currentCvTranspose = newCvTranspose;

    // root_midi = encoder root + CV transpose (semitone-snap + scale-snap baked in).
    int  encRootMidi = (int)ROOT_OCTAVE_BASE_MIDI + (int)rootPC;
    int  rootMidi    = encRootMidi + currentCvTranspose;

    // arp.nextNote() returns one of CHORD_INTERVALS — we registered intervals
    // as the arp's "notes" so we get an interval offset directly.
    int  intervalIdx = arp.nextNote();
    int  played      = rootMidi + intervalIdx;
    played           = (int)quantize((uint8_t)clampi(played, 0, 127), scale);
    played           = clampi(played, MIDI_NOTE_MIN, MIDI_NOTE_MAX);

    float    volts = midiToVolts((uint8_t)played);
    uint16_t cnt   = voltsToPwmCount(volts);

    analogWrite(PIN_DAC_PWM, cnt);
    gateWrite(true);
    ledWrite(true);
    gateOn     = true;
    lastStepMs = millis();

    // Live ROOT display: re-render menu if the effective root changed and
    // ROOT is the active param. (Only ROOT depends on CV — Scale/Order don't.)
    if (cvChanged && active == Param::Root) renderMenu();
}

// --------------------------- UI helpers ----------------------------
static void updateNeoPixel() {
    uint8_t r = 0, g = 0, b = 0;
    if      (active == Param::Scale) g = 255;
    else if (active == Param::Order) b = 255;
    else if (active == Param::Root)  { r = 255; b = 255; } // magenta
    pixel.setPixelColor(0, pixel.Color(r, g, b));
    pixel.show();
}

static const char* activeValueName() {
    static char buf[8];
    switch (active) {
        case Param::Scale: return scaleName(scale);
        case Param::Order: return orderName(order);
        case Param::Root: {
            // Show the EFFECTIVE root: encoder rootPC + CV transpose, mod 12.
            // Trailing '*' indicates CV is contributing (so encoder rotation
            // alone doesn't fully account for what's playing).
            int eff = ((int)rootPC + currentCvTranspose);
            eff = ((eff % 12) + 12) % 12;
            const char* name = rootName((uint8_t)eff);
            if (currentCvTranspose != 0) {
                snprintf(buf, sizeof(buf), "%s*", name);
                return buf;
            }
            return name;
        }
        default: return "?";
    }
}

static void renderMenu() {
    if ((int32_t)(millis() - resetFlashUntilMs) < 0) {
        ui.showLabel("RESET");
        ui.show();
        return;
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
void setup() {
    analogWriteResolution(12);
    analogReadResolution(12);
    analogWriteFreq(PWM_FREQ_HZ);
    pinMode(PIN_DAC_PWM, OUTPUT);
    pinMode(PIN_GATE,    OUTPUT);
    pinMode(PIN_LED,     OUTPUT);
    gateWrite(false);
    ledWrite(false);

    Wire.begin();
    Wire.setClock(400000);

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* wait briefly for USB */ }

    Serial.println();
    Serial.println("=== gio Story 010 — clock-in on J1 ===");
    Serial.println("Click=cycle  Rotate=value  Hold=reset");
    Serial.println("J1 clock auto-detected; tempo pot becomes div (1/2/4) when ext");

    // Internal pulldown is enabled belt-and-braces, but on RP2350 it cannot be
    // relied on alone (silicon errata RP2350-E9). The bench fix is a low-impedance
    // external divider (10 kΩ + 10 kΩ at J1, ≤ 8.2 kΩ effective pulldown).
    // See decisions.md §18.
    pinMode(PIN_J1, INPUT_PULLDOWN);
    if (!ui.begin()) Serial.println("OLED init failed!");
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
            case Param::Root:
                rootPC = wrap((int)rootPC + (int)d, 12);
                Serial.printf("root=%s\n", rootName(rootPC));
                break;
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

    if (gateOn && (now - lastStepMs) >= gateMs) {
        gateWrite(false);
        ledWrite(false);
        gateOn = false;
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
