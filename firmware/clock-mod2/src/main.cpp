#include <Arduino.h>
#include <math.h>

// clock-mod2 — minimal square-wave clock source for the HAGIWO MOD2 board.
// Drives GP1 (which feeds the MOD2's 0→10 V output op-amp stage) at a rate
// set by POT1 (A0). The on-panel LED on GP5 blinks on each rising edge.
//
// The pot maps EXPONENTIALLY to a pulse rate in Hz (CLOCK_MIN_HZ..CLOCK_MAX_HZ).
// "BPM" isn't the right unit for a raw clock source — at the receiving end the
// arp may treat each pulse as a quarter, an eighth, or a sixteenth note,
// depending on configuration. Here we just pick a pulse rate.
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
static const float    DUTY          = 0.5f;       // 50 % gate high
static const uint16_t ADC_MAX       = 4095;
static const float    CLOCK_MIN_HZ  = 2.0f;       // ≈ 120 quarter-note BPM, slow end
static const float    CLOCK_MAX_HZ  = 20.0f;      // ≈ 1200 quarter-note BPM (or 300 BPM 16ths), fast end

// ------------------------------ state ------------------------------
static uint32_t periodMs   = 500;
static uint32_t highMs     = 250;
static uint32_t lastTickMs = 0;
static bool     highPhase  = false;

// Map normalised pot reading [0, 1] EXPONENTIALLY to a pulse rate in Hz.
// Endpoints exact; ratio per equal pot slice is constant — uniform musical feel.
static float potToHz(float pot) {
    if (pot < 0.0f) pot = 0.0f;
    if (pot > 1.0f) pot = 1.0f;
    return CLOCK_MIN_HZ * powf(CLOCK_MAX_HZ / CLOCK_MIN_HZ, pot);
}

// Sample POT1 and convert to period / high-phase durations. Called at each
// rising edge so the rate updates between ticks (never mid-pulse — no jitter).
static void refreshRateFromPot() {
    uint16_t raw = analogRead(PIN_POT_RATE);
    if (raw > ADC_MAX) raw = ADC_MAX;
    float pot = (float)raw / (float)ADC_MAX;
    float hz  = potToHz(pot);
    periodMs  = (uint32_t)(1000.0f / hz + 0.5f);
    highMs    = (uint32_t)((float)periodMs * DUTY + 0.5f);
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
    Serial.println("POT1: rate (2..20 Hz exp)   OUT: GP1   LED: GP5");

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
