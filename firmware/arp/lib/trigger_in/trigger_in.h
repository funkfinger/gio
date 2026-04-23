#pragma once

// Firmware Schmitt trigger: detect rising edges on a voltage signal that's
// being polled at the loop rate (typically the buffered output of the input
// stage — see Story 015).
//
// Why in firmware rather than a hardware comparator (LM393, 74HC14): the
// op-amp-buffered input gives us a clean signal at the ADC; the analog
// hysteresis baked into this state machine is sufficient for trigger-rate
// signals and saves a part. If audio-rate trigger detection (>1 kHz) is ever
// needed, revisit.
//
// Threshold convention is in JACK volts (post-firmware-inverse-mapping), not
// raw ADC volts. Defaults match Eurorack convention: trigger high = +1.5 V
// (conservative; standard is ≥1 V), low = +0.5 V (1 V of hysteresis kills
// ripple / slow-ramp false fires).

namespace trigger_in {

class Schmitt {
public:
    // Construct with optional custom thresholds (in jack volts).
    explicit Schmitt(float high_threshold = 1.5f, float low_threshold = 0.5f);

    // Feed the latest input voltage. Returns true exactly once on a rising
    // edge that crosses the high threshold AFTER having dropped below the
    // low threshold.
    //
    // Initial state is "not armed" — the input must drop below low_threshold
    // before the first rising edge can fire. This prevents a spurious trigger
    // if the input idles HIGH at startup (e.g. a normalled-high jack).
    bool poll(float volts);

    // Force-reset the state machine to "not armed" (caller must drop below
    // low_threshold before the next rising edge can fire).
    void reset();

    // Diagnostic: is the Schmitt currently armed (i.e. waiting for a rising
    // edge)? True after the input has dropped below low_threshold; false
    // after a fire, until the next low excursion.
    bool armed() const { return armed_; }

private:
    float high_;
    float low_;
    bool  armed_ = false;
};

} // namespace trigger_in
