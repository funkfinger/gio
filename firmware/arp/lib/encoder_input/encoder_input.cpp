#include "encoder_input.h"
#include <Arduino.h>
#include <RotaryEncoder.h>

static const uint32_t CLICK_DEBOUNCE_MS = 50;

// LatchMode::TWO03 = one detent per two raw quadrature transitions, latches at
// the rest positions of typical PEC11 encoders. Gives one count per click on
// a standard 24-detent encoder.

namespace {
RotaryEncoder* gEnc = nullptr;
} // namespace

void EncoderInput::begin(uint8_t pinA, uint8_t pinB, uint8_t pinClick) {
    pinMode(pinA,     INPUT_PULLUP);
    pinMode(pinB,     INPUT_PULLUP);
    pinMode(pinClick, INPUT_PULLUP);
    pinClick_ = pinClick;

    // mathertel/RotaryEncoder does not own its pins beyond construction;
    // we hold one static instance to keep memory layout predictable.
    static RotaryEncoder s_enc(pinA, pinB, RotaryEncoder::LatchMode::TWO03);
    gEnc = &s_enc;
    impl_ = gEnc;

    lastPos_      = gEnc->getPosition();
    pending_      = 0;
    pressedFlag_  = false;
    clickLast_    = (digitalRead(pinClick_) == LOW);
    lastClickMs_  = 0;
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

    // Debounced click: detect HIGH→LOW edge on PIN_CLICK.
    bool now = (digitalRead(pinClick_) == LOW);
    uint32_t t = millis();
    if (now != clickLast_ && (t - lastClickMs_) > CLICK_DEBOUNCE_MS) {
        if (now) pressedFlag_ = true; // edge into pressed
        clickLast_   = now;
        lastClickMs_ = t;
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
