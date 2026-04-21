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

### 3. PWM DAC instead of external I2C DAC (MCP4725)

The RP2350 has no onboard DAC. Rather than adding an external MCP4725 I2C DAC, V/Oct output uses the RP2350's 12-bit hardware PWM peripheral on D2 (GP28) with a 2-pole passive RC low-pass filter feeding the MCP6002 op-amp.

**PWM DAC parameters:**
- PWM frequency: 150 MHz / 4096 ≈ 36.6 kHz (12-bit wrap, no clock divider)
- Filter: 2× (10 kΩ + 100 nF), f_c ≈ 160 Hz, −40 dB/decade above cutoff
- Ripple at 36.6 kHz: ≈ 63 µV (< 0.1¢) — well below the 2 mV spec
- Settling time (5τ): ≈ 5 ms — fine at 50 ms/step (300 BPM, 16th notes)
- Firmware write: `analogWrite(PIN_DAC_PWM, count)` — no library needed

**Why not MCP4725:**
- MCP4725 shares I2C bus with the OLED → potential bus contention during OLED refresh (~5.8 ms for a full 64×32 frame)
- Extra IC, extra BOM cost, extra library dependency
- PWM + RC filter uses components already in the passives BOM (same 10 kΩ / 100 nF values as op-amp network)
- `analogWrite()` updates are faster than an I2C transaction (~15–20 µs)

**Risk to validate on bench (Story 003):** 36.6 kHz PWM switching noise on D2 may couple into adjacent ADC pins (A0 tempo pot, A1 CV in) on breadboard. Scope the ADC readings while PWM is active. If coupling is a problem on PCB, route PWM trace away from analog inputs and add ground pour between them.

### 4. I2C bus dedicated to SSD1306 OLED

Hardware I2C0 on D4/D5, used exclusively by the SSD1306 OLED (address 0x3C). No bus sharing — the PWM DAC approach eliminates the MCP4725 from the I2C bus entirely.

**Benefit:** OLED can refresh at will without blocking DAC updates. Full 64×32 frame refresh takes ~5.8 ms at 400 kHz — this no longer affects V/Oct timing at all.

### 5. MVP (v0.1) is deliberately minimal

First working firmware:
- Internal clock only (tempo pot drives it)
- Major scale only
- Up-arp, 4 notes (root, 3rd, 5th, octave)
- PWM on D2 → 2-pole RC filter → MCP6002 op-amp → V/Oct out, calibrated
- Gate out via NPN transistor stage
- OLED shows tempo BPM only
- Encoder click cycles through arp orders
- No chaos, no CV in processing, no calibration mode, no NeoPixel

**Why:** the one genuinely novel subsystem is RP2350 PWM → 2-pole RC filter → MCP6002 → Eurorack V/Oct. Prove that before stacking features.

### 6. Reuse logic libraries from xiao-ra4m1-arp verbatim

`lib/scales/`, `lib/arp/`, `lib/tempo/` are pure C++ with no Arduino dependencies. They port without modification. Their existing GoogleTest suites run on the RP2350 native env unchanged.

**Why:** they were TDD'd and bench-validated on the RA4M1 project. No reason to rewrite them.

### 7. KiCad deferred until breadboard signal chain validates

Same rule as RA4M1 project: don't open KiCad until DAC → MCP6002 → multimeter matches the calibration voltage table within ±2 mV.

**New risk for this project:** 36.6 kHz PWM switching on D2 may couple noise into adjacent ADC pins. Confirm on breadboard (Story 003) before committing to PCB traces.

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
