#pragma once
#include <cstdint>

// Wraps mathertel/RotaryEncoder + a debounced click button.
// Polling-based: call poll() frequently from the main loop.
class EncoderInput {
public:
    // Pin assignments per docs/generative-arp-module.md §2.4:
    //   pinA = D8 (GP2), pinB = D9 (GP3), pinClick = D10 (GP4)
    void begin(uint8_t pinA, uint8_t pinB, uint8_t pinClick);

    // Sample encoder + button. Call this every loop().
    void poll();

    // Net step delta since the last call (positive = CW, negative = CCW).
    // Returns 0 if no movement. Reading clears the accumulator.
    int8_t delta();

    // Short click: latched on release, only if the press was shorter than
    // LONG_PRESS_MS. Reading clears the latched flag.
    bool pressed();

    // Long press: latched on release, only if the press was at least
    // LONG_PRESS_MS. A long press SUPPRESSES the short click for the same
    // gesture. Reading clears the latched flag.
    bool pressedLong();

    static constexpr uint32_t LONG_PRESS_MS = 500;

private:
    void* impl_      = nullptr; // opaque handle to RotaryEncoder
    uint8_t pinClick_ = 0xFF;
    long    lastPos_  = 0;
    int8_t  pending_  = 0;

    // Press / debounce state. Long-press fires the moment we cross the
    // threshold while the button is still held; the release edge of the same
    // gesture is then suppressed (no spurious short-press follows a long-press).
    bool     buttonDown_     = false; // current debounced state
    uint32_t lastEdgeMs_     = 0;     // for debounce timing
    uint32_t pressStartMs_   = 0;     // millis() at press-down edge
    bool     longFiredThisGesture_ = false;
    bool     pressedFlag_    = false; // latched short-press
    bool     longPressedFlag_= false; // latched long-press
};
