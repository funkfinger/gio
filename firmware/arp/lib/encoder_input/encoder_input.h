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

    // True once per click edge (rising falling-edge of the button to LOW),
    // debounced. Reading clears the latched flag.
    bool pressed();

private:
    void* impl_      = nullptr; // opaque handle to RotaryEncoder
    uint8_t pinClick_ = 0xFF;
    long    lastPos_  = 0;
    int8_t  pending_  = 0;

    // Click debounce
    bool     clickLast_   = false; // last sampled state (LOW = pressed)
    bool     pressedFlag_ = false;
    uint32_t lastClickMs_ = 0;
};
