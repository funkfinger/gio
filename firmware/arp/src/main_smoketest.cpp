// Smoke-test entrypoint for the Story 012 SPI bring-up.
//
// Build via the dedicated PlatformIO env (see platformio.ini):
//     pio run -d firmware/arp -e seeed-xiao-rp2350-smoketest --target upload
//     pio device monitor -d firmware/arp -e seeed-xiao-rp2350-smoketest
//
// The production arp firmware (src/main.cpp) is excluded from this build via
// build_src_filter, so it stays untouched until the SPI HAL is integrated
// in a follow-up.
//
// What this smoke test does:
//   1. Initializes outputs:: (DAC8552) and inputs:: (MCP3208) HALs over SPI0
//   2. DAC channel A: 1 Hz triangle (0 → VREF → 0), 50 samples per cycle
//      → at jack J3 (after the inverting output stage) this appears as a
//        clean ±9 V symmetric triangle, also at 1 Hz
//   3. DAC channel B: 0.5 Hz square wave alternating between 1.024 V and
//      3.072 V on the DAC. Through the inverting output stage that becomes
//      an alternation between roughly +4.5 V and −4.5 V at jack J4. The
//      slower rate + different waveform makes it visually obvious on a
//      2-channel scope that A and B are independent (no crosstalk, no
//      CS-line crossover).
//   4. After each loop iteration, reads MCP3208 channel 0 (intended to
//      track DAC OUTA via jumper) and prints t_ms / dac_a_v / dac_b_v /
//      adc_v / adc_count to serial.
//
// Bench setup: jumper DAC OUTA directly to MCP3208 channel 0 (skipping
// the input scaling stage). Expected: ADC reading tracks DAC OUTA value
// linearly within a few LSBs.
//
// Pin assignments per docs/bench-wiring.md §3 + §4 (SPI-pivot map):
//     SPI: SCK=D8/GP2, MISO=D9/GP4, MOSI=D10/GP3
//     CS_DAC=D3/GP5, CS_ADC=D6/GP0  (both with 10 kΩ pull-up to +3.3 V)

#include <Arduino.h>
#include <SPI.h>

#include "encoder_input.h"
#include "inputs.h"
#include "oled_ui.h"
#include "outputs.h"

static constexpr uint8_t PIN_CS_DAC    = D3;  // GP5 — DAC8552 /SYNC
static constexpr uint8_t PIN_CS_ADC    = D6;  // GP0 — MCP3208 /CS
// Encoder A/B match the schematic convention (A→D1, B→D2). The 2026-05-02
// firmware-swap was reverted on 2026-05-04 after the breadboard wires were
// physically swapped to align with the KiCad schematic.
static constexpr uint8_t PIN_ENC_A     = D1;  // GP27
static constexpr uint8_t PIN_ENC_B     = D2;  // GP28
static constexpr uint8_t PIN_ENC_CLICK = D7;  // GP1

static EncoderInput g_encoder;
static int32_t      g_enc_count = 0;

static OledUI       g_oled;
static uint32_t     g_last_oled_ms = 0;
static const uint32_t OLED_REFRESH_MS = 100;  // ~10 Hz; I2C bandwidth-friendly

// Bench substitution: VREF supplied by the pot+TL072 buffer at ~4.096 V.
// Re-measure with a multimeter and update if needed.
static constexpr float BENCH_VREF_V = 4.096f;

// Channel A — 1 Hz triangle (full DAC range)
static constexpr uint32_t TRIANGLE_PERIOD_MS = 1000;  // full cycle (up + down)
static constexpr uint32_t STEP_MS            = 20;    // ~50 samples per cycle
static constexpr uint8_t  DAC_CHANNEL_A      = 0;
// MCP3208 channel layout (decisions.md §26): CH0 = pot, CH1 = J1, CH2 = J2.
// "Loopback" channel for the OUTA-to-ADC test is now CH1 (= J1's input op-amp
// output). To validate the raw DAC->ADC path without the input stage, jumper
// DAC OUTA directly to MCP3208 pin 2 (CH1), bypassing the op-amp.
static constexpr uint8_t  ADC_CH_POT         = 0;     // CH0 — primary pot wiper
static constexpr uint8_t  ADC_CH_J1          = 1;     // CH1 — J1 input op-amp output
static constexpr uint8_t  ADC_CH_J2          = 2;     // CH2 — J2 input op-amp output

// Channel B — 0.5 Hz square between 1/4 and 3/4 of VREF.
// Through the inverting output stage these DAC voltages map to roughly
// +4.5 V and −4.5 V at jack J4 — a clearly different waveform from
// channel A's triangle when both are scoped together.
static constexpr uint32_t SQUARE_PERIOD_MS = 2000;
static constexpr uint8_t  DAC_CHANNEL_B    = 1;
// DAC voltages computed from VREF in setup() so they track the actual
// bench-dialled VREF rather than a compile-time constant.
static float g_square_low_v  = 0.0f;
static float g_square_high_v = 0.0f;

void setup() {
    Serial.begin(115200);
    // Wait briefly for USB CDC; don't block forever in case the bench rig
    // isn't on a host.
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    Serial.println();
    Serial.println(F("=== gio Story 012 smoke test ==="));
    Serial.print(F("VREF (assumed): "));
    Serial.print(BENCH_VREF_V, 4);
    Serial.println(F(" V"));
    Serial.println(F("Loopback: J3 (DAC OUTA via op-amp) → J1 → MCP3208 CH1"));
    Serial.println();

    // SPI bus shared between DAC8552 and MCP3208. Both Rob Tillaart libs call
    // SPI.begin() internally on first transaction; explicit call here makes
    // the clock pin active early so a scope sees something on power-up.
    SPI.begin();

    if (!outputs::begin(PIN_CS_DAC)) {
        Serial.println(F("ERROR: outputs::begin() failed"));
    }
    outputs::setVRef(BENCH_VREF_V);

    if (!inputs::begin(PIN_CS_ADC)) {
        Serial.println(F("ERROR: inputs::begin() failed"));
    }
    inputs::setVRef(BENCH_VREF_V);

    g_encoder.begin(PIN_ENC_A, PIN_ENC_B, PIN_ENC_CLICK);

    // OLED probe — Wire.begin() is called inside Adafruit_SSD1306; begin()
    // returns false if the I2C device doesn't ACK at OLED_I2C_ADDR (0x3C).
    if (g_oled.begin()) {
        Serial.println(F("OLED: detected at 0x3C, ready"));
        g_oled.clear();
        g_oled.showLabel("gio");
        g_oled.show();
    } else {
        Serial.println(F("OLED: NOT detected at 0x3C — check wiring (SDA=D4, SCL=D5, VCC=3V3)"));
    }

    // Park channel-B endpoints at 1/4 and 3/4 of the actual VREF
    // (so the absolute voltages adjust if the bench VREF drifts).
    g_square_low_v  = outputs::getVRef() * 0.25f;
    g_square_high_v = outputs::getVRef() * 0.75f;

    Serial.println(F("Setup complete. Streaming DAC,ADC pairs..."));
    Serial.println(F("# t_ms  dac_a_v  dac_b_v  pot_v  pot_count  j1_v  j1_count  j2_v  j2_count  enc_count  click  long"));
}

void loop() {
    // ---- Channel A: triangle, full DAC range, 1 Hz ----
    uint32_t t_a = millis() % TRIANGLE_PERIOD_MS;
    float phase_a = static_cast<float>(t_a) / static_cast<float>(TRIANGLE_PERIOD_MS);
    float tri     = (phase_a < 0.5f) ? (phase_a * 2.0f) : ((1.0f - phase_a) * 2.0f);
    float dac_a_v = tri * outputs::getVRef();
    outputs::write(DAC_CHANNEL_A, dac_a_v);

    // ---- Channel B: square between 1/4·VREF and 3/4·VREF, 0.5 Hz ----
    uint32_t t_b   = millis() % SQUARE_PERIOD_MS;
    float dac_b_v  = (t_b < SQUARE_PERIOD_MS / 2) ? g_square_low_v : g_square_high_v;
    outputs::write(DAC_CHANNEL_B, dac_b_v);

    // Tiny settle delay for the analog node — RC of the breadboard wiring +
    // ADC sample-and-hold is well under 1 ms, but a few hundred µs is cheap
    // insurance against transient capture during the SPI transaction.
    delayMicroseconds(500);

    uint16_t pot_count = inputs::readRaw(ADC_CH_POT);
    float    pot_v     = inputs::readVolts(ADC_CH_POT);
    uint16_t j1_count  = inputs::readRaw(ADC_CH_J1);
    float    j1_v      = inputs::readVolts(ADC_CH_J1);
    uint16_t j2_count  = inputs::readRaw(ADC_CH_J2);
    float    j2_v      = inputs::readVolts(ADC_CH_J2);

    // Encoder + click. Poll once per loop iteration. delta() returns net ticks
    // since the last call (positive = CW, negative = CCW); reading clears it.
    g_encoder.poll();
    g_enc_count += g_encoder.delta();
    bool click      = g_encoder.pressed();
    bool long_click = g_encoder.pressedLong();

    Serial.print(millis());
    Serial.print('\t');
    Serial.print(dac_a_v, 4);
    Serial.print('\t');
    Serial.print(dac_b_v, 4);
    Serial.print('\t');
    Serial.print(pot_v, 4);
    Serial.print('\t');
    Serial.print(pot_count);
    Serial.print('\t');
    Serial.print(j1_v, 4);
    Serial.print('\t');
    Serial.print(j1_count);
    Serial.print('\t');
    Serial.print(j2_v, 4);
    Serial.print('\t');
    Serial.print(j2_count);
    Serial.print('\t');
    Serial.print(g_enc_count);
    Serial.print('\t');
    Serial.print(click ? 1 : 0);
    Serial.print('\t');
    Serial.println(long_click ? 1 : 0);

    // Refresh the OLED at ~10 Hz with the live encoder count. Keeps the I2C
    // bus mostly idle (a 64×32 framebuffer push is ~700 µs at 400 kHz I2C).
    if (g_oled.ready() && (millis() - g_last_oled_ms) >= OLED_REFRESH_MS) {
        g_last_oled_ms = millis();
        g_oled.clear();
        g_oled.showParameter("ENC", g_enc_count);
        g_oled.show();
    }

    delay(STEP_MS);
}
