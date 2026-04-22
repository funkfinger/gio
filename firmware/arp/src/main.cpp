#include <Arduino.h>
#include "arp.h"
#include "tempo.h"

// Story 006: tempo pot. Same arp as v0.1 but step period is now derived live
// from the tempo pot on A0 (D0). 16th-note subdivision: 125 ms/step at 120 BPM.

// ------------------------------ pins -------------------------------
static const uint8_t  PIN_DAC_PWM = D2;        // GP28 — 12-bit PWM V/Oct
static const uint8_t  PIN_GATE    = D6;        // GP0  — gate via NPN inverter
static const uint8_t  PIN_TEMPO   = A0;        // D0/GP26 — tempo pot wiper
// PIN_LED is provided by the variant (GPIO25, active low)

// ------------------------------ DAC --------------------------------
static const uint32_t PWM_FREQ_HZ = 36621;
static const uint16_t PWM_MAX     = 4095;
static const float    VREF        = 3.3f;
static const float    GAIN        = 1.26f;     // bench-calibrated; calibration.md

// ------------------------------ ADC --------------------------------
static const uint16_t ADC_MAX     = 4095;      // 12-bit

// ----------------------------- music -------------------------------
static const uint8_t CHORD[]      = {48, 52, 55, 60}; // C3 E3 G3 C4
static const uint8_t CHORD_LEN    = 4;
static const uint8_t SUBDIV       = 4;         // 16th notes (4 per quarter)
static const float   GATE_FRAC    = 0.5f;      // 50 % gate duty

// --------------------------- voltage ↔ note ------------------------
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
static const bool GATE_INVERTED = true; // BC548 NPN inverter on D6 → J4

static inline void gateWrite(bool on) {
    digitalWrite(PIN_GATE, (on ^ GATE_INVERTED) ? HIGH : LOW);
}

static inline void ledWrite(bool on) {
    digitalWrite(PIN_LED, on ? LOW : HIGH); // active-low onboard LED
}

// ------------------------------ state ------------------------------
static Arp      arp;
static uint32_t lastStepMs = 0;
static uint32_t stepMs     = 125;  // refreshed at every step boundary
static uint32_t gateMs     = 62;   // = stepMs * GATE_FRAC, refreshed alongside
static bool     gateOn     = false;

// Read tempo pot (12-bit) → BPM (exponential 40..300) → step period in ms.
// Recomputed once per step so the BPM in flight stays stable for the duration
// of any one step; the pot reading is sampled at the moment of step advance.
static void refreshTempoFromPot() {
    uint16_t raw = analogRead(PIN_TEMPO);
    if (raw > ADC_MAX) raw = ADC_MAX;
    float pot = (float)raw / (float)ADC_MAX;
    float bpm = tempo::potToBpm(pot);
    stepMs = tempo::bpmToStepMs(bpm, SUBDIV);
    gateMs = (uint32_t)((float)stepMs * GATE_FRAC + 0.5f);
}

static void fireStep() {
    refreshTempoFromPot();

    uint8_t  midi  = arp.nextNote();
    float    volts = midiToVolts(midi);
    uint16_t cnt   = voltsToPwmCount(volts);

    analogWrite(PIN_DAC_PWM, cnt);
    gateWrite(true);
    ledWrite(true);
    gateOn     = true;
    lastStepMs = millis();
}

// ------------------------------- setup -----------------------------
void setup() {
    analogWriteResolution(12);
    analogReadResolution(12);
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
    Serial.println("=== gio Story 006 — tempo pot live ===");
    Serial.println("Notes: C3 E3 G3 C4   Order: Up   Subdiv: 16ths   BPM: 40..300 (exp)");

    fireStep();
}

// ------------------------------- loop ------------------------------
void loop() {
    uint32_t now = millis();

    if (gateOn && (now - lastStepMs) >= gateMs) {
        gateWrite(false);
        ledWrite(false);
        gateOn = false;
    }

    if ((now - lastStepMs) >= stepMs) {
        fireStep();
    }
}
