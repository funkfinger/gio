# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and firmware in this repo follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Hardware uses `Rev N.M` notation, silkscreened on the PCB. See `docs/decisions.md` §13 for the full scheme.

**Convention:** this file is updated on every commit. Changes in-flight go under `## [Unreleased]`; on a firmware tag (`arp/vX.Y.Z`), those entries move under a new `## [arp/vX.Y.Z] — YYYY-MM-DD` heading.

Section keys: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`, `Docs`.

---

## [Unreleased]

### Added

- **Story 007 complete.** OLED + PEC11 encoder bench bring-up — both work standalone (no arp integration yet). Bench used a 0.91" 128×32 SSD1306 in landscape; final design will use a 0.49" 64×32 in portrait — one-line swap in `lib/oled_ui/oled_ui.h`.
- `firmware/arp/lib/oled_ui/`: HAL wrapper around `Adafruit_SSD1306`. Three-constant config block (`OLED_WIDTH`, `OLED_HEIGHT`, `OLED_ROTATION`) for size/orientation. API: `begin()`, `clear()`, `showLabel()`, `showParameter(name, value)`, `showBeat(on)`, `show()`, `raw()` escape hatch.
- `firmware/arp/lib/encoder_input/`: HAL wrapper around `mathertel/RotaryEncoder` + a 50 ms-debounced click. Polling API: `begin(pinA, pinB, pinClick)`, `poll()`, `delta()`, `pressed()`.
- `firmware/arp/platformio.ini`: added `mathertel/RotaryEncoder` to `lib_deps`.
- `firmware/arp/src/main.cpp`: replaced arp loop with standalone OLED+encoder bring-up — boot splash "HELLO", live counter from encoder rotation, LED + serial on encoder click.
- `docs/decisions.md` §17: encoder library choice documented (`mathertel/RotaryEncoder`, polling, RP2350-compatible). Removed the corresponding "deferred decisions" bullet.
- **Story 006 complete.** Tempo pot on A0 (D0/GP26, B10K) drives live BPM control. `src/main.cpp` reads `analogRead(PIN_TEMPO)` at every step boundary and recomputes step/gate periods from `tempo::potToBpm()` + `tempo::bpmToStepMs(bpm, 4)`. Subdivision switched from quarter notes to 16th notes (125 ms/step at 120 BPM).
- New host test `PotToBpm.ConstantRatioPerEqualPotSlice` — verifies the exponential mapping has constant ratio between equally-spaced pot positions. Total host tests: 35.

### Changed

- `tempo::potToBpm()` mapping changed from linear to exponential: `BPM_MIN × (BPM_MAX/BPM_MIN)^pot`. Endpoints exact; ratio per equal pot slice is constant — gives a constant musical "feel" across the rotation.
- `tempo::BPM_MIN`: 30 → 40 (Story 006 spec) → 20 (bench-feel adjustment 2026-04-22 — 40 BPM 16ths still too fast at the slow end). Per-third ratio is now ≈2.47×.
- `docs/stories/006-tempo-pot.md`: AC updated to reflect 20..300 BPM range and constant-ratio framing (replaces "every 1/3 doubles BPM" wording).

---

## [arp/v0.1.0] — 2026-04-22

**Milestone: the module makes music.** Stories 001–005 complete. End-to-end signal chain bench-validated: scales/arp/tempo logic + 12-bit PWM DAC on D2 + 2-pole RC filter + MCP6002 op-amp scaling + BC548 NPN gate driver + LED beat indicator. Audible 4-note up-arpeggio at 120 BPM through a VCO confirmed on bench. 34 host tests passing.

### Added

- **Story 005:** `firmware/arp/lib/arp/` — pure-C++ arpeggiator engine. `Arp::nextNote()` advances state and returns the MIDI note for the current step. Orders implemented: `Up`, `Down`, `UpDown` (palindrome, no endpoint repeat), `Order1324` (skip pattern, falls back to `Up` for arps with <4 notes). Adding a new order = adding one case in `indexForStep()`.
- **Story 005:** `firmware/arp/lib/tempo/` — pure-C++ tempo helper. `bpmToStepMs(bpm, sub=1)` clamped to [30, 300] BPM with subdivision support; `potToBpm(pot)` linear in [0, 1] → [BPM_MIN, BPM_MAX].
- **Story 005:** 21 new GoogleTest cases (`test_arp` ×11, `test_tempo` ×10). Combined with `test_scales` (×13): 34 host tests, all green via `pio test -e native`.
- **Story 005:** `src/main.cpp` arp integration — non-blocking `millis()` loop, fixed 120 BPM, 4-note up-arpeggio over MIDI 48/52/55/60 (C3 E3 G3 C4), 50 % gate duty, `PIN_LED` as beat indicator. `GATE_INVERTED` constant abstracts NPN driver polarity.
- **Story 005:** BC548 NPN common-emitter gate driver wired on breadboard — D6 → 1 kΩ → base; collector → 10 kΩ pullup → 5V (= J4 gate node); emitter → GND.
- **Story 004:** MCP6002 op-amp non-inverting amp breadboarded (gain network 2.2 kΩ + 470 Ω in series for R_f, 10 kΩ for R_g), bench-calibrated 2026-04-22. Final firmware `GAIN = 1.26`. V/Oct output within ±2 mV / ±2.4¢ across C3–C7 by multimeter; ≤±4¢ best-fit residual end-to-end through VCO + tuner.
- **Story 003:** `firmware/arp/lib/scales/` — pure-C++ scale quantiser library: `quantize(midiNote, Scale)` and `scaleFromPot(pot, current)` with ±2 % hysteresis. No Arduino deps. 13 GoogleTest cases (in-scale pass-through, out-of-scale snapping with tie-break-downward, chromatic identity, octave invariance, scaleFromPot zone centres + hysteresis).
- **Story 003:** PWM DAC ramp bench-verified — raw 36.6 kHz square, 1-pole sawtooth, 2-pole clean ramp.
- **Story 002:** XIAO RP2350 blinky flashed via UF2 (picotool). Toolchain end-to-end verified.
- **Story 001:** Repo scaffolded — `firmware/arp/`, `hardware/`, `docs/`, `docs/stories/` directory tree; `LICENSE-firmware` (MIT), `LICENSE-hardware` (Apache 2.0), `LICENSE-docs` (CC-BY 4.0); root `README.md`; `CLAUDE.md`; `.gitignore`; `.github/workflows/ci.yml`; `firmware/arp/platformio.ini` and `firmware/arp/README.md`.
- New file `docs/calibration.md` — circuit, procedure, and raw data for the V/Oct stage.
- `docs/bench-log.md` started — entries for Stories 003, 004, 005.

### Changed

- **DAC architecture:** replaced MCP4725 I2C DAC with RP2350 12-bit hardware PWM on D2 + 2-pole RC filter (2× 10 kΩ + 100 nF, f_c ≈ 160 Hz). Eliminates I2C bus contention, removes MCP4725 from BOM, simplifies firmware (no library needed). Ripple < 63 µV; settling time ~5 ms. D2 pin reassigned from CV In #2 to PWM V/Oct out.
- **Input jacks:** J1 moved from D7 (digital-only, GP1) to A3 (GP29, ADC-capable). Both jacks dual-purpose: CLOCK mode (rising-edge) or PITCH mode (V/Oct → quantise), selectable per jack in encoder menu. Identical protection circuits (100 kΩ series + 68 kΩ to GND + BAT54 dual Schottky), 0–8V V/Oct → 0–3.24V ADC. D7 (GP1) freed as spare digital GPIO.
- **Timing architecture:** `millis()` state machine (non-blocking) chosen over `delay()` for gate/step timing. Allows encoder, OLED, pot, and external clock to be serviced every loop iteration. Hardware timer (Path B) deferred unless bench reveals audible jitter.
- `firmware/arp/platformio.ini`: `platform =` set to `maxgerhardt/platform-raspberrypi` with `platform_packages = framework-arduinopico @ …#4.4.0`. The bare `earlephilhower/arduino-pico` git URL cannot be used as `platform =` directly because PlatformIO requires a `platform.json` manifest at the package root.
- `firmware/arp/platformio.ini`: removed `paulstoffregen/Encoder` from `lib_deps` — uses AVR-specific register macros (`DIRECT_PIN_READ`, `pin1_register`) that do not compile on RP2350. Will be replaced with an RP2350-compatible alternative in Story 007.
- `firmware/arp/src/main.cpp`: `LED_BUILTIN` → `PIN_LED` — the `seeed_xiao_rp2350` variant defines `PIN_LED (25u)` but does not alias `LED_BUILTIN`.
- `docs/decisions.md` §11 corrected: arduino-pico has no per-pin `analogWriteFreq(pin, freq)` overload in any released version (only global form exists). Decision updated: use global `analogWriteFreq(freq)` once in `setup()`; for future multi-slice PWM use pico-sdk directly.
- `docs/decisions.md`: renumbered the duplicate §9/§10 entries (OLED and Git workflow); §11 is now "Platform version pinned to a tagged release"; subsequent sections shift to §12–§16.
- Stale `MCP4725` references removed from `docs/decisions.md` and `docs/stories/004` and `005`.

### Docs

- `docs/generative-arp-module.md` — Rev 0.1 spec: 2HP, XIAO RP2350, PWM DAC, 64×32 OLED, 1 pot, 1 encoder, 4 jacks.
- `docs/decisions.md` — tooling, scope, workflow decisions; rationale for PWM DAC, I2C sharing, port from RA4M1, library reuse, platform pinning, NPN gate driver.
- `docs/stories/README.md` — story format and workflow.
- `docs/stories/001` through `008` — backlog from repo scaffolding through encoder menu integration.
