#include "encoder_input.h"
#include <Arduino.h>
#include <RotaryEncoder.h>

static const uint32_t CLICK_DEBOUNCE_MS = 50;

// LatchMode chosen to match this build's PEC11. FOUR0 = one detent per four
// raw quadrature transitions, latching at state 00 — typical for full-step
// PEC11s (e.g. PEC11R-4015F-S0024). If you see one detent producing two
// counts, try FOUR3. If you see one detent producing 0.5 counts (every other
// detent missed), try TWO03.

namespace {
RotaryEncoder* gEnc = nullptr;

// Interrupt handler — called from CHANGE ISRs on both A and B pins so the
// state machine never misses a transition, even if the main loop is busy
// driving the OLED, SPI, or serial. mathertel/RotaryEncoder::tick() is
// short and reentrant-safe (just reads two pins + updates internal state).
// arduino-pico places ISRs in flash by default; no IRAM attribute needed
// (the RP2350 instruction cache handles flash-resident ISR latency fine
// for a hand-turned encoder).
void gEncIsr() {
    if (gEnc) gEnc->tick();
}
} // namespace

void EncoderInput::begin(uint8_t pinA, uint8_t pinB, uint8_t pinClick) {
    pinMode(pinA,     INPUT_PULLUP);
    pinMode(pinB,     INPUT_PULLUP);
    pinMode(pinClick, INPUT_PULLUP);
    pinClick_ = pinClick;

    // mathertel/RotaryEncoder does not own its pins beyond construction;
    // we hold one static instance to keep memory layout predictable.
    static RotaryEncoder s_enc(pinA, pinB, RotaryEncoder::LatchMode::FOUR3);
    gEnc = &s_enc;
    impl_ = gEnc;

    // Attach CHANGE ISRs on both quadrature pins so the decoder never misses
    // a transition. tick() is still called from poll() too as a belt-and-
    // suspenders safety (cheap; just reads pins + updates state).
    attachInterrupt(digitalPinToInterrupt(pinA), gEncIsr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(pinB), gEncIsr, CHANGE);

    lastPos_         = gEnc->getPosition();
    pending_         = 0;
    pressedFlag_     = false;
    longPressedFlag_ = false;
    buttonDown_      = (digitalRead(pinClick_) == LOW);
    pressStartMs_    = 0;
    lastEdgeMs_      = 0;
}

void EncoderInput::poll() {
    if (gEnc) {
        gEnc->tick();
        long pos = gEnc->getPosition();
        long d   = pos - lastPos_;
        lastPos_ = pos;
        if (d != 0) {
            // Clamp into int8_t; large step skips would only happen if poll()
            // were starved for many ms, in which case losing precision is fine.
            if (d >  127) d =  127;
            if (d < -128) d = -128;
            int16_t sum = (int16_t)pending_ + (int16_t)d;
            if (sum >  127) sum =  127;
            if (sum < -128) sum = -128;
            pending_ = (int8_t)sum;
        }
    }

    // Debounced press tracking.
    //   - Long-press fires once at the threshold while still held (tactile feedback).
    //   - Release of a long-press gesture is silent (no short-click follows).
    //   - Short-press fires on release, only if the long-press never armed.
    bool now = (digitalRead(pinClick_) == LOW);
    uint32_t t = millis();

    if (now != buttonDown_ && (t - lastEdgeMs_) > CLICK_DEBOUNCE_MS) {
        lastEdgeMs_ = t;
        if (now) {
            // Press-down edge
            buttonDown_           = true;
            pressStartMs_         = t;
            longFiredThisGesture_ = false;
        } else {
            // Release edge
            if (!longFiredThisGesture_) {
                pressedFlag_ = true;   // short click
            }
            buttonDown_           = false;
            longFiredThisGesture_ = false;
        }
    }

    // While still held: fire long-press once we've crossed the threshold.
    if (buttonDown_ && !longFiredThisGesture_
        && (t - pressStartMs_) >= EncoderInput::LONG_PRESS_MS) {
        longPressedFlag_      = true;
        longFiredThisGesture_ = true;
    }
}

int8_t EncoderInput::delta() {
    int8_t d = pending_;
    pending_ = 0;
    return d;
}

bool EncoderInput::pressed() {
    bool p = pressedFlag_;
    pressedFlag_ = false;
    return p;
}

bool EncoderInput::pressedLong() {
    bool p = longPressedFlag_;
    longPressedFlag_ = false;
    return p;
}
