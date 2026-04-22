#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "arp.h"
#include "tempo.h"
#include "scales.h"
#include "oled_ui.h"
#include "encoder_input.h"

// Story 008: encoder menu — full integration.
// Live arp on V/Oct + gate, live tempo on the pot, scale + arp order
// selectable via the encoder. Click cycles which parameter is active;
// rotation changes the active param's value. Long-press resets the arp.

// ------------------------------ pins -------------------------------
static const uint8_t  PIN_DAC_PWM    = D2;   // GP28
static const uint8_t  PIN_GATE       = D6;   // GP0
static const uint8_t  PIN_TEMPO      = A0;   // D0/GP26
static const uint8_t  PIN_ENC_A      = D8;   // GP2
static const uint8_t  PIN_ENC_B      = D9;   // GP3
static const uint8_t  PIN_ENC_CLICK  = D10;  // GP4
// PIN_LED — GPIO25, active LOW.
// Onboard RGB NeoPixel (per Seeed XIAO RP2350 wiki — not exposed by the
// arduino-pico variant header, so we hardcode the pins):
//   - GPIO22 = WS2812 data line
//   - GPIO23 = POWER ENABLE (must be HIGH for the LED to light)
//   - The LED is RGBW (4 bytes/pixel), so use NEO_GRBW.
static const uint8_t PIN_NEOPIXEL_DATA   = 22;
static const uint8_t PIN_NEOPIXEL_POWER  = 23;
static const uint8_t NEOPIXEL_BRIGHTNESS = 64; // out of 255

// ------------------------------ DAC --------------------------------
static const uint32_t PWM_FREQ_HZ = 36621;
static const uint16_t PWM_MAX     = 4095;
static const float    VREF        = 3.3f;
static const float    GAIN        = 1.26f;     // calibration.md, 2026-04-22
static const uint16_t ADC_MAX     = 4095;

// ----------------------------- music -------------------------------
// Base chord — semitones get re-quantised through the active scale every
// time we play, so e.g. selecting Pent Min snaps E→Eb at the moment of step.
static const uint8_t CHORD_BASE[] = {48, 52, 55, 60}; // C3 E3 G3 C4
static const uint8_t CHORD_LEN    = 4;
static const uint8_t SUBDIV       = 4;       // 16th-note steps
static const float   GATE_FRAC    = 0.5f;

// ----------------------------- menu --------------------------------
enum class Param : uint8_t { Scale = 0, Order, COUNT };

static const char* paramName(Param p) {
    switch (p) {
        case Param::Scale: return "SCALE";
        case Param::Order: return "ORDER";
        default:           return "?";
    }
}

static const char* scaleName(Scale s) {
    switch (s) {
        case Scale::Major:           return "Major";
        case Scale::NaturalMinor:    return "Nat Min";
        case Scale::PentatonicMajor: return "Pent Maj";
        case Scale::PentatonicMinor: return "Pent Min";
        case Scale::Dorian:          return "Dorian";
        case Scale::Chromatic:       return "Chrom";
        default:                     return "?";
    }
}

static const char* orderName(ArpOrder o) {
    switch (o) {
        case ArpOrder::Up:     return "Up";
        case ArpOrder::Down:   return "Down";
        case ArpOrder::UpDown: return "Up-Down";
        case ArpOrder::Skip:   return "Skip";
        default:               return "?";
    }
}

// Wrap-around step within an enum range [0, COUNT).
static inline uint8_t wrap(int v, uint8_t count) {
    int n = (int)count;
    int r = ((v % n) + n) % n;
    return (uint8_t)r;
}

// ----------------------------- state -------------------------------
static Arp           arp;
static Scale         scale       = Scale::Major;
static ArpOrder      order       = ArpOrder::Up;
static Param         active      = Param::Scale;

static OledUI        ui;
static EncoderInput  enc;
static Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL_DATA, NEO_GRBW + NEO_KHZ800);

static uint32_t lastStepMs       = 0;
static uint32_t stepMs           = 125;
static uint32_t gateMs           = 62;
static bool     gateOn           = false;

static uint32_t resetFlashUntilMs = 0; // OLED shows "RESET" until this ms

// ---------------------- voltage ↔ note -----------------------------
static float midiToVolts(uint8_t midi) {
    return ((float)midi - 48.0f) / 12.0f;
}
static uint16_t voltsToPwmCount(float v) {
    float vp = v / GAIN;
    if (vp < 0.0f) vp = 0.0f;
    if (vp > VREF) vp = VREF;
    return (uint16_t)((vp / VREF) * (float)PWM_MAX + 0.5f);
}

// --------------------------- HAL helpers ---------------------------
static const bool GATE_INVERTED = true;
static inline void gateWrite(bool on) {
    digitalWrite(PIN_GATE, (on ^ GATE_INVERTED) ? HIGH : LOW);
}
static inline void ledWrite(bool on) {
    digitalWrite(PIN_LED, on ? LOW : HIGH);
}

// --------------------------- engine glue ---------------------------
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

    uint8_t base    = arp.nextNote();
    uint8_t played  = quantize(base, scale);
    float   volts   = midiToVolts(played);
    uint16_t cnt    = voltsToPwmCount(volts);

    analogWrite(PIN_DAC_PWM, cnt);
    gateWrite(true);
    ledWrite(true);
    gateOn     = true;
    lastStepMs = millis();
}

// Active-param indicator on the onboard RGB LED.
//   Scale = green, Order = blue.  Long-press flash will tint white briefly
//   via renderMenu() being called from the long-press handler.
static void updateNeoPixel() {
    uint8_t r = 0, g = 0, b = 0;
    if      (active == Param::Scale) g = 255;
    else if (active == Param::Order) b = 255;
    pixel.setPixelColor(0, pixel.Color(r, g, b));
    pixel.show();
}

// --------------------------- UI helpers ----------------------------
static void renderMenu() {
    if ((int32_t)(millis() - resetFlashUntilMs) < 0) {
        ui.showLabel("RESET");
        ui.show();
        return;
    }
    const char* val =
        (active == Param::Scale) ? scaleName(scale) : orderName(order);
    ui.showParameter(paramName(active), 0); // builds a 2-line layout
    // Replace the int "0" we just drew with the actual value name.
    ui.clear();
    ui.raw().setCursor(0, 0);
    ui.raw().setTextSize(1);
    ui.raw().println(paramName(active));
    ui.raw().setTextSize(2);
    ui.raw().println(val);
    ui.show();
}

// ------------------------------ setup ------------------------------
void setup() {
    analogWriteResolution(12);
    analogReadResolution(12);
    analogWriteFreq(PWM_FREQ_HZ);
    pinMode(PIN_DAC_PWM, OUTPUT);
    pinMode(PIN_GATE,    OUTPUT);
    pinMode(PIN_LED,     OUTPUT);
    gateWrite(false);
    ledWrite(false);

    Wire.begin();
    Wire.setClock(400000);

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* wait briefly for USB */ }

    Serial.println();
    Serial.println("=== gio Story 008 — encoder menu ===");
    Serial.println("Click=cycle param  Rotate=change value  Hold=reset");

    if (!ui.begin()) Serial.println("OLED init failed!");
    enc.begin(PIN_ENC_A, PIN_ENC_B, PIN_ENC_CLICK);

    pinMode(PIN_NEOPIXEL_POWER, OUTPUT);
    digitalWrite(PIN_NEOPIXEL_POWER, HIGH); // enable LED power rail
    delay(20);                              // let the rail settle before driving WS2812
    pixel.begin();
    pixel.setBrightness(NEOPIXEL_BRIGHTNESS);
    updateNeoPixel();                       // sets colour AND calls show()

    arp.setNotes(CHORD_BASE, CHORD_LEN);
    arp.setOrder(order);
    renderMenu();

    fireStep();
}

// ------------------------------ loop -------------------------------
void loop() {
    enc.poll();
    uint32_t now = millis();

    // ---- input handling -------------------------------------------
    int8_t d = enc.delta();
    if (d != 0) {
        if (active == Param::Scale) {
            scale = (Scale)wrap((int)scale + (int)d, (uint8_t)Scale::COUNT);
            Serial.printf("scale=%s\n", scaleName(scale));
        } else {
            order = (ArpOrder)wrap((int)order + (int)d, (uint8_t)ArpOrder::COUNT);
            arp.setOrder(order);          // resets step to 0
            Serial.printf("order=%s (arp restart)\n", orderName(order));
        }
        renderMenu();
    }

    if (enc.pressed()) {
        active = (Param)wrap((int)active + 1, (uint8_t)Param::COUNT);
        Serial.printf("active=%s\n", paramName(active));
        renderMenu();
        updateNeoPixel();
    }

    if (enc.pressedLong()) {
        arp.reset();
        resetFlashUntilMs = now + 600;
        Serial.println("LONG PRESS — arp reset");
        renderMenu();
    }

    // ---- engine ---------------------------------------------------
    if (gateOn && (now - lastStepMs) >= gateMs) {
        gateWrite(false);
        ledWrite(false);
        gateOn = false;
    }
    if ((now - lastStepMs) >= stepMs) {
        fireStep();
        // Refresh menu when the RESET overlay finishes
        if (resetFlashUntilMs && (int32_t)(now - resetFlashUntilMs) >= 0) {
            resetFlashUntilMs = 0;
            renderMenu();
        }
    }
}
