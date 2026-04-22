#include "tempo.h"
#include <cmath>

namespace tempo {

uint32_t bpmToStepMs(float bpm, uint8_t sub) {
    if (bpm < BPM_MIN) bpm = BPM_MIN;
    if (bpm > BPM_MAX) bpm = BPM_MAX;
    if (sub == 0)      sub = 1;
    // ms per quarter = 60000 / bpm; ms per step = (60000 / bpm) / sub
    float ms = 60000.0f / (bpm * (float)sub);
    return (uint32_t)(ms + 0.5f);
}

float potToBpm(float pot) {
    if (pot < 0.0f) pot = 0.0f;
    if (pot > 1.0f) pot = 1.0f;
    // Exponential: BPM = BPM_MIN * (BPM_MAX/BPM_MIN)^pot.
    // Equivalent to a constant doubling-per-rotation feel.
    return BPM_MIN * powf(BPM_MAX / BPM_MIN, pot);
}

} // namespace tempo
