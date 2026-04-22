# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and firmware in this repo follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Hardware uses `Rev N.M` notation, silkscreened on the PCB. See `docs/decisions.md` §13 for the full scheme.

**Convention:** this file is updated on every commit. Changes in-flight go under `## [Unreleased]`; on a firmware tag (`arp/vX.Y.Z`), those entries move under a new `## [arp/vX.Y.Z] — YYYY-MM-DD` heading.

Section keys: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`, `Docs`.

---

## [Unreleased]

*(empty — Story 010 starts here)*

---

## [arp/v0.2.0] — 2026-04-22

**Module is fully hands-on playable.** Stories 006–009 complete. Live tempo pot, OLED + encoder hardware alive, full encoder menu (scale + order + root with long-press reset), audible scale quantisation per step, and V/oct CV input on J2 that transposes the arp. 45 host tests passing.

### Added

- **Story 009:** `firmware/arp/lib/cv/` — pure-C++ `cvVoltsToTranspose(volts, scale)` (semitone round → scale-snap within octave, input clamped to [0, 8 V]). 10 new host tests.
- **Story 009:** CV input on J2 transposes the arp (V/oct, α-summed with encoder ROOT). ½-semitone hysteresis wrapper kills ADC-noise flutter at snap boundaries. Final played notes clamped to MIDI [24, 96].
- **Story 009:** MCP6002 op-amp B repurposed as unity-gain buffer on the CV input path. Divider: 100 kΩ series + (47 kΩ + 22 kΩ) to GND = 69 kΩ (≈ spec 68 kΩ). Maps 0..8 V at J2 to 0..3.24 V at A1/GP27.
- **Story 009:** New menu param `ROOT` (12 pitch classes, pinned to MIDI octave 3, default C). Encoder menu cycle: SCALE → ORDER → ROOT → SCALE. NeoPixel **magenta** for ROOT active. OLED `ROOT` line shows the *effective* root (encoder + CV, mod 12) with trailing `*` when CV is contributing.
- **Story 009:** Chord stored as intervals `{0, 4, 7, 12}` instead of absolute MIDI notes. Runtime `root_midi` computed per step.
- **Story 008:** Encoder menu for Scale and Order. Click cycles parameter (Scale → Order → Scale), rotate changes the active value, **long-press (≥500 ms)** resets the arp at step 0 and flashes "RESET" on the OLED. Long-press fires mid-hold; release is silent (no spurious short-click on the same gesture).
- **Story 008:** Scale changes are now *audible* — each arp note passes through `quantize()` with the active scale before reaching the DAC.
- **Story 008:** Onboard RGB LED as active-param indicator — green for Scale, blue for Order. Pin discovery: data on **GPIO22**, power-enable on **GPIO23** (per Seeed wiki; not exposed by the arduino-pico variant header). LED is **RGBW** (4 bytes/pixel) → `NEO_GRBW + NEO_KHZ800`. 20 ms settle delay between power-enable and first `show()`.
- **Story 007:** `firmware/arp/lib/oled_ui/` — HAL wrapper around `Adafruit_SSD1306`. Three-constant config block (`OLED_WIDTH`, `OLED_HEIGHT`, `OLED_ROTATION`) for one-line swap between bench (0.91" 128×32 landscape) and final (0.49" 64×32 portrait).
- **Story 007:** `firmware/arp/lib/encoder_input/` — HAL wrapper around `mathertel/RotaryEncoder` + 50 ms-debounced click + `pressedLong()` (≥500 ms threshold). `LatchMode = FOUR3` chosen after bench iteration.
- **Story 006:** Tempo pot on A0 (D0/GP26, B10K) drives live BPM control. `src/main.cpp` reads `analogRead(PIN_TEMPO)` at every step boundary and recomputes step/gate periods from `tempo::potToBpm()` + `tempo::bpmToStepMs(bpm, 4)`. Subdivision switched from quarter notes to 16th notes (125 ms/step at 120 BPM).
- Host test coverage grew from 34 → 45: new `test_tempo::PotToBpm.ConstantRatioPerEqualPotSlice`, 11 `test_arp` cases kept current after `Order1324 → Skip` rename, 10 new `test_cv` cases.
- `docs/decisions.md` §17: encoder library choice documented (`mathertel/RotaryEncoder`, polling, RP2350-compatible). Deferred-decisions bullet removed.
- `firmware/arp/README.md`: documented the BOOTSEL quirk (double-tap BOOT unreliable on our XIAO RP2350; use hold-BOOT-while-plugging).
- `docs/stories/009-cv-input-transpose.md` added. `docs/bench-log.md` entries for Stories 006, 007, 008, 009 (including the 100 Ω vs 100 kΩ colour-band bug and its diagnosis from serial output).

### Changed

- `lib/arp/ArpOrder::Order1324` → `ArpOrder::Skip` (matches menu UI labeling). Test cases renamed. No behaviour change.
- `lib/tempo/potToBpm()` mapping: linear → exponential (`BPM_MIN × (BPM_MAX/BPM_MIN)^pot`). Endpoints exact; ratio per equal pot slice is constant → uniform musical feel across rotation.
- `lib/tempo/BPM_MIN`: 30 → 40 (Story 006 spec) → **20** (bench-feel adjustment — 40 BPM 16ths still felt too fast at the slow end). Per-third ratio ≈ 2.47×.
- `firmware/arp/src/main.cpp` rewritten three times across Stories 006–009: tempo-pot integration → standalone OLED/encoder bring-up → full arp+menu integration → CV-transpose integration. Now a non-blocking `millis()` loop with all subsystems live.
- `lib/encoder_input/` `RotaryEncoder::LatchMode` bench-tuned to `FOUR3` (TWO03 → FOUR0 → FOUR3 during Story 008 bring-up; header documents troubleshooting hints).
- `docs/stories/006-tempo-pot.md`: AC text updated to reflect the 20..300 BPM range and constant-ratio framing (replaces "every 1/3 doubles BPM" language).
- `firmware/arp/README.md`: library notes corrected (was: Stoffregen Encoder, default OLED size; now: mathertel/RotaryEncoder, `oled_ui` config block, NeoPixel pinout).
- `firmware/arp/platformio.ini`: added `mathertel/RotaryEncoder` to `lib_deps`; removed `paulstoffregen/Encoder` (AVR-only).

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
