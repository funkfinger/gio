#include "trigger_in.h"

namespace trigger_in {

Schmitt::Schmitt(float high_threshold, float low_threshold)
    : high_(high_threshold), low_(low_threshold) {}

bool Schmitt::poll(float volts) {
    if (armed_) {
        // Waiting for a rising edge.
        if (volts >= high_) {
            armed_ = false;
            return true;
        }
    } else {
        // Just fired (or just reset / just constructed). Re-arm only after
        // the input drops below the low threshold — this is the hysteresis
        // that prevents ripple at the high threshold from multi-firing, and
        // the mechanism that prevents a slow rising ramp from firing twice.
        if (volts <= low_) {
            armed_ = true;
        }
    }
    return false;
}

void Schmitt::reset() {
    armed_ = false;
}

} // namespace trigger_in
