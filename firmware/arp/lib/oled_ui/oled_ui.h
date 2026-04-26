#pragma once
#include <cstdint>
#include <Adafruit_SSD1306.h>
#include "screens.h"

// ---------------------------------------------------------------------------
// OLED hardware config — change THESE THREE constants when swapping displays.
//
//   Bench bring-up (0.91" 128×32 landscape):  WIDTH=128, HEIGHT=32, ROTATION=0
//   Final design   (0.49" 64×32 portrait):    WIDTH=64,  HEIGHT=32, ROTATION=1
//
// ROTATION values: 0=native landscape, 1=90° CW (portrait), 2=180°, 3=270° CW.
// ---------------------------------------------------------------------------
#define OLED_WIDTH     64
#define OLED_HEIGHT    32
#define OLED_ROTATION   1

#define OLED_I2C_ADDR 0x3C   // both 0.91" and 0.49" SSD1306 default to 0x3C

class OledUI {
public:
    bool begin(uint8_t i2cAddr = OLED_I2C_ADDR);

    // Clear backbuffer (does not push to display until show()).
    void clear();

    // One-line label (centred-ish). Useful for bring-up and splash.
    void showLabel(const char* label);

    // Two-line "Name = Value" parameter display.
    void showParameter(const char* name, int32_t value);

    // Beat indicator dot in a corner. Caller toggles per beat.
    void showBeat(bool on);

    // Draw a generated screen bitmap at (x, y). Default (0, 0) centres a
    // full-panel bitmap in the top-left. Uses SSD1306_WHITE; a lit pixel in
    // the source PNG becomes a white pixel on the OLED. Does NOT call show().
    void drawScreen(const screens::Screen& s, int16_t x = 0, int16_t y = 0);

    // Blocking: play a generated animation from first frame to last. The
    // first frame is held on screen for `first_frame_hold_ms` (useful for
    // boot-splash behaviour); all subsequent frames tick at `frame_delay_ms`.
    // Calls show() internally after each frame. Draws each frame at (x, y);
    // default origin draws full-panel animations in the top-left.
    void playAnimation(const screens::Animation& ani,
                       uint16_t first_frame_hold_ms,
                       uint16_t frame_delay_ms,
                       int16_t x = 0,
                       int16_t y = 0);

    // Push backbuffer to the display.
    void show();

    // Escape hatch for code that needs raw GFX access.
    Adafruit_SSD1306& raw() { return display_; }

    // Has begin() succeeded? Helpful for early-bail in callers that draw
    // outside the standard helpers.
    bool ready() const { return ready_; }

private:
    Adafruit_SSD1306 display_{OLED_WIDTH, OLED_HEIGHT, &Wire, -1};
    bool             ready_ = false;
};
