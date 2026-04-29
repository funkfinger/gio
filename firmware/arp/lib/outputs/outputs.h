#pragma once
#include <cstdint>

// HAL wrapper around DAC8552 (16-bit dual SPI DAC).
//
// Two channels: 0 = OUTA, 1 = OUTB.
//
// `write(ch, volts)` takes a target jack voltage (typically −10..+10 V after
// the inverting summing-amp output stage in Story 013), applies per-channel
// `gain` / `offset` calibration, and clamps to the DAC's 0..VREF range before
// pushing the count over SPI.
//
// `gate(ch, on)` is a convenience for digital-style use (write VREF or 0 V).
//
// Calibration constants are filled in at bench time via `setCalibration()` —
// initially `gain = 1.0, offset = 0.0` means the firmware writes the raw DAC
// volts (no scaling), and `voltsToCount()` does a straight VREF-relative map.
//
// Usage:
//     outputs::begin(CS_DAC_PIN);
//     outputs::setVRef(4.096f);           // bench-tuned (or 5.0 if VREF=VDD)
//     outputs::write(0, 2.048f);          // mid-scale
//     outputs::gate(1, true);             // OUTB high
namespace outputs {

constexpr uint8_t CHANNEL_COUNT = 2;

// One-time setup. Call after Serial.begin() so error messages can print.
// CS pin should be pulled HIGH externally (10 kΩ to +3.3 V) so the IC starts
// deselected. Returns true on success.
bool begin(uint8_t cs_pin);

// Update the firmware's notion of the hardware VREF. Default = 4.096 V.
// Affects voltsToCount() math everywhere.
void setVRef(float vref_volts);
float getVRef();

// Per-channel calibration. After bench-fitting:
//   actual_dac_volts = gain * target_volts + offset
// Defaults are gain=1.0, offset=0.0 (passthrough — caller's `volts` is the DAC
// voltage directly, not the post-output-stage jack voltage).
void setCalibration(uint8_t channel, float gain, float offset);

// Write a target DAC voltage on `channel`. Out-of-range values are clamped
// to [0, VREF]. Returns the count actually written (for diagnostics).
uint16_t write(uint8_t channel, float volts);

// Convenience: `on=true` → VREF, `on=false` → 0. Useful when an output is
// configured as a gate by the application layer.
void gate(uint8_t channel, bool on);

// Diagnostic: write a raw 16-bit count without calibration or clamping.
void writeRaw(uint8_t channel, uint16_t count);

}  // namespace outputs
