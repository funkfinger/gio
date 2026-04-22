#include <Arduino.h>

// Story 004: V/Oct calibration ramp.
// Steps through C3..C7 (MIDI 48..96) on D2 PWM, 2 s per step.
// Multimeter at the op-amp output (after R4=100Ω) should read 0, 1, 2, 3, 4 V.
//
// Replace with arp integration in Story 005.

static const uint8_t  PIN_DAC_PWM = D2;        // GP28
static const uint32_t PWM_FREQ_HZ = 36621;     // 150 MHz / 4096
static const uint16_t PWM_MAX     = 4095;      // 12-bit
static const float    VREF        = 3.3f;      // RP2350 IO rail

// Op-amp non-inverting gain = 1 + R_f / R_g.
// Nominal: 1 + (2.2k + 470) / 10k = 1.267. Bench-measured 2026-04-22: 1.26 (fit
// slope 0.9917 V/oct across C3..C7; residuals within ±2 mV of linear fit).
static const float GAIN = 1.26f;

// V/Oct convention for this module: C3 (MIDI 48) = 0.000 V, +1 V per octave.
// MIDI semitone step = 1/12 V. Voltage is read at the op-amp output node.
static float midiToVolts(uint8_t midi) {
    return ((float)midi - 48.0f) / 12.0f;
}

// Map a *target output voltage* (post op-amp) to a 12-bit PWM count.
// Returns 0..4095, clamped if the request exceeds the headroom (V_pwm <= VREF).
static uint16_t voltsToPwmCount(float volts_out) {
    float v_pwm = volts_out / GAIN;
    if (v_pwm < 0.0f)  v_pwm = 0.0f;
    if (v_pwm > VREF)  v_pwm = VREF;
    float count = (v_pwm / VREF) * (float)PWM_MAX;
    return (uint16_t)(count + 0.5f);
}

// "C3", "C4", ... helper for Serial readability.
static const char* noteName(uint8_t midi) {
    switch (midi) {
        case 48: return "C3";
        case 60: return "C4";
        case 72: return "C5";
        case 84: return "C6";
        case 96: return "C7";
        default: return "??";
    }
}

void setup() {
    analogWriteResolution(12);
    analogWriteFreq(PWM_FREQ_HZ);
    pinMode(PIN_DAC_PWM, OUTPUT);

    Serial.begin(115200);
    // Wait briefly for USB-CDC, but don't hang headless.
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* wait */ }

    Serial.println();
    Serial.println("=== gio V/Oct calibration (Story 004) ===");
    Serial.printf("GAIN = %.3f  (update after measuring)\n", GAIN);
    Serial.printf("VREF = %.3f V   PWM_MAX = %u\n", VREF, PWM_MAX);
    Serial.println("MIDI  Note  Target_V   PWM_count");
}

void loop() {
    static const uint8_t notes[] = {48, 60, 72, 84, 96}; // C3..C7
    for (uint8_t i = 0; i < sizeof(notes); ++i) {
        uint8_t  n     = notes[i];
        float    volts = midiToVolts(n);
        uint16_t count = voltsToPwmCount(volts);

        analogWrite(PIN_DAC_PWM, count);
        Serial.printf("%4u  %-4s  %6.3f V   %4u\n", n, noteName(n), volts, count);
        delay(2000);
    }
}
