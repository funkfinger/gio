# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and firmware in this repo follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Hardware uses `Rev N.M` notation, silkscreened on the PCB. See `docs/decisions.md` §13 for the full scheme.

**Convention:** this file is updated on every commit. Changes in-flight go under `## [Unreleased]`; on a firmware tag (`arp/vX.Y.Z`), those entries move under a new `## [arp/vX.Y.Z] — YYYY-MM-DD` heading.

Section keys: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`, `Docs`.

---

## [Unreleased]

### Added

- **Story 005 complete (v0.1.0 milestone — module makes music for the first time).** Bench-confirmed 2026-04-22.
- `firmware/arp/lib/arp/`: pure-C++ arpeggiator engine. `Arp::nextNote()` advances state and returns the MIDI note for the current step. Orders implemented: `Up`, `Down`, `UpDown` (palindrome, no endpoint repeat), `Order1324` (skip pattern, falls back to `Up` for arps with <4 notes). Adding a new order = adding one case in `indexForStep()`.
- `firmware/arp/lib/tempo/`: pure-C++ tempo helper. `bpmToStepMs(bpm, sub=1)` clamped to [30, 300] BPM with subdivision support; `potToBpm(pot)` linear in [0, 1] → [BPM_MIN, BPM_MAX].
- `firmware/arp/test/test_arp/` and `test/test_tempo/`: 21 new GoogleTest cases (11 arp + 10 tempo). Total host tests: 34, all green via `pio test -e native`.
- `firmware/arp/src/main.cpp`: replaced V/Oct calibration sequence with arp integration — non-blocking `millis()` loop, fixed 120 BPM, 4-note up-arpeggio over MIDI 48/52/55/60 (C3 E3 G3 C4), 50 % gate duty, `PIN_LED` as beat indicator. `GATE_INVERTED` constant abstracts NPN driver polarity.
- BC548 NPN common-emitter gate driver wired on breadboard: D6 → 1 kΩ → base; collector → 10 kΩ pullup → 5V (= J4 gate node); emitter → GND.
- `firmware/arp/lib/scales/` — pure-C++ scale quantiser library: `quantize(midiNote, Scale)` and `scaleFromPot(pot, current)` with ±2 % hysteresis. No Arduino deps.
- `firmware/arp/test/test_scales/test_main.cpp` — 13 GoogleTest cases: in-scale pass-through for all 6 scales, out-of-scale snapping with tie-break-downward, chromatic identity, octave invariance, scaleFromPot zone centres and hysteresis. All pass via `pio test -e native`.
- Story 004 complete: MCP6002 op-amp non-inverting amp breadboarded (gain network 2.2 kΩ + 470 Ω in series for R_f, 10 kΩ for R_g), bench-calibrated 2026-04-22. Final firmware `GAIN = 1.26`. V/Oct output within ±2 mV / ±2.4¢ across C3–C7 by multimeter; ≤±4¢ best-fit residual end-to-end through VCO + tuner. New file `docs/calibration.md` records procedure and raw data.
- `firmware/arp/src/main.cpp`: replaced PWM ramp with V/Oct calibration sequence — outputs C3..C7 (MIDI 48..96), 2 s each, with target voltages on USB serial. To be replaced by arp integration in Story 005.
- Story 003 complete: scales library + 13 host tests passing; PWM DAC ramp bench-verified (raw 36.6 kHz square, 1-pole sawtooth, 2-pole clean ramp). ADC noise check deferred to PCB stage. First entry in `docs/bench-log.md`.
- Story 002 complete: blinky flashed via UF2 (picotool), LED blinks at 1 Hz on bench. Flash: 56 KB / 2 MB.

### Changed

- `firmware/arp/src/main.cpp`: replaced blinky with PWM DAC ramp — 12-bit `analogWrite` on D2 (GP28), sweeping 0→4095 at ~10 Hz. `analogWriteFreq(36621)` sets 150 MHz / 4096 ≈ 36.6 kHz PWM carrier. Story 003 bench steps pending scope verification.
- `firmware/arp/platformio.ini`: switched `platform` from `earlephilhower/arduino-pico.git#4.4.0` (bare git URL — PlatformIO cannot find `platform.json` in this repo) to `maxgerhardt/platform-raspberrypi` + `platform_packages = framework-arduinopico @ …#4.4.0`. This is the correct PlatformIO wrapper for the earlephilhower core; the pinned arduino-pico version is unchanged.
- `docs/decisions.md` §11: corrected — arduino-pico has no per-pin `analogWriteFreq(pin, freq)` overload in any released version (only global form exists). Decision updated: use global `analogWriteFreq(freq)` once in `setup()`; for future multi-slice PWM use pico-sdk directly. Also updated platform spec to reflect the maxgerhardt wrapper approach.
- `firmware/arp/platformio.ini`: removed `paulstoffregen/Encoder` from `lib_deps` — it uses AVR-specific register macros (`DIRECT_PIN_READ`, `pin1_register`) that do not compile on RP2350. Will be replaced with an RP2350-compatible alternative in Story 007.
- `firmware/arp/src/main.cpp`: `LED_BUILTIN` → `PIN_LED` — the `seeed_xiao_rp2350` variant defines `PIN_LED (25u)` but does not alias `LED_BUILTIN`.

### Changed

- `firmware/arp/platformio.ini`: pinned `earlephilhower/arduino-pico` platform to `#4.4.0` instead of tracking `main`. Locks the per-pin `analogWriteFreq(pin, freq)` overload that the PWM DAC depends on, and keeps CI + local + bench builds reproducible until we deliberately bump. Bump protocol documented in `decisions.md` §11. Story 003 gains a toolchain sanity check (`analogWriteFreq(PIN_DAC_PWM, 36621)` must compile against the pinned platform).
- `docs/decisions.md`: renumbered the duplicate §9/§10 entries (OLED and Git workflow); §11 is now "Platform version pinned to a tagged release"; subsequent sections shift to §12–§16.
- Stale `MCP4725` references in `docs/decisions.md` (§10 bench list, bring-up table) and `docs/stories/005-arp-plays-up-pattern.md` updated to reflect the PWM DAC.
- DAC approach: replaced MCP4725 I2C DAC with RP2350 12-bit hardware PWM on D2 + 2-pole RC filter (2× 10 kΩ + 100 nF, f_c ≈ 160 Hz). Eliminates I2C bus contention between DAC and OLED, removes MCP4725 from BOM, simplifies firmware (no library needed). Ripple < 63 µV; settling time ~5 ms. Spec, decisions, and Stories 003–005 updated accordingly. D2 pin reassigned from CV In #2 to PWM V/Oct out.
- Input jacks: J1 moved from D7 (digital-only, GP1) to A3 (GP29, ADC-capable). Both jacks dual-purpose.
- Timing architecture: `millis()` state machine (non-blocking) chosen over `delay()` for gate/step timing. Allows encoder, OLED, pot, and external clock to be serviced every loop iteration. Hardware timer (Path B) deferred unless bench reveals audible jitter. Both J1 and J2 now on ADC pins with identical protection circuits (100 kΩ series + 68 kΩ to GND + BAT54 dual Schottky). Both jacks are dual-purpose: CLOCK mode (rising-edge trigger) or PITCH mode (V/Oct → quantise), selectable per jack in encoder menu. Maps 0–8V V/Oct → 0–3.24V ADC (42 counts/semitone). D7 (GP1) freed as spare digital GPIO.

### Added

- Repo scaffolded: `firmware/arp/`, `hardware/`, `docs/`, `docs/stories/` directory tree
- `LICENSE-firmware` (MIT), `LICENSE-hardware` (Apache 2.0), `LICENSE-docs` (CC-BY 4.0)
- Root `README.md` — project summary, status, toolchain, versioning, license map
- `CLAUDE.md` — build instructions and hardware summary for Claude Code sessions
- `.gitignore` covering PlatformIO, KiCad, editor, macOS metadata, build artefacts
- `.github/workflows/ci.yml` — host tests + firmware compile-check on push and PR
- `firmware/arp/platformio.ini` — earlephilhower arduino-pico platform, `seeed_xiao_rp2350` board, native test env
- `firmware/arp/src/main.cpp` — placeholder blinky (Story 002 content)
- `firmware/arp/README.md` — build, upload (UF2), test, library notes

### Docs

- `docs/generative-arp-module.md` — Rev 0.1 spec: 2HP, XIAO RP2350, MCP4725 DAC, 64×32 OLED, 1 pot, 1 encoder, 4 jacks
- `docs/decisions.md` — tooling, scope, workflow decisions; rationale for MCP4725, I2C sharing, port from RA4M1, library reuse
- `docs/stories/README.md` — story format and workflow
- `docs/stories/001` through `008` — backlog from repo scaffolding through encoder menu integration
