#include <Arduino.h>
#include "oled_ui.h"
#include "encoder_input.h"

// Story 007: OLED + encoder bring-up. Standalone — no arp engine.
// Encoder rotation increments/decrements a counter shown live on the OLED.
// Encoder click prints "CLICK" and flashes the onboard LED.

// ------------------------------ pins -------------------------------
// Encoder pins per docs/generative-arp-module.md §2.4:
static const uint8_t PIN_ENC_A     = D8;   // GP2
static const uint8_t PIN_ENC_B     = D9;   // GP3
static const uint8_t PIN_ENC_CLICK = D10;  // GP4
// I2C SDA/SCL are D4/D5 (GP6/GP7) — handled by Wire.

// ------------------------------ state ------------------------------
static OledUI       ui;
static EncoderInput enc;
static int32_t      counter = 0;

// LED helpers (active LOW on XIAO RP2350)
static inline void ledWrite(bool on) {
    digitalWrite(PIN_LED, on ? LOW : HIGH);
}

static uint32_t ledOffAtMs = 0;
static bool     ledOn      = false;
static void blinkLed(uint32_t ms = 80) {
    ledWrite(true);
    ledOn      = true;
    ledOffAtMs = millis() + ms;
}

// ------------------------------- setup -----------------------------
void setup() {
    pinMode(PIN_LED, OUTPUT);
    ledWrite(false);

    Wire.begin();
    Wire.setClock(400000); // 400 kHz fast mode (decisions.md §4 / §12)

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* wait briefly for USB */ }

    Serial.println();
    Serial.println("=== gio Story 007 — OLED + encoder bring-up ===");

    if (!ui.begin()) {
        Serial.println("OLED init failed (no ACK at 0x3C). Check SDA/SCL/3V3/GND.");
    } else {
        ui.showLabel("HELLO");
        ui.show();
        Serial.println("OLED OK");
    }

    enc.begin(PIN_ENC_A, PIN_ENC_B, PIN_ENC_CLICK);

    // Show the initial counter after the splash so the screen is meaningful.
    delay(750);
    ui.showParameter("counter", counter);
    ui.show();
}

// ------------------------------- loop ------------------------------
void loop() {
    enc.poll();

    int8_t d = enc.delta();
    if (d != 0) {
        counter += d;
        ui.showParameter("counter", counter);
        ui.show();
        Serial.printf("delta=%d  counter=%ld\n", (int)d, (long)counter);
    }

    if (enc.pressed()) {
        Serial.println("CLICK");
        blinkLed(120);
    }

    if (ledOn && (int32_t)(millis() - ledOffAtMs) >= 0) {
        ledWrite(false);
        ledOn = false;
    }
}
