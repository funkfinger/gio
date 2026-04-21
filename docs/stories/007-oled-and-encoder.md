# Story 007: OLED + Encoder Brought Up

**As a** developer
**I want** the 64×32 SSD1306 OLED and PEC11 encoder wired and working independently of the arp engine
**So that** I can confirm the UI hardware before integrating it with the generative engine

## Acceptance Criteria

- [ ] SSD1306 OLED (64×32) initialises on I2C bus (address 0x3C) alongside MCP4725 (0x60) — no conflict
- [ ] OLED displays a simple label (e.g. "HELLO") on power-on, rotated 90° to portrait orientation (32px wide × 64px tall)
- [ ] Encoder rotation increments/decrements an integer counter; counter value displays on OLED in real time
- [ ] Encoder click prints "CLICK" to serial and flashes the onboard LED
- [ ] `lib/encoder_input/` stub created with `encoderDelta()` and `encoderPressed()` API
- [ ] `lib/oled_ui/` stub created with `showParameter(name, value)` and `showBeat()` API
- [ ] No arp integration yet — this story is isolated UI bring-up
- [ ] Bench notes added to `docs/bench-log.md`

## Notes

- SSD1306 64×32 init with Adafruit SSD1306: `display.begin(SSD1306_SWITCHCAPVCC, 0x3C)` with width=64, height=32; call `display.setRotation(1)` for portrait.
- I2C bus shared with MCP4725 — confirm OLED does not cause glitches on DAC output during I2C bus sharing. Scope the DAC output while OLED refreshes.
- Encoder: INPUT_PULLUP on D8, D9, D10. Debounce click with 50 ms gap.
- `lib/encoder_input/` and `lib/oled_ui/` are HAL modules — no host tests; bench-verified only.
- Encoder library choice: try `Encoder.h` (Paul Stoffregen) first — interrupt-driven, reliable. Document choice in `decisions.md`.

## Depends on

- Story 006

## Status

not started
