# Story 003: MCP4725 DAC Ramp and Scale Quantiser

**As a** bench engineer and developer
**I want** (a) the MCP4725 I2C DAC to emit a visually clean linear voltage ramp, and (b) a pure-logic scale quantiser with host unit tests
**So that** the key novel hardware subsystem (I2C DAC) is validated in isolation, and the first piece of musical logic exists with TDD scaffolding in place

Bundled in one PR because they share the same milestone: "novel subsystems proved, on their own terms."

## Acceptance Criteria — MCP4725 DAC Ramp (bench)

- [ ] MCP4725 wired to XIAO RP2350 on I2C bus (D4=SDA, D5=SCL), powered from 3.3V
- [ ] Firmware writes a 0→4095 linear ramp via `Adafruit_MCP4725::setVoltage()` in a loop at ~10 Hz
- [ ] Rigol DS1054Z shows 0V → ~3.3V linear sawtooth on MCP4725 VOUT pin, no missing steps, no glitches
- [ ] Ramp frequency and full-scale voltage recorded in `docs/bench-log.md` with scope screenshot in `requirements/`
- [ ] No op-amp yet — this measures the raw MCP4725 output only
- [ ] Confirm SSD1306 OLED (if wired) does not disrupt DAC I2C transactions (bus sharing smoke test)

## Acceptance Criteria — Scale Quantiser (host TDD)

- [ ] `firmware/arp/lib/scales/scales.h` and `scales.cpp` added — pure C++, no `<Arduino.h>`
- [ ] `uint8_t quantize(uint8_t midiNote, Scale scale)` — nearest in-scale note; ties break downward
- [ ] All 6 scales defined: Major, Natural Minor, Pentatonic Major, Pentatonic Minor, Dorian, Chromatic
- [ ] `Scale scaleFromPot(float pot, Scale current)` with ±2% hysteresis (reused from RA4M1)
- [ ] Tests under `firmware/arp/test/test_scales/` pass via `pio test -e native`:
  - Each scale's in-scale notes pass through unchanged
  - Out-of-scale notes map to nearest in-scale tone (ties break downward)
  - Chromatic is identity for all 128 MIDI notes
  - Octave invariance: `quantize(n, s) + 12 == quantize(n + 12, s)` for all n in range
  - `scaleFromPot` zone centres, hysteresis, sweep visits all 6 in order
- [ ] CI runs `pio test -e native` and stays green

## Notes

- The `scales` library code and tests can be ported verbatim from `xiao-ra4m1-arp`. The goal of this story is to confirm they compile and pass in the new PlatformIO env, not to rewrite them.
- MCP4725 `setVoltage(value, false)` writes to the DAC register (not EEPROM). Use `false` for the second param to avoid EEPROM wear during the ramp loop.
- GoogleTest requires an explicit `main()` in the test file (PlatformIO does not auto-link `gtest_main`).
- I2C address confirmation: MCP4725A0 → 0x60. Wire `Wire.begin()` before `mcp.begin(0x60)`.

## Depends on

- Story 002

## Status

not started
