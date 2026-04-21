# Design & Process Decisions

Captured from the project kick-off. These settle scope, tooling, and workflow for the first revision of the module. Source of truth for decisions that aren't obvious from the code.

---

## Context

This repo (`gio`) is a 2 HP Eurorack generative arpeggiator based on the XIAO RP2350, specified in [`generative-arp-module.md`](./generative-arp-module.md). It is a spiritual successor to [`xiao-ra4m1-arp`](https://github.com/funkfinger/xiao-ra4m1-arp), adopting the design pivot (encoder + OLED, 2HP, more CV inputs) as the baseline rather than a retrofit.

---

## Decisions

### 1. Monorepo with three top-level areas

```
gio/
├── firmware/arp/     PlatformIO project for the arpeggiator firmware
├── hardware/         KiCad project (schematic, PCB, panel) — added post-v0.1
└── docs/             Spec, decisions, stories
```

**Why:** hardware and firmware revs are tightly coupled on a single-PCB module. Pin assignments are the contract between them and should version together.

### 2. PlatformIO as the build system

Using the earlephilhower arduino-pico platform:

```ini
platform = https://github.com/earlephilhower/arduino-pico.git
board    = seeed_xiao_rp2350
framework = arduino
```

**Why PlatformIO:** scriptable builds, `pio test -e native` for host-side unit tests, library management, CI-friendly.

**Fallback:** `arduino-cli` if PlatformIO hits a wall with the RP2350 board definition.

### 3. External DAC (MCP4725 I2C) instead of onboard DAC

The RP2350 has no onboard DAC. MCP4725 is the simplest drop-in:
- 12-bit, same resolution as the RA4M1 DAC
- I2C, shares bus with OLED → no extra pins
- Adafruit library, well-supported on arduino-pico
- ~$1.50 in breakout form, available as SOT-23-6 for PCB

**Alternatives considered:**
- MCP4922 (SPI dual DAC) — more pins, overkill for one CV output
- PWM + RC filter — lower accuracy (~8-bit effective), harder to calibrate

### 4. I2C bus shared between MCP4725 (DAC) and SSD1306 (OLED)

Hardware I2C0 on D4/D5. MCP4725 is address 0x60; SSD1306 is 0x3C. No conflict.

**Trade-off:** I2C DAC updates go through software transaction overhead (~15–20 µs at 400 kHz). At Eurorack control rates this is negligible — the MCP4725 settles in ~7 µs anyway.

**Why not software I2C:** hardware I2C on D4/D5 does not consume any ADC pins on the RP2350 (unlike the RA4M1 where D5 was ADC-capable). No reason to use software I2C here.

### 5. MVP (v0.1) is deliberately minimal

First working firmware:
- Internal clock only (tempo pot drives it)
- Major scale only
- Up-arp, 4 notes (root, 3rd, 5th, octave)
- MCP4725 → op-amp → V/Oct out, calibrated
- Gate out via NPN transistor stage
- OLED shows tempo BPM only
- Encoder click cycles through arp orders
- No chaos, no CV in processing, no calibration mode, no NeoPixel

**Why:** the one genuinely novel subsystem is RP2350 → MCP4725 I2C → MCP6002 → Eurorack V/Oct. Prove that before stacking features.

### 6. Reuse logic libraries from xiao-ra4m1-arp verbatim

`lib/scales/`, `lib/arp/`, `lib/tempo/` are pure C++ with no Arduino dependencies. They port without modification. Their existing GoogleTest suites run on the RP2350 native env unchanged.

**Why:** they were TDD'd and bench-validated on the RA4M1 project. No reason to rewrite them.

### 7. KiCad deferred until breadboard signal chain validates

Same rule as RA4M1 project: don't open KiCad until DAC → MCP6002 → multimeter matches the calibration voltage table within ±2 mV.

**New risk for this project:** the MCP4725 I2C transaction and the OLED update compete for the same I2C bus. Confirm the bus sharing works cleanly before committing to PCB traces.

### 8. Test strategy: host TDD for pure logic, bench for HAL

- **Host tests (`pio test -e native`):** `scales`, `arp`, `tempo` — no Arduino deps, pure logic
- **Bench-verified:** `dac_out` (MCP4725 via I2C), `gate_out`, `oled_ui`, `encoder_input`, `clock_in`

Bench workflow: oscilloscope (Rigol DS1054Z) for timing, multimeter for V/Oct voltages, VCO + tuner for pitch tracking.

### 9. 64×32 OLED (0.49"), mounted vertically

The 64×32 SSD1306 at 0.49" fits within the 2HP panel width. Mounted vertically (rotated 90° in software), the active area is 32 px wide × 64 px tall in the panel window — enough for a parameter name and value on two lines with large-ish characters.

**Alternative:** 128×32 OLED (used in the RA4M1 design pivot doc) — wider than 2HP when mounted horizontally; would need vertical mount too. The 64×32 is a better physical fit.

### 10. Git workflow: feature branches + self-reviewed PRs

Same as RA4M1 project. CI gates PRs. Self-review catches real bugs.

### 11. User stories as the unit of work

See [`stories/README.md`](./stories/README.md) for format. Each story defines a verifiable outcome. Stories are satisfied by code and, for pure-logic modules, by tests first (TDD).

### 12. CHANGELOG.md updated on every commit

Keep a Changelog format. `## [Unreleased]` in flight; moves to a versioned heading on tag.

### 13. Versioning — SemVer for firmware, Rev N.M for hardware

**Firmware:** `arp/v0.1.0`, `arp/v0.2.0`, etc. First tag on completion of Story 005.

**Hardware:** `Rev 0.1` is first fabricated board. `Rev 1.0` is first public release.

---

## Bring-up sequence (first five PRs)

| PR | Story | Outcome |
|---|---|---|
| 1 | [001](./stories/001-repo-scaffolded.md) | Repo skeleton, licenses, CI, `.gitignore` |
| 2 | [002](./stories/002-xiao-blinks.md) | Firmware builds and blinks the XIAO RP2350 |
| 3 | [003](./stories/003-dac-and-quantiser.md) | MCP4725 emits calibration ramp; scale quantiser TDD'd on host |
| 4 | [004](./stories/004-voct-tracks-octaves.md) | Op-amp breadboard + calibration; V/Oct ±5¢ across C3–C7 |
| 5 | [005](./stories/005-arp-plays-up-pattern.md) | Minimal arpeggiator — **v0.1 makes music** |

After PR 5, everything else (encoder menu, OLED display, six scales, CV in, chaos, NeoPixel, calibration mode, KiCad) is additive.

---

## Deferred decisions

- **CV divider values:** 0–5V Eurorack → 0–3.3V RP2350 ADC. Exact resistor values to be confirmed on bench (Story 004 or a dedicated story).
- **Calibration storage:** hardcoded `GAIN` constant in v0.1. Move to flash/EEPROM post-MVP.
- **Encoder library:** `Encoder.h` vs `RotaryEncoder.h` — decide at Story 009.
- **OLED rotation strategy:** software rotation (Adafruit GFX `setRotation(1)`) or hardware OLED orientation strap — confirm with part in hand.
- **Chaos design:** deferred, same as RA4M1 project.
- **RNG choice:** `random()` fine for MVP; XORShift or `std::minstd_rand` for reproducible host tests post-MVP.
