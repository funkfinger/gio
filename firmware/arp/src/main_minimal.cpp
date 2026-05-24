// Minimal "hello" sketch for diagnosing XIAO boot failures.
//
// Selected by env:seeed-xiao-rp2350-minimal in platformio.ini.
// Purpose: rule out whether our smoke-test setup() is the cause of a
// post-flash USB CDC hang. This sketch does NOTHING beyond Serial.begin and
// a periodic Serial.println — no SPI, no Wire, no GPIO setup, no encoders,
// no OLED. If this DOES enumerate and stream "alive" lines, then the smoke
// test setup() has the bug. If it DOESN'T, the issue is lower than our code
// (toolchain, board file, subtle chip damage, etc.).

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* wait briefly for USB */ }
    Serial.println();
    Serial.println(F("=== gio minimal hello (no peripherals) ==="));
}

void loop() {
    Serial.print(F("alive t="));
    Serial.println(millis());
    delay(500);
}
