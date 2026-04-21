#include <Arduino.h>

// Story 002: blinky — confirms RP2350 toolchain and upload work end to end.
// Replace with arp integration in Story 005.

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_BUILTIN, LOW);   // active LOW on XIAO RP2350
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
}
