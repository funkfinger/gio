#include <Arduino.h>
#include "arp.h"
#include "tempo.h"

// Story 005: v0.1.0 — first arp. Plays a 4-note up-arpeggio (C3, E3, G3, C4)
// at a fixed 120 BPM through PWM DAC + op-amp on D2 and gate on D6.

// ------------------------------ pins -------------------------------
static const uint8_t  PIN_DAC_PWM = D2;        // GP28 — 12-bit PWM V/Oct
static const uint8_t  PIN_GATE    = D6;        // GP0  — gate via NPN inverter (active low at pin → high at jack)
// PIN_LED is provided by the variant header (GPIO25, active low — see Story 002)

// ------------------------------ DAC --------------------------------
static const uint32_t PWM_FREQ_HZ = 36621;     // ~150 MHz / 4096
static const uint16_t PWM_MAX     = 4095;      // 12-bit
static const float    VREF        = 3.3f;
static const float    GAIN        = 1.26f;     // bench-calibrated 2026-04-22; see calibration.md

// ----------------------------- music -------------------------------
// MIDI notes for the 4-note up arpeggio: C major triad rooted at C3 + octave.
// C3=48, E3=52, G3=55, C4=60.
static const uint8_t CHORD[]      = {48, 52, 55, 60};
static const uint8_t CHORD_LEN    = 4;

// 120 BPM quarter notes → 500 ms/step. 50 % gate duty.
static const uint32_t STEP_MS     = 500;
static const uint32_t GATE_MS     = STEP_MS / 2;

// --------------------------- voltage ↔ note ------------------------
// V/Oct convention: C3 (MIDI 48) = 0.000 V, +1 V per octave.
static float midiToVolts(uint8_t midi) {
    return ((float)midi - 48.0f) / 12.0f;
}

static uint16_t voltsToPwmCount(float volts_out) {
    float v_pwm = volts_out / GAIN;
    if (v_pwm < 0.0f) v_pwm = 0.0f;
    if (v_pwm > VREF) v_pwm = VREF;
    return (uint16_t)((v_pwm / VREF) * (float)PWM_MAX + 0.5f);
}

// --------------------------- gate driver ---------------------------
// NPN common-emitter inverter at J4: digitalWrite(PIN_GATE, HIGH) saturates the
// transistor → jack pulled LOW. digitalWrite(LOW) → jack pulled HIGH (5V).
static const bool GATE_INVERTED = true;

static inline void gateWrite(bool on) {
    digitalWrite(PIN_GATE, (on ^ GATE_INVERTED) ? HIGH : LOW);
}

static inline void ledWrite(bool on) {
    // PIN_LED is active-low on the XIAO RP2350 (LOW = lit).
    digitalWrite(PIN_LED, on ? LOW : HIGH);
}

// ------------------------------ state ------------------------------
static Arp      arp;
static uint32_t lastStepMs = 0;
static bool     gateOn     = false;

static void fireStep() {
    uint8_t  midi  = arp.nextNote();
    float    volts = midiToVolts(midi);
    uint16_t cnt   = voltsToPwmCount(volts);

    analogWrite(PIN_DAC_PWM, cnt);
    gateWrite(true);
    ledWrite(true);
    gateOn     = true;
    lastStepMs = millis();

    Serial.printf("step=%u  midi=%u  volts=%.3f  count=%u\n",
                  arp.step() - 1, midi, volts, cnt);
}

// ------------------------------- setup -----------------------------
void setup() {
    analogWriteResolution(12);
    analogWriteFreq(PWM_FREQ_HZ);
    pinMode(PIN_DAC_PWM, OUTPUT);
    pinMode(PIN_GATE,    OUTPUT);
    pinMode(PIN_LED,     OUTPUT);
    gateWrite(false);
    ledWrite(false);

    arp.setNotes(CHORD, CHORD_LEN);
    arp.setOrder(ArpOrder::Up);

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* wait briefly for USB */ }

    Serial.println();
    Serial.println("=== gio v0.1 — arp plays up pattern (Story 005) ===");
    Serial.println("Notes: C3 E3 G3 C4   Order: Up   Tempo: 120 BPM (500 ms/step, 50% gate)");

    fireStep(); // first note immediately
}

// ------------------------------- loop ------------------------------
void loop() {
    uint32_t now = millis();

    // Gate-off after 50 % of step
    if (gateOn && (now - lastStepMs) >= GATE_MS) {
        gateWrite(false);
        ledWrite(false);
        gateOn = false;
    }

    // Advance to next step at the BPM clock
    if ((now - lastStepMs) >= STEP_MS) {
        fireStep();
    }
}
