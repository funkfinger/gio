#pragma once
#include <cstdint>
#include "scales.h"

// Convert a V/oct CV reading at the JACK NODE (post-buffer voltage in volts,
// 0..~8 V) to a semitone transpose offset, snapped first to the nearest
// semitone (V/oct hygiene) then to the nearest scale degree of `scale`.
//
// Conventions:
//   - 0 V → 0 semitones (no transpose) — under α-summing, this is the
//     unplugged baseline because the input is normalled to GND.
//   - +1 V → +12 semitones (one octave up).
//   - Negative volts clamp to 0 (the divider doesn't pass them through anyway,
//     but defensive against ADC noise wrapping).
//   - Volts above 8 V clamp to 8 V (96 semitones max).
//
// The scale-snap preserves the octave count; only the within-octave
// pitch class gets snapped.
int cvVoltsToTranspose(float volts, Scale scale);
