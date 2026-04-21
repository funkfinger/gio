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

### 8. Dual-purpose input jacks — same hardware, firmware-selectable mode

J1 (A3/GP29) and J2 (A1/GP27) use identical protection and divider circuits: 100 kΩ series + 68 kΩ to GND + BAT54 dual Schottky. This maps 0–8V V/Oct → 0–3.24V at ADC and provides safe clamping for negative or over-voltage signals.

Both jacks can operate as either a **trigger/clock input** (`digitalRead()`, rising-edge detection above ~1.5V) or a **pitch CV input** (`analogRead()` → inverse divider → semitone → `quantize()`). Mode is selected per jack in the encoder menu (CLOCK / PITCH).

**Why:** the requirement was one trigger in and one V/Oct in. Rather than dedicate each jack in hardware — which would require different circuits and fix the panel semantics forever — both jacks get identical hardware and the split is done in firmware. Patcher can reassign either jack. D7 (GP1, previously the clock-only digital input) is freed.

**Why 100 kΩ + 68 kΩ:** the 100 kΩ series resistor limits clamp diode current to < 70 µA even at 10V. The 68 kΩ lower leg maps 0–8V → 0–3.24V, covering 8 octaves of V/Oct with 42 ADC counts/semitone — sufficient for scale quantisation. A lower series resistor (e.g., 3.3 kΩ as in the RA4M1 design) would give faster rise time but the 100 kΩ + 100 pF cable capacitance RC = 10 µs — fine at 300 BPM (50 ms/step).

### 9. Gate / clock timing: `millis()` state machine (non-blocking)

The main loop uses `millis()` timestamps instead of `delay()` for all gate and step timing. Each iteration checks whether enough time has passed to transition state (gate on → gate off → next step), then reads pots, polls the encoder, updates the OLED if needed, and checks for external clock edges — without blocking.

```cpp
// sketch of the pattern
if (millis() - stepStart >= stepMs) {
    stepStart = millis();
    // advance arp, write DAC, gate on
}
if (millis() - stepStart >= gateOnMs) {
    // gate off (if not already)
}
```

**Why not `delay()`:** the RA4M1 firmware blocked for up to 125 ms per half-step. With an encoder, OLED, two ADC reads, and external clock detection in the loop, a blocked loop drops encoder turns, misses clock edges, and delays pot reads by up to 250 ms.

**Why not hardware timer interrupt or dual-core (yet):** `millis()` is the simplest path. The worst-case loop jitter is dominated by I2C OLED refresh (~5.8 ms for a full 64×32 frame), which only happens on parameter change — not every loop. At 300 BPM (50 ms/step), 5.8 ms jitter is 12% of a step: audible in theory, likely inaudible in practice for a generative arp. **Revisit with a hardware timer (Path B) if jitter is audible on the bench.**

### 10. Test strategy: host TDD for pure logic, bench for HAL

- **Host tests (`pio test -e native`):** `scales`, `arp`, `tempo` — no Arduino deps, pure logic
- **Bench-verified:** `pwm_dac` (PWM + 2-pole RC filter → MCP6002), `gate_out`, `oled_ui`, `encoder_input`, `clock_in`

Bench workflow: oscilloscope (Rigol DS1054Z) for timing, multimeter for V/Oct voltages, VCO + tuner for pitch tracking.

### 11. Platform version pinned to a tagged release

`platformio.ini` uses `maxgerhardt/platform-raspberrypi` (the PlatformIO wrapper) with the arduino-pico core pinned via `platform_packages`:

```ini
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
platform_packages = framework-arduinopico @ https://github.com/earlephilhower/arduino-pico.git#4.4.0
```

The bare `earlephilhower/arduino-pico` git URL cannot be used directly as `platform =` because PlatformIO expects a `platform.json` manifest that the core repo does not supply.

**Why pin:**
- CI and local builds stay reproducible across time. A platform bump should be a deliberate act, not a silent surprise on the next `pio run`.
- Bench work is expensive. A platform change can shift PWM slice assignments, ADC averaging defaults, or I2C timing — any of which invalidate Story 003/004 calibration. If we're going to revalidate the bench, we do it because we chose to, not because GitHub's `main` moved.

**PWM frequency note (Story 003 finding):** arduino-pico only exposes the global `analogWriteFreq(uint32_t freq)` — there is no per-pin overload in any released version. In practice this is acceptable for this design: D2 (GP28) is the only `analogWrite` pin; gate output uses digital GPIO; NeoPixel uses PIO (not PWM slices). Call `analogWriteFreq(36621)` once in `setup()` before `analogWrite()`. If a future story needs a second PWM slice at a different frequency, use the pico-sdk `pwm_set_clkdiv()` / `pwm_set_wrap()` APIs directly.

**Bump protocol:** update the pin → run host tests → rebuild firmware → re-run Story 003 PWM ramp bench checks → record in `bench-log.md`.

### 12. 64×32 OLED (0.49"), mounted vertically

The 64×32 SSD1306 at 0.49" fits within the 2HP panel width. Mounted vertically (rotated 90° in software), the active area is 32 px wide × 64 px tall in the panel window — enough for a parameter name and value on two lines with large-ish characters.

**Alternative:** 128×32 OLED (used in the RA4M1 design pivot doc) — wider than 2HP when mounted horizontally; would need vertical mount too. The 64×32 is a better physical fit.

### 13. Git workflow: feature branches + self-reviewed PRs

Same as RA4M1 project. CI gates PRs. Self-review catches real bugs.

### 14. User stories as the unit of work

See [`stories/README.md`](./stories/README.md) for format. Each story defines a verifiable outcome. Stories are satisfied by code and, for pure-logic modules, by tests first (TDD).

### 15. CHANGELOG.md updated on every commit

Keep a Changelog format. `## [Unreleased]` in flight; moves to a versioned heading on tag.

### 16. Versioning — SemVer for firmware, Rev N.M for hardware

**Firmware:** `arp/v0.1.0`, `arp/v0.2.0`, etc. First tag on completion of Story 005.

**Hardware:** `Rev 0.1` is first fabricated board. `Rev 1.0` is first public release.

---

## Bring-up sequence (first five PRs)

| PR | Story | Outcome |
|---|---|---|
| 1 | [001](./stories/001-repo-scaffolded.md) | Repo skeleton, licenses, CI, `.gitignore` |
| 2 | [002](./stories/002-xiao-blinks.md) | Firmware builds and blinks the XIAO RP2350 |
| 3 | [003](./stories/003-dac-and-quantiser.md) | PWM DAC + 2-pole RC filter emits clean ramp; scale quantiser TDD'd on host |
| 4 | [004](./stories/004-voct-tracks-octaves.md) | Op-amp breadboard + calibration; V/Oct ±5¢ across C3–C7 |
| 5 | [005](./stories/005-arp-plays-up-pattern.md) | Minimal arpeggiator — **v0.1 makes music** |

After PR 5, everything else (encoder menu, OLED display, six scales, CV in, chaos, NeoPixel, calibration mode, KiCad) is additive.

---

## Deferred decisions

- **CV divider bench confirmation:** 100 kΩ / 68 kΩ divider maps 8V → 3.24V (Story 004 bench work). Verify no ADC saturation at max Eurorack V/Oct; confirm 42 counts/semitone is sufficient for clean quantisation.
- **Calibration storage:** hardcoded `GAIN` constant in v0.1. Move to flash/EEPROM post-MVP.
- **Encoder library:** `Encoder.h` vs `RotaryEncoder.h` — decide at Story 009.
- **OLED rotation strategy:** software rotation (Adafruit GFX `setRotation(1)`) or hardware OLED orientation strap — confirm with part in hand.
- **Chaos design:** deferred, same as RA4M1 project.
- **RNG choice:** `random()` fine for MVP; XORShift or `std::minstd_rand` for reproducible host tests post-MVP.
