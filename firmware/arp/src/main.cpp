#include <Arduino.h>

// Story 003: PWM DAC ramp — bench-verify signal path before adding op-amp (Story 004).
// Replace with arp integration in Story 005.

static const uint8_t PIN_DAC_PWM = D2; // GP28

void setup() {
    // Global form only — no per-pin overload exists in arduino-pico; see decisions.md §11.
    analogWriteResolution(12);
    analogWriteFreq(36621); // 150 MHz / 4096 ≈ 36,621 Hz
    pinMode(PIN_DAC_PWM, OUTPUT);
}

void loop() {
    // Sweep 0→4095 at ~10 Hz (24 µs × 4096 steps ≈ 98 ms per ramp).
    for (int i = 0; i <= 4095; ++i) {
        analogWrite(PIN_DAC_PWM, i);
        delayMicroseconds(24);
    }
}
