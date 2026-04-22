# Story 007: OLED + Encoder Brought Up

**As a** developer
**I want** the 64×32 SSD1306 OLED and PEC11 encoder wired and working independently of the arp engine
**So that** I can confirm the UI hardware before integrating it with the generative engine

## Acceptance Criteria

- [ ] SSD1306 OLED initialises on I2C bus (address 0x3C). I2C bus is dedicated to OLED (no MCP4725 — DAC is PWM-based; see decisions §3, §4)
- [ ] OLED displays a simple label (e.g. "HELLO") on power-on, rotated 90° to portrait orientation (32px wide × 64px tall)
- [ ] Encoder rotation increments/decrements an integer counter; counter value displays on OLED in real time
- [ ] Encoder click prints "CLICK" to serial and flashes the onboard LED
- [ ] `lib/encoder_input/` stub created with `encoderDelta()` and `encoderPressed()` API
- [ ] `lib/oled_ui/` stub created with `showParameter(name, value)` and `showBeat()` API
- [ ] No arp integration yet — this story is isolated UI bring-up
- [ ] Bench notes added to `docs/bench-log.md`

## Notes

- SSD1306 64×32 init with Adafruit SSD1306: `display.begin(SSD1306_SWITCHCAPVCC, 0x3C)` with width=64, height=32; call `display.setRotation(1)` for portrait.
- I2C bus is OLED-only (PWM DAC, not MCP4725). No bus contention to bench-verify.
- Encoder: INPUT_PULLUP on D8, D9, D10. Debounce click with 50 ms gap.
- `lib/encoder_input/` and `lib/oled_ui/` are HAL modules — no host tests; bench-verified only.
- Encoder library: `mathertel/RotaryEncoder` (polling, pure C++, no AVR-specific register macros). Paul Stoffregen's Encoder library was tried in Story 002 era and removed — it uses `DIRECT_PIN_READ` / `pin1_register` macros that don't exist on RP2350. Choice documented in `decisions.md` §17.

## Depends on

- Story 006

## Status

done — bench-confirmed 2026-04-22 with bench-substitute 0.91" 128×32 OLED (final design uses 0.49" 64×32; one-line swap in `oled_ui.h`). PEC11 encoder rotation + click both functional. Encoder library decision documented in `decisions.md` §17 (`mathertel/RotaryEncoder`).
