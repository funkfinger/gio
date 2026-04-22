#pragma once
#include <cstdint>

namespace tempo {

inline constexpr float BPM_MIN = 30.0f;
inline constexpr float BPM_MAX = 300.0f;

// Convert BPM to step period in milliseconds.
// sub = subdivisions per quarter note (1 = quarter, 2 = eighth, 4 = sixteenth).
uint32_t bpmToStepMs(float bpm, uint8_t sub = 1);

// Map a normalised pot reading [0.0, 1.0] linearly to BPM_MIN..BPM_MAX.
// Pot values outside the range are clamped.
float potToBpm(float pot);

} // namespace tempo
