#include <Arduino.h>
#include "tempo.h"

// clock-mod2 — minimal square-wave clock source for the HAGIWO MOD2 board.
// Drives GP1 (which feeds the MOD2's 0→10 V output op-amp stage) at a rate
// set by POT1 (A0). The on-panel LED on GP5 blinks on each rising edge.
//
// Toolchain, board definition, and tempo library are shared with gio.
//
// Bench note: MOD2 needs Eurorack ±12 V to actually emit a signal on the
// output jack (the op-amp runs off ±12 V). When powered from USB alone,
// the LED still blinks at the correct rate — useful for verifying before
// reinstalling into a rack.

// ------------------------------ pins -------------------------------
static const uint8_t PIN_POT_RATE  = A0;   // POT1 — rate control
static const uint8_t PIN_CLOCK_OUT = 1;    // GP1 → MOD2 output stage
static const uint8_t PIN_MOD2_LED       = 5;    // GP5 → D9 on the panel (active HIGH)

// ----------------------------- constants ---------------------------
static const float    DUTY        = 0.5f;      // 50 % gate high
static const uint16_t ADC_MAX     = 4095;
static const uint8_t  SUBDIV      = 1;         // quarter notes: 1 pulse = 1 beat

// ------------------------------ state ------------------------------
static uint32_t periodMs   = 500;
static uint32_t highMs     = 250;
static uint32_t lastTickMs = 0;
static bool     highPhase  = false;

// Sample POT1 and convert to period / high-phase durations. Called at each
// rising edge so the rate updates between ticks (never mid-pulse — no jitter).
static void refreshRateFromPot() {
    uint16_t raw = analogRead(PIN_POT_RATE);
    if (raw > ADC_MAX) raw = ADC_MAX;
    float pot = (float)raw / (float)ADC_MAX;
    float bpm = tempo::potToBpm(pot);              // exponential 20..300 BPM
    periodMs = tempo::bpmToStepMs(bpm, SUBDIV);
    highMs   = (uint32_t)((float)periodMs * DUTY + 0.5f);
}

// ------------------------------- setup -----------------------------
void setup() {
    analogReadResolution(12);
    pinMode(PIN_CLOCK_OUT, OUTPUT);
    pinMode(PIN_MOD2_LED,       OUTPUT);
    digitalWrite(PIN_CLOCK_OUT, LOW);
    digitalWrite(PIN_MOD2_LED,       LOW);

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* brief wait for USB CDC */ }

    Serial.println();
    Serial.println("=== clock-mod2 (HAGIWO MOD2 clock source) ===");
    Serial.println("POT1: rate   OUT: GP1   LED: GP5");

    refreshRateFromPot();
    lastTickMs = millis();
}

// ------------------------------- loop ------------------------------
void loop() {
    uint32_t now     = millis();
    uint32_t elapsed = now - lastTickMs;

    // End-of-high-phase: bring OUT low halfway through the period.
    if (highPhase && elapsed >= highMs) {
        digitalWrite(PIN_CLOCK_OUT, LOW);
        digitalWrite(PIN_MOD2_LED,       LOW);
        highPhase = false;
    }

    // Start-of-period: rising edge. Re-sample pot here so rate changes take
    // effect at the next tick boundary instead of mid-pulse.
    if (elapsed >= periodMs) {
        digitalWrite(PIN_CLOCK_OUT, HIGH);
        digitalWrite(PIN_MOD2_LED,       HIGH);
        highPhase  = true;
        lastTickMs = now;
        refreshRateFromPot();
    }
}
