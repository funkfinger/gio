#pragma once
#include <cstdint>

// HAL wrapper around MCP3208 (12-bit 8-channel SPI ADC).
//
// `readVolts(ch)` returns the **jack-side** voltage after applying per-channel
// calibration. With default `gain=1.0, offset=0.0`, the returned value is
// the raw ADC voltage (0..VREF). Once the input op-amp stage in Story 015
// is in place and bench-calibrated, `gain` / `offset` map ADC volts back to
// jack volts (typical mapping: jack +10 V → ADC 0 V, jack −10 V → ADC VREF).
//
// `readRaw(ch)` returns the unmodified 12-bit ADC count (0..4095) for
// diagnostics.
//
// Usage:
//     inputs::begin(CS_ADC_PIN);
//     inputs::setVRef(4.096f);
//     float v = inputs::readVolts(0);     // ch 0 = J1
namespace inputs {

constexpr uint8_t CHANNEL_COUNT = 8;

bool begin(uint8_t cs_pin);

void setVRef(float vref_volts);
float getVRef();

// After bench-fitting:
//   jack_volts = gain * adc_volts + offset
// Defaults are gain=1.0, offset=0.0 (passthrough — caller gets raw ADC volts).
void setCalibration(uint8_t channel, float gain, float offset);

// Read calibrated jack voltage on `channel`. Returns NaN-safe (0.0) on error.
float readVolts(uint8_t channel);

// Raw 12-bit ADC count (0..4095). Useful for diagnostics + scope-correlation.
uint16_t readRaw(uint8_t channel);

}  // namespace inputs
