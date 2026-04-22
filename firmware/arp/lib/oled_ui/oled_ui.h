#pragma once
#include <cstdint>
#include <Adafruit_SSD1306.h>

// ---------------------------------------------------------------------------
// OLED hardware config — change THESE THREE constants when swapping displays.
//
//   Bench bring-up (0.91" 128×32 landscape):  WIDTH=128, HEIGHT=32, ROTATION=0
//   Final design   (0.49" 64×32 portrait):    WIDTH=64,  HEIGHT=32, ROTATION=1
//
// ROTATION values: 0=native landscape, 1=90° CW (portrait), 2=180°, 3=270° CW.
// ---------------------------------------------------------------------------
#define OLED_WIDTH    128
#define OLED_HEIGHT    32
#define OLED_ROTATION   0

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

    // Push backbuffer to the display.
    void show();

    // Escape hatch for code that needs raw GFX access.
    Adafruit_SSD1306& raw() { return display_; }

private:
    Adafruit_SSD1306 display_{OLED_WIDTH, OLED_HEIGHT, &Wire, -1};
    bool             ready_ = false;
};
