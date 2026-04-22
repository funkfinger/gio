#pragma once
#include <cstdint>

namespace tempo {

inline constexpr float BPM_MIN = 20.0f;
inline constexpr float BPM_MAX = 300.0f;

// Convert BPM to step period in milliseconds.
// sub = subdivisions per quarter note (1 = quarter, 2 = eighth, 4 = sixteenth).
uint32_t bpmToStepMs(float bpm, uint8_t sub = 1);

// Map a normalised pot reading [0.0, 1.0] EXPONENTIALLY to BPM_MIN..BPM_MAX.
// BPM = BPM_MIN * (BPM_MAX/BPM_MIN)^pot. Endpoints are exact; the curve gives
// a constant ratio per equal slice of pot rotation (≈2.47× per third-turn for
// the current 20..300 BPM range). Pot values outside [0, 1] are clamped.
float potToBpm(float pot);

} // namespace tempo
