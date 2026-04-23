# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and firmware in this repo follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Hardware uses `Rev N.M` notation, silkscreened on the PCB. See `docs/decisions.md` §13 for the full scheme.

**Convention:** this file is updated on every commit. Changes in-flight go under `## [Unreleased]`; on a firmware tag (`arp/vX.Y.Z`), those entries move under a new `## [arp/vX.Y.Z] — YYYY-MM-DD` heading.

Section keys: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`, `Docs`.

---

## [Unreleased]

### Added

- `firmware/arp/lib/trigger_in/` — pure-C++ firmware Schmitt trigger (`trigger_in::Schmitt`) for rising-edge detection on a polled voltage signal. Defaults to Eurorack-standard thresholds (high = +1.5 V, low = +0.5 V, 1 V hysteresis). Constructor accepts custom thresholds; initial state is "not armed" so an idle-high jack at startup doesn't fire spuriously. Story 016 prep — wires straight into the buffered ADC reads from Story 015 when bench-built.
- 15 new GoogleTest cases in `test_trigger_in/` covering: initial-state safety, rising-edge fire-once, square-wave repeat, exact-threshold behaviour, hysteresis (ripple rejection + must-drop-below-low to re-arm), slow-ramp single-fire, reset, custom thresholds. **Total host tests: 45 → 60, all green.**

### Changed

- **SPI pin map confirmed against the actual arduino-pico variant header** for `seeed_xiao_rp2350`. Corrects mistakes in the earlier spec-doc draft. Final mapping: SCK=D8/GP2, MISO=D9/GP4, MOSI=D10/GP3, default SS=D3/GP5. CS_DAC=D3 (default SS), CS_ADC=D6/GP0. Encoder migrates from D8/D9/D10 to A1/A2/D7 to free SPI0 default pins.
- **Library decision for SPI peripherals locked: Rob Tillaart's `DAC8552` and `MCP_ADC`** — both Arduino-framework, MIT-licensed, well-maintained. User has bench-validated the DAC8552 library previously in `~/Git/Electronics/eurorack/dac8852-test/`. This means we don't roll our own SPI frame packers; instead `lib/outputs/` and `lib/inputs/` HAL wrappers add calibration math on top of the proven libraries. Lib_deps staged in `platformio.ini` (commented; uncomment when bench-ready).
- Story 012 updated with confirmed pin map, encoder migration table, library choice, and bench acceptance criteria. Status: prep complete, awaiting parts.

- **`docs/generative-arp-module.md` rewritten end-to-end** to reflect the platform-module pivot and the SPI signal-chain architecture. Net changes from prior revision:
  - Reframed as platform module hosting multiple apps; arp is now one app of several
  - PWM-DAC + MCP6002 + native ADC chain replaced by **DAC8552 + MCP3208 + TL072s on ±12V**
  - All 4 jacks documented as symmetric / dual-purpose (CV ↔ trigger/gate)
  - **Mutable Instruments–style protection** topology spec'd on every input and output (100k series + BAT43 clamps + op-amp buffer)
  - Pin-assignment table updated for SPI bus; encoder reassigned from D8/D9/D10 to A1/A2/D3 to free SPI0 default pin set
  - Generic panel labels (`IN 1`/`IN 2`/`OUT 1`/`OUT 2`/`KNOB`)
  - NeoPixel light-pipe through panel PCB documented as a Rev 0.1 feature
  - Firmware section grew an "Apps + Platform + HAL + Drivers" four-layer architecture diagram and `App` interface definition
  - BOM rebuilt against actual inventory locations
  - Removed all stale MCP4725 references and the GP29-on-XIAO-RP2350 myth
- `docs/decisions.md` — added §19 (platform-module reframing), §20 (SPI bus replaces PWM-DAC + native ADC), §21 (TL072 + ±12V replaces MCP6002 + 5V), §22 (Mutable-style protection on every jack), §23 (generic jack labels). Reorganised "Deferred decisions" to separate **resolved** items (CV divider, ext-clock divider feel, gio-as-clock-source) from **still-open** items (calibration storage, app-private settings, second pot for Rev 0.2, NeoPixel light-pipe style, DAC VREF source, trigger output edge speed). Added a note to §18 (RP2350-E9) clarifying that the platform pivot moves CV/trigger jacks behind op-amp buffers, so the rule's practical impact on Rev 0.1 is small.
- `docs/stories/011-power-rails.md` reduced from "build a power supply" to "verify the Hivemind Protomato breadboarding kit's rails" — the bench platform handles ±12V/+5V/GND with reverse-protection. Original LM7805 + 1N5818 design preserved as reference for the Rev 0.1 PCB.

### Docs

- **Platform-module pivot scoped.** Stories 011–018 added — full bench-rebuild roadmap moving the design from "single-purpose arpeggiator on native ADC + PWM-DAC" to "platform module with shared SPI bus (DAC8552 + MCP3208) + MI-style protection + TL072 buffers + app-loader firmware skeleton."
- **Inventory check complete** (against `~/Git/binkey-data/parts/`). On hand and sufficient for bench: TL072CP (×25), DAC8552 (×4 — *upgrade* from spec'd MCP4822), PEC11L encoders (×5), PJ-3001F Thonkiconn jacks (30+), 100 kΩ linear pots (20+), BAT43 Schottky (50+), LM7805 + LM7912, all standard SMT R/C values. **Ordered:** MCP3208 SPI ADC (×2), Eurorack 2×5 shrouded headers (×5), 0.49" 64×32 SSD1306 OLED (delivery tomorrow). DAC8552 already mounted on a hand-soldered protoboard, ready for Story 012 SPI bring-up.

---

## [arp/v0.3.0] — 2026-04-23

**External clock-in lands; bench tooling matures.** Story 010 complete. gio can now be patched into an external Eurorack clock and follow it; the tempo pot becomes a clock multiplier (×1 / ×2 / ×4) when external is active. A standalone bench clock source (`firmware/clock-mod2/`) was built on a HAGIWO MOD2 board to drive testing. Two notable hardware bugs encountered and documented: RP2350-E9 silicon errata (broken internal pulldowns) and a MOD2 PCB defect (JP2 doesn't route to C18). 45 host tests still green.

### Added

- **Story 010:** External clock input on J1 (D3 / GP5). `digitalRead`-based edge detection with 10 ms software debounce (rejects bounce on slow edges). Auto-switches to external mode within 2 s of a detected rising edge; falls back to internal tempo pot on timeout. OLED top-right tag shows `INT` / `EXT`.
- **Story 010:** Tempo pot acts as a **clock multiplier** when external is active — bottom 1/3 = ×1 (one step per pulse), middle = ×2 (insert one interpolated step at half-period), top = ×4 (three interpolated steps at quarter-period spacing). Default ×2 chosen as the most musically useful at typical Eurorack clock rates. Interpolation uses the previously-measured period — one-pulse-late approximation; see `decisions.md` deferred section for the runaway-on-slowdown caveat.
- `firmware/clock-mod2/` — bench clock source for a HAGIWO MOD2 board (XIAO RP2350 socketed). POT1 sets pulse rate exponentially in **2..20 Hz** (renamed from BPM since "BPM" is the wrong unit for a raw clock). GP1 → MOD2 op-amp gain stage → 0–10 V gates at J6. GP5 drives the panel LED in sync.
- `docs/stories/010-clock-input.md`, detailed `docs/bench-log.md` entry covering the full Story 010 + side-quest bench journey.

### Changed

- `firmware/arp/src/main.cpp`: `PIN_J1 = D3 / GP5` (digital, not analog). Spec called for `A3 / GP29` but **GP29 is not exposed on the XIAO RP2350** (variant only breaks out GP0–7 + GP9–12, GP16–17, GP20–21). All ADC-capable pins (GP26/27/28) are already used, so J1 is digital-only on this build; pitch mode on J1 is deferred until an ADC channel is freed.
- `firmware/arp/src/main.cpp`: `pinMode(PIN_J1, INPUT_PULLDOWN)` — kept as belt-and-braces, but the internal pulldown is silently broken on RP2350 (see RP2350-E9 below). The external 10 kΩ pulldown is what actually pulls the pin low.
- Bench input divider on J1: spec was 100 kΩ + 68 kΩ; **bench-built as 10 kΩ + 10 kΩ** to overpower the RP2350-E9 latched-pad voltage. Same 1:2 ratio so firmware needs no change.
- `firmware/clock-mod2/`: switched from `tempo::potToBpm` (20..300 BPM, quarter notes) to direct exponential `potToHz` (2..20 Hz). "BPM" is the wrong unit for a raw clock source; raised the floor and ceiling to better match Eurorack clock rates.

### Docs

- **`docs/decisions.md` §18 — RP2350-E9 silicon errata.** Internal `INPUT_PULLDOWN` latches the pad at ~2.2 V instead of pulling to GND. Pi Foundation workaround: external pulldown ≤ 8.2 kΩ. Cost ~90 min of bench debugging during Story 010 before user spotted the errata. **Rule for Rev 0.1 PCB:** every GPIO that needs a pulldown gets an external resistor ≤ 8.2 kΩ.
- `docs/decisions.md` "Deferred decisions" — added: (a) the MOD2 JP2 lesson (verify every solder-jumper net survives schematic→PCB transfer); (b) "gio as a clock source" feature placeholder; (c) external-clock multiplier feel notes (×1/×2/×4 is fine; replace with ÷N would be a regression at Eurorack rates; multiplication is the genuinely useful direction).
- **BOOTSEL workflow update** — discovered today that `picotool` can soft-reset the running firmware into BOOTSEL via USB CDC, so `pio run --target upload` works without any button-press for normal re-flashes. Manual hold-BOOT-while-plug only needed for fresh chips or unresponsive firmware. Updated `CLAUDE.md`, `firmware/arp/README.md`, and `firmware/clock-mod2/README.md`.
- `firmware/clock-mod2/README.md`: documents the **JP2/C18 board defect on MOD2 Rev A** — soldered solder-bridge has no electrical effect because the JP2 traces don't reach C18. Workaround: solder a wire across C18 directly.
- New `docs/stories/010-clock-input.md`.

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
