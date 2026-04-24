#include "oled_ui.h"
#include <Wire.h>

bool OledUI::begin(uint8_t i2cAddr) {
    // Adafruit_SSD1306::begin() returns true on successful init.
    if (!display_.begin(SSD1306_SWITCHCAPVCC, i2cAddr)) {
        ready_ = false;
        return false;
    }
    display_.setRotation(OLED_ROTATION);
    display_.clearDisplay();
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    display_.display();
    ready_ = true;
    return true;
}

void OledUI::clear() {
    if (!ready_) return;
    display_.clearDisplay();
}

void OledUI::showLabel(const char* label) {
    if (!ready_) return;
    display_.clearDisplay();
    display_.setCursor(0, 0);
    display_.setTextSize(2);
    display_.println(label);
}

void OledUI::showParameter(const char* name, int32_t value) {
    if (!ready_) return;
    display_.clearDisplay();
    display_.setCursor(0, 0);
    display_.setTextSize(1);
    display_.println(name);
    display_.setTextSize(2);
    display_.println(value);
}

void OledUI::showBeat(bool on) {
    if (!ready_) return;
    // 4×4 dot in the bottom-right corner; caller is responsible for show().
    int16_t w = display_.width();
    int16_t h = display_.height();
    if (on) display_.fillRect(w - 5, h - 5, 4, 4, SSD1306_WHITE);
    else    display_.fillRect(w - 5, h - 5, 4, 4, SSD1306_BLACK);
}

void OledUI::drawScreen(const screens::Screen& s, int16_t x, int16_t y) {
    if (!ready_) return;
    display_.drawBitmap(x, y, s.data, s.width, s.height, SSD1306_WHITE);
}

void OledUI::playAnimation(const screens::Animation& ani,
                           uint16_t first_frame_hold_ms,
                           uint16_t frame_delay_ms,
                           int16_t  x,
                           int16_t  y) {
    if (!ready_) return;
    for (uint8_t i = 0; i < ani.count; ++i) {
        display_.clearDisplay();
        display_.drawBitmap(x, y, ani.frames[i]->data, ani.frames[i]->width,
                            ani.frames[i]->height, SSD1306_WHITE);
        display_.display();
        delay(i == 0 ? first_frame_hold_ms : frame_delay_ms);
    }
}

void OledUI::show() {
    if (!ready_) return;
    display_.display();
}
