#include <Arduino.h>

// Story 002: blinky — confirms RP2350 toolchain and upload work end to end.
// Replace with arp integration in Story 005.

void setup() {
    pinMode(PIN_LED, OUTPUT);
}

void loop() {
    digitalWrite(PIN_LED, LOW);   // active LOW on XIAO RP2350
    delay(500);
    digitalWrite(PIN_LED, HIGH);
    delay(500);
}
