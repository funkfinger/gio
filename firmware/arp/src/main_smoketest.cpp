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
//   2. Walks DAC channel A from 0 → VREF → 0 (1-second triangle)
//   3. After each DAC write, reads MCP3208 channel 0 and prints both to serial
//   4. Bench setup: jumper DAC OUTA directly to MCP3208 channel 0
//   5. Expected: ADC reading tracks DAC value linearly within a few LSBs
//
// Pin assignments per docs/bench-wiring.md §3 + §4 (SPI-pivot map):
//     SPI: SCK=D8/GP2, MISO=D9/GP4, MOSI=D10/GP3
//     CS_DAC=D3/GP5, CS_ADC=D6/GP0  (both with 10 kΩ pull-up to +3.3 V)

#include <Arduino.h>
#include <SPI.h>

#include "inputs.h"
#include "outputs.h"

static constexpr uint8_t PIN_CS_DAC = D3;  // GP5 — DAC8552 /SYNC
static constexpr uint8_t PIN_CS_ADC = D6;  // GP0 — MCP3208 /CS

// Bench substitution: VREF supplied by the pot+TL072 buffer at ~4.096 V.
// Re-measure with a multimeter and update if needed.
static constexpr float BENCH_VREF_V = 4.096f;

// Triangle parameters.
static constexpr uint32_t TRIANGLE_PERIOD_MS = 1000;  // full cycle (up + down)
static constexpr uint32_t STEP_MS            = 20;    // ~50 samples per cycle
static constexpr uint8_t  DAC_CHANNEL        = 0;     // A
static constexpr uint8_t  ADC_CHANNEL        = 0;     // jumper OUTA → ch0

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
    Serial.println(F("Loopback: jumper DAC OUTA → MCP3208 ch 0"));
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

    Serial.println(F("Setup complete. Streaming DAC,ADC pairs..."));
    Serial.println(F("# t_ms  dac_v   adc_v   adc_count"));
}

void loop() {
    // Phase: 0..1 across one period, then mirrored to make a triangle.
    uint32_t t = millis() % TRIANGLE_PERIOD_MS;
    float phase = static_cast<float>(t) / static_cast<float>(TRIANGLE_PERIOD_MS);
    float tri   = (phase < 0.5f) ? (phase * 2.0f) : ((1.0f - phase) * 2.0f);
    float dac_v = tri * outputs::getVRef();

    outputs::write(DAC_CHANNEL, dac_v);

    // Tiny settle delay for the analog node — RC of the breadboard wiring +
    // ADC sample-and-hold is well under 1 ms, but a few hundred µs is cheap
    // insurance against transient capture during the SPI transaction.
    delayMicroseconds(500);

    uint16_t adc_count = inputs::readRaw(ADC_CHANNEL);
    float adc_v        = inputs::readVolts(ADC_CHANNEL);

    Serial.print(millis());
    Serial.print('\t');
    Serial.print(dac_v, 4);
    Serial.print('\t');
    Serial.print(adc_v, 4);
    Serial.print('\t');
    Serial.println(adc_count);

    delay(STEP_MS);
}
