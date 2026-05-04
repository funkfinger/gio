# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and firmware in this repo follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Hardware uses `Rev N.M` notation, silkscreened on the PCB. See `docs/decisions.md` §13 for the full scheme.

**Convention:** this file is updated on every commit. Changes in-flight go under `## [Unreleased]`; on a firmware tag (`arp/vX.Y.Z`), those entries move under a new `## [arp/vX.Y.Z] — YYYY-MM-DD` heading.

Section keys: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`, `Docs`.

---

## [Unreleased]

### Added

- **`hardware/gio-opamp/` — first KiCad project for Rev 0.1 PCB**, focused on the input scaling stage. Schematic implements the full §7 design that was bench-validated across the 2026-04-29 → 2026-05-03 sessions: per-channel offset divider (R5/R8 + R6/R7 = 22 k / 14.7 k → ~1.66 V bias), 22 k feedback (R1/R2), 22 k input resistor (R10/R11), 100 k series-protection resistor before the clamp node (R9/R12), BAT54S series-pair Schottky clamps to ±12 V, and 1 k output isolation (R3/R4) feeding adc_ch1 / adc_ch2. Power decoupling: bulk 100 nF on each rail at the chip (C1/C2), local 100 nF directly across U1 V+/V− (C5), and 100 nF filter caps on each bias-divider node (C3/C4). Schematic only — PCB layout deferred until all subsystems are captured.
- **Input-stage schematic refinements** (2026-05-03 review-driven updates): `input tip a/b` net renamed to `clamp_a/clamp_b` (more accurate — it's the protection node *after* R_series); local 100 nF decoupling across U1 V+/V−; per-channel 100 nF filter caps on each bias-divider node; Schottky clamps switched to BAT54S 3-pin series-pair symbol (one part replaces two BAT43s per channel for the production PCB).
- **`tools/bench-read.py` — reusable Python bench script** for reading the smoke-test firmware's tab-separated serial stream and summarizing channel min/max/range/last per labeled column. Auto-discovers `/dev/cu.usbmodem*`, parses the `# t_ms ...` header line, and falls back to the known smoke-test column names if the header was printed before the script connected. Replaces the inline `python3 -c "..."` blocks scattered through the bench transcripts. Usage: `python3 tools/bench-read.py [--seconds N] [--port PATH] [--rows N]`.

### Changed

- **`.gitignore` extended for KiCad cruft** — `~*.lck` (KiCad project/schematic lock files when the editor is open) and `.history/` (KiCad's local-history snapshot directory) added so they don't pollute commits.
- **MCP3208 channel layout settled** (decisions.md §26 updated). Convention: **CH0 = primary pot** (tempo today), **CH1 = J1**, **CH2 = J2**, **CH3–CH7 = expansion / I/O daughterboard**. Internal controls at low channels; jacks contiguous after; expansion at high channels — clean rule for any future pot/jack/button. Wiring updates: pot wiper to MCP3208 pin 1 (was pin 2); J1 op-amp output to pin 2 (was pin 1); J2 op-amp output (when wired) to pin 3. Firmware constants renamed `ADC_CH_TEMPO` → `ADC_CH_POT`, `ADC_CH_CV_IN` → `ADC_CH_CV_IN_1`, added `ADC_CH_CV_IN_2`. Calibration now applies to CH1 (J1) instead of CH0 (which is now the pot — no calibration needed; raw 0..VREF). Smoke test serial header is now `t_ms  dac_a_v  dac_b_v  pot_v  pot_count  j1_v  j1_count  j2_v  j2_count  enc_count  click  long`. `bench-wiring.md` §3, §7, §8, §9 updated. Production firmware builds at 2.2 % RAM / 4.6 % flash; smoke test builds; all 66 host tests pass.
- **`EncoderInput` switched from polled-only to interrupt-driven decoding.** Both quadrature pins (A and B) now have CHANGE ISRs attached that call `RotaryEncoder::tick()` the moment a transition occurs, so the state machine never misses a detent regardless of how busy the main loop is (OLED I²C bursts, SPI transactions, serial prints can all stall the loop for tens of ms with no impact on encoder accuracy). `poll()` still calls `tick()` as belt-and-suspenders. Caught at the bench when fast rotations dropped detents in the smoke test (the loop's ~50 Hz polling rate plus ~5 ms of OLED I²C transmission per refresh was eating transitions). After the change, an 8-second fast-rotation test caught all 21 detents (22 unique counts seen). No firmware-side calling-code changes — `g_encoder.poll()` still works identically.
- **Smoke-test firmware initializes and drives the OLED.** Boot probes for an SSD1306 at I²C `0x3C` and prints "OLED: detected" or "NOT detected" to serial; if detected, splash "gio" briefly then refresh "ENC = N" at 10 Hz reflecting the live encoder count. Lets the bench rig validate the OLED end-to-end without flashing the production firmware.
- **Encoder A/B swapped in firmware** (`PIN_ENC_A = D2`, `PIN_ENC_B = D1`) in both `src/main.cpp` and `src/main_smoketest.cpp`. The bench encoder reads CW as a negative delta when wired with the original A→D1, B→D2 assignment; flipping the firmware constants is cleaner than fighting the breadboard. Convention is now: encoder pin A → XIAO D2, encoder pin B → XIAO D1, common → GND. Bench-validated: CW = positive count, CCW = negative count.
- **Smoke-test firmware streams encoder columns** (`enc_count  click  long`) alongside the existing DAC/ADC/tempo columns. Output line is now `t_ms  dac_a_v  dac_b_v  adc_v  adc_count  tempo_v  tempo_count  enc_count  click  long`. Lets the bench rig validate the encoder + click + long-press paths without re-flashing a one-shot test. Internal pull-ups (`INPUT_PULLUP`) handle idle state — no external resistors needed.
- **Tempo pot relocated from XIAO D0 onto MCP3208 CH1.** All analog inputs now share the precision REF3040 reference (uniform calibration story, no mixing of XIAO native ADC with external ADC). Frees D0 for J1 clock/gate digital edge detection. Hardware change: pot CW now ties to the **VREF rail** instead of +3.3 V; wiper goes to MCP3208 pin 2; pot CCW unchanged at GND. Firmware change: `PIN_TEMPO` constant removed from `src/main.cpp`; 3× `analogRead(PIN_TEMPO)` calls replaced with `inputs::readRaw(ADC_CH_TEMPO)` where `ADC_CH_TEMPO = 1`. J2 CV input (when wired) moves from MCP3208 CH1 → CH2. Updated `bench-wiring.md` §3, §8, §9 and `decisions.md` §26 pin-budget table. Build still 2.2 % RAM / 4.6 % flash; all 66 host tests pass.
- **Production firmware migrated to the SPI HAL.** `src/main.cpp` no longer drives the PWM-DAC + analog-CV + digital-gate paths; instead it uses `outputs::write()` (V/Oct on jack J3 via DAC ch A), `outputs::gate()` (gate on jack J4 via DAC ch B), and `inputs::readVolts()` (CV transpose in on jack J1). Pin migration: encoder moved D8/D9/D10 → D1/D2/D7 to free SPI pins; CS_DAC=D3, CS_ADC=D6, SPI bus on D8/D9/D10 (per `bench-wiring.md` §3). Production firmware builds at 2.2 % RAM / 4.6 % flash; smoke-test env still builds; all 66 host tests pass.
- **Hardcoded calibration constants from the 2026-04-30 bench session** baked into `setup()` via `outputs::setCalibration()` and `inputs::setCalibration()` calls — musical-grade approximations for the breadboard prototype, slated for replacement by Story 022 (calibration app) once Rev 0.1 PCB is in hand.
- **External clock detection (Story 010) is dormant** — `pollExternalClockEdge()` always returns false until input ch B (jack J2) scaling stage is wired and a Schmitt trigger is integrated. Arp runs from internal tempo only in the meantime. The clock-mode-handling code paths are preserved so the Schmitt integration is a localized change.

### Added

- **Smoke-test firmware streams a `tempo_v` / `tempo_count` column** for MCP3208 CH1 alongside the existing CH0 columns. Lets the user verify any pot/CV wired to CH1 (currently the tempo pot) without re-flashing a one-shot test firmware. Output line is now `t_ms  dac_a_v  dac_b_v  adc_v  adc_count  tempo_v  tempo_count`.
- **Tempo pot bench-validated end-to-end on the new MCP3208 CH1 wiring.** Endpoints clean (CCW: count=0 / 0.000 V, CW: count=4090 / 4.09 V — within 5 LSB of full-scale), smooth monotonic sweep across the full rotation, no jitter or dropouts. `bench-log.md` 2026-05-02 entry covers the move; this commit confirms the result.

- **`docs/decisions.md` §27 — Calibration architecture, designed now, built later.** Captures the full design of the calibration story after a focused grilling session: form factor (app within Story 018's app switcher), trigger (auto-on-missing-data + manual via app switcher), loopback method (Eurorack patch cable J3 → J1 / J4 → J2), reference (REF3040 trusted as ±0.2 % absolute, ~5 cents at V/Oct), math (fused output + input pair per channel, 5-point linear fit, REF3040-anchored), one-time-only assumption (hardware doesn't drift), and the EEPROM-emulated-in-flash storage format with magic / version / last-app / calibration block / CRC32. Calibration data survives normal `picotool` re-flash (lives in last 4 KB sector). Sequenced after Rev 0.1 PCB manufacture so we calibrate against the real board, not the breadboard.
- **`docs/stories/022-calibration-app.md`** — deferred story implementing §27. Acceptance criteria cover app registration, auto-trigger on missing data, full UI flow (welcome → patch prompt → DAC sweep → linear fit → sanity check → confirm + write), EEPROM I/O, bench verification (V/Oct accuracy ±5 cents per octave against multimeter), and host tests (linear fit + CRC + storage round-trip). Hard dependencies: Story 018 + Rev 0.1 PCB. Until Story 022 ships, production firmware uses hardcoded calibration constants from breadboard measurements (musically usable, not lab-grade).
- **Story 018 updated to reserve the §27 EEPROM layout** — Story 018 only writes the magic / version / last-app fields, but the calibration block + CRC32 region is laid out from the start so Story 022 doesn't need to migrate existing storage when it lands.

### Changed

- **`docs/decisions.md` "Still open" list pruned:** "Calibration storage", "Calibration-utility app", and "App-private settings persistence" all now point at §27 / Story 022 / the §27 grilling outcome (app-private state is RAM-only, no story needed).

- **Third bench session — input scaling stage + full-chain loopback validated.** `bench-log.md` 2026-04-30 (continued) entry. Built TL072 #3 channel A as the input scaling stage for jack J1 → MCP3208 ch 0 (R_series + BAT43 clamps + summing amp + offset divider on pin 3 mirrored from §6). Static jumper test gave op-amp output within 10 mV of prediction across ±5 V input range; slope −0.181 V/V vs predicted −0.180 (1 % error). Round-trip loopback (J3 → cable → J1) tracks within 20–30 mV across full DAC sweep, slope 0.796 vs predicted 0.792 (0.5 % error). **Story 015 acceptance criteria met. The entire post-pivot platform is now bench-validated end-to-end** (Stories 012 + 013 + 015).

### Fixed

- **`bench-wiring.md` §7 step 3 — "input floating ≈ +1.96 V" was wrong.** With the jack truly floating, no current can flow through R_series + R_in2, so no offset gain develops and pin 1 sits at V_pin3 ≈ +1.66 V instead of the bias-only output. The +1.96 V reading only appears when the jack is **actively grounded** — that's the actual test condition. Caught when the freshly-replaced TL072 #3 read 1.65 V on pin 1 with jack floating; doc said 1.96 V was expected, but the chip was behaving correctly. Doc updated to spell out both cases.

- **Bench rebuild after voltage misconfiguration killed the original MCP3208 + at least one TL072** (cause: rails misrouted on the breadboard pre-power-on). New `bench-log.md` 2026-05-02 entry. Replaced XIAO RP2350, MCP3208, and TL072 #3; brought up the analog chain piece by piece via the smoke-test firmware. DAC8552 + VREF buffer + output op-amps all survived (firmware still reads correct ±9 V at jacks J3/J4). Input stage rebuilt and characterized: 3-point linear fit gives slope −0.180 V/V (matches design within 1 %), and the inverted firmware constants (`gain ≈ −5.57`, `offset ≈ +10.88`) come within 0.2 % of the existing constants in `main.cpp` — **no firmware calibration change required.** Full-loop validation (J3 → patch cable → J1 → MCP3208 ch 0) tracks within 30 mV across the ±9 V swing. **The bench rig is fully restored.** Also captured: pot-as-source impedance lesson (100 kΩ pot driving 100 kΩ R_series sags badly — use a stiff supply or buffered source), and the SPI-MISO-pull-up disambiguation trick for "is the chip alive or not" debugging.

- **Input scaling stage topology + math (`bench-wiring.md` §7).** Same shape of bug as the §6 fix earlier in the week:
  - Original spec had VREF feeding the inverting input via R_off — same single-positive-supply problem as §6 had. Corrected to put the offset on **pin 3 (non-inverting) via a divider** (VREF → 22 kΩ → pin 3 → 15 kΩ → GND ≈ 1.66 V).
  - Original spec also missed that R_series (100 kΩ) and R_in2 (22 kΩ) are **in series** between the jack and the op-amp's virtual-ground inverting input — they look like a single 122 kΩ input resistor, not separate stages. Predicted-values table assumed `gain = R_fb/R_in2 = 0.214`, but actual gain is `R_fb/(R_series + R_in2) = 0.039` with R_fb = 4.7 kΩ — about 17 % of expected. Caught at the bench when the first measurement gave the right shape but the wrong magnitude.
  - Fix: bump R_fb from 4.7 kΩ to **22 kΩ** to compensate for the 100 kΩ R_series. New gain = 22/122 = 0.180. Bench-validated with predictions within 1 % across the full sweep.
  - Wire list, math, schematic, predicted-values table, and bench procedure all updated. Channel-B note updated to share the new offset divider via pin 5 → pin 3, mirroring the §6 trick.

- **Second bench session — channel B + ADC loopback validated.** `bench-log.md` 2026-04-30 entry. Channel B built (47 kΩ R_fb, sharing channel A's offset divider via pin 5 → pin 3); static jumper test gives +9.28 V / −9.91 V, dynamic two-channel scope shows clean independent triangle (J3) + square (J4). MCP3208 wired and brought up; loopback (DAC OUTA → MCP3208 ch 0) tracks within ±15 mV across the full 0..4.096 V sweep — dominated by analog noise + op-amp offset, not converter accuracy. **Story 012 acceptance criteria met on the bench.**
- **Smoke-test firmware extended to drive both DAC channels.** `src/main_smoketest.cpp` now writes channel A as a 1 Hz triangle (full DAC range) and channel B as a 0.5 Hz square between 1/4·VREF and 3/4·VREF. Different waveforms make channel-independence visually obvious on a 2-channel scope. Serial output gains a `dac_b_v` column.

- **First post-pivot bench session — channel A end-to-end working.** Captured in `bench-log.md` 2026-04-29 entry with scope screenshot. Validates: SPI bus arbitration, DAC8552 24-bit frame protocol via Rob Tillaart lib + `outputs::write()` HAL, pot+TL072 VREF stand-in stable to 4.096 V, inverting amp + non-inverting offset divider produces clean ±9 V bipolar swing at jack J3, end-to-end signal chain (XIAO firmware → SPI → DAC → analog stage → Eurorack-spec jack output) all the way through. Static measurements within 0.4 V of predicted; dynamic 1 Hz triangle on scope shows period 1.004 s and frequency 0.996 Hz vs spec'd 1.000 / 1.000.
- **`docs/images/bench-channel-a-triangle-jack.png`** — Rigol DS1Z scope capture of the bipolar triangle at jack J3 during the first end-to-end test, linked from `bench-log.md`.

### Fixed

- **DAC output-stage topology (`bench-wiring.md` §6 + Story 013).** Original spec had VREF feeding pin 2 alongside VDAC via R_off — this gives `Vout = −(R_fb/R_in)·VDAC − (R_fb/R_off)·VREF`, which can never produce a bipolar swing from a single positive reference (both contributions invert; output rails to negative). Caught at the bench when the first measurement read −8.90 V instead of the predicted +9 V. Corrected topology puts the offset on **pin 3 (non-inverting input) via a divider** — VREF → 22 kΩ → pin 3 → 15 kΩ → GND. With pin 3 at 1.66 V, the transfer function becomes `Vout = (1 + R_fb/R_in)·V_pin3 − (R_fb/R_in)·VDAC`, giving a clean symmetric ±9 V swing. Channel B's wire list updated to share the divider with channel A (pin 5 ties directly to pin 3 — no second divider needed). Story 013 acceptance criteria + Notes updated to match.

### Added

- **`docs/decisions.md` §26 — Pin budget at the limit.** Captures that all 11 castellated XIAO pins (D0–D10) are used after the SPI pivot; back-side pads aren't realistic for a 2 HP form factor. The MCP3208's 6 spare ADC channels (ch 2..ch 7) are the architectural safety valve — any future analog input (extra pots, more CV jacks) routes there at zero XIAO-pin cost. The tempo pot can also move from D0 to MCP3208 if a future app needs a dedicated digital pin. On-board LED + NeoPixel cover status indication without spending edge pins.
- **Deferred decision — I/O daughterboard / breakout module.** Long-term answer to the pin-budget ceiling: a second small PCB mating to the main board via header, exposing additional jacks/pots/buttons/LEDs. Sketch covers analog expansion (spare MCP3208 channels → extra jacks; possible 2nd DAC8552 on the SPI bus), digital expansion (MCP23017 IO expander on the OLED I²C bus → button matrix / LED grid), and form-factor options (side-mate, daughter-stack, wing). Deferred until a concrete app demands it.

- **Story 012 firmware scaffolding — SPI HAL + smoke test.**
  - `lib/outputs/` wraps DAC8552 with `outputs::write(ch, volts)` / `gate(ch, on)` / `setVRef(v)` / per-channel `setCalibration(gain, offset)`. Defaults are `gain=1.0, offset=0.0` (passthrough) until bench-fitted.
  - `lib/inputs/` wraps MCP3208 with `inputs::readVolts(ch)` / `readRaw(ch)` / `setVRef(v)` / `setCalibration(gain, offset)`. Same calibration shape.
  - `src/main_smoketest.cpp` — dedicated smoke-test entrypoint: triangle wave on DAC ch A (1 s period, 50 samples/cycle), reads MCP3208 ch 0 via 500 µs settle delay, prints `t_ms / dac_v / adc_v / adc_count` to serial. Loopback procedure: jumper DAC OUTA → MCP3208 ch 0; expect linear tracking within a few LSBs.
  - New `[env:seeed-xiao-rp2350-smoketest]` PlatformIO env uses `build_src_filter` to swap `main.cpp` for `main_smoketest.cpp` — production arp firmware stays untouched until SPI HAL integration in a follow-up. Run: `pio run -e seeed-xiao-rp2350-smoketest --target upload`.
  - `lib_deps` enabled: `robtillaart/DAC8552`, `robtillaart/MCP_ADC` (both MIT, both auto-resolve to MCP3208 with hardware SPI default).
  - Smoke-test env: 2.0% RAM, 3.0% flash. Production env still builds. All 66 host tests pass.
  - Bench expects VREF = 4.096 V; sourced from the pot+TL072 stand-in per `docs/bench-wiring.md` §5 until REF3040 arrives, then drop-in replace.
- **`docs/bench-wiring.md`** — single-source pin/net/component reference for the breadboard rebuild covering Stories 012 + 013 + 015 in one session. Sections: BOM, power distribution, XIAO pinout w/ encoder migration (D8/D9/D10 → D1/D2/D7), shared SPI bus, pot+TL072 VREF stand-in, output stage with E96/E12 picks, input protection + scaling, OLED + UI hookup, jack assignments, 9-step incremental build/test order.

- **`docs/decisions.md` §25 — JLCPCB-first BOM strategy + VDD/VREF separation.** Captures the policy now driving Rev 0.1 part selection: prefer JLC Basic parts when an equivalent exists; accept Extended for specialty ICs. Architectural change recorded: VDD (AMS1117 +5 V LDO) is decoupled from VREF (REF3040 4.096 V precision reference) so the 16-bit DAC isn't bottlenecked by the LDO's 1–2 % accuracy. Both DAC8552 and MCP3208 share the REF3040 output. Resolution gains: DAC = 62.5 µV/step exactly; ADC = 1.00 mV/step exactly.
- **`hardware/bom.md`** — single source of truth for the JLC PCBA BOM. Lists every SMD part with LCSC number, package, Basic/Extended class, and unit price. Through-hole / panel parts (Thonkiconn jacks, encoder, pots, OLED, XIAO sockets) tracked separately as hand-soldered. Includes a vetting checklist for adding future parts.
- **Vetted JLC parts (vs decisions.md §25):** AMS1117-5.0 (C6187, Basic) for +5 V LDO; REF3040AIDBZR (C19415) for 4.096 V VREF; DAC8552IDGKR (C136020) for the dual 16-bit DAC; MCP3208T-CI/SL (C626764) C-grade for the 8-ch ADC; TL072CDT (C6961, Basic) ×2 for op-amps; BAT54SLT1G (C19726) ×4 for jack clamps (Schottky series-pair, replaces individual BAT43s — same 30 V breakdown, fewer footprints); GZ2012D601TF (C1017, Basic) ×2 for ±12 V rail ferrites.

### Changed

- **Stories 011 / 012 / 013 / 015 updated** to reflect the new architecture: ferrites + AMS1117 LDO + REF3040 precision VREF; output-stage gain bumped from 4 to ≈ 4.88 (R_fb = 48.7 k, R_in = 10 k, R_off = 20 k) so DAC's 4.096 V max swing still produces ±10 V at the jack; input-stage scaling recomputed for ADC's 4.096 V span; clamp diodes specified as BAT54S throughout (was BAT43).
- **Decisions.md "Deferred" list:** "DAC8552 VREF source" entry resolved — REF3040 is in from Rev 0.1 per §25, not deferred.

- **Story 020 — Order screens + Random + UpDownClosed.** Major arp engine + UI work:
  - **Engine renames** (clarification, no behaviour change): `ArpOrder::UpDown` → `ArpOrder::UpDownOpen`, `ArpOrder::Skip` → `ArpOrder::SkipUp`. The `SkipUp` rename anticipates a `SkipDown` and other finger-picking patterns.
  - **`ArpOrder::UpDownClosed`** — palindrome WITH endpoint repeat (`0,1,2,3,3,2,1,0`, period `2N`). Top doubled in middle; bottom doubled across loop boundary.
  - **`ArpOrder::Random`** — pure random index per step (with replacement). Uses `std::rand()` seeded in `setup()` from `millis()` ⊕ noisy ADC read.
  - **5 static order screens** auto-exported from `order.aseprite` tags. Bar-chart art: one bar per chord note, height = pitch, position = playback order.
  - **Procedural random-bars animation** for Random — 8 vertical bars at random heights, redrawn on every step advance when ORDER is the active param. Communicates unpredictability viscerally.
  - **`docs/decisions.md` §24** documents open vs closed palindrome terminology + Random seeding convention.
  - 6 new host tests (UpDownClosed × 3, Random × 3) + existing UpDown/Skip tests renamed. **Total: 60 → 66, all green.**
  - **Future deferred:** highlight active bar in the order-screen diagram per step.

- **Aseprite CLI auto-export pipeline.** New `tools/aseprite-export.js` walks `assets/screens/**/*.aseprite` and exports each tagged frame using Aseprite's built-in `{tag}` placeholder substitution (`aseprite -b file --save-as 'dir/<base>-{tag}.png'`). Single-frame tags produce `<base>-<tag>.png` directly; multi-frame tags (e.g. `Loop` covering all frames) produce `<base>-<tag>1.png`...`<base>-<tag>N.png` which are auto-detected and deleted with a friendly warning telling the user to remove the multi-frame tag in Aseprite. Untagged .aseprite files produce `<base>-.png` (empty-tag artifact) which is also auto-deleted with a warning. Tag-based naming (`key-A.png`) makes the C++ lookup self-documenting (`screens::key_A`). Wired into `npm run build:screens` ahead of `bitmap2header.js`. Aseprite path defaults to the macOS standard install; override via `ASEPRITE_PATH`.
- `tools/aseprite-list-tags.lua` kept around but **deprecated** — the simpler `{tag}`-substitution approach replaced it. Preserved in case a future task needs richer Aseprite introspection that the standard CLI flags don't expose (1.3.17.1's `--list-tags` and `--data` JSON tag-export return empty even when tags exist).
- **All 12 keys now graphical.** Auto-exported A through G PNGs from `key.aseprite`'s tags. The `keyNaturalScreen()` lookup maps each pitch class to its natural-note bitmap (sharps to the natural just below — C# uses C, F# uses F, etc.). When the active key is a sharp, an 8×8 `screens::sharp` glyph is composited over the natural at `(24, 18)`–`(31, 25)` (top-right corner flush with the right edge of the 32-wide portrait panel). Bench-tuned position 2026-04-26.
- New `screens::sharp` (8×8) and `screens::key_A`–`screens::key_G` (32×64 each) in the generated screens library.

- **Trigger ratcheting (Story 019).** New menu param `TRIG` (1, 2, 3, 4 — selectable via encoder). Each arp note still produces one pitch, but the gate fires N evenly-spaced times within the note. Each sub-gate is `(stepMs / N) * 0.5` long with a 5 ms floor. Composes with the external-clock multiplier — multiplier controls notes-per-pulse, ratchet controls gates-per-note (so external + ×2 + TRIG 3 = 6 gates per incoming pulse). Encoder cycle: SCALE → ORDER → ROOT → TRIG → SCALE. NeoPixel adds yellow/amber for TRIG. Bench-confirmed audibly at all four values. Future: random ratchet (Story 020 deferred).

- **Graphics pipeline for the OLED** — `assets/screens/*.png` are converted to a packed C++ bitmap library by `tools/bitmap2header.js` (Node, one dep: `pngjs`). The generated `firmware/arp/lib/screens/{screens.h,screens.cpp}` exposes each PNG as `screens::<name>` with a tiny `Screen` struct (`data`, `width`, `height`). **Animation support:** any subdirectory of PNGs under `assets/screens/` also emits a `screens::Animation` (ordered frame-pointer array + count). Frame order within a directory is natural-sort (`frame2` before `frame10`). Packed in Adafruit GFX `drawBitmap` byte order. Workflow: drop PNG → `npm run build:screens` → `pio run`. Details in `assets/screens/README.md`.
- **Boot splash animation.** 9-frame splash in `assets/screens/splash-screen-animation/` plays on every boot: frame 1 holds 3 s, frames 2–9 tick at 100 ms each (total ~3.8 s). Blocking call in `setup()`; runs before the normal menu renders.
- `OledUI::drawScreen(const screens::Screen&, x, y)` — thin passthrough to `Adafruit_SSD1306::drawBitmap()`.
- `OledUI::playAnimation(const screens::Animation&, first_frame_hold_ms, frame_delay_ms, x, y)` — blocking animation player with distinct first-frame hold (splash-friendly).
- `tools/generate-sample-png.js` — one-shot helper that creates a 32×64 test PNG for round-trip verification on fresh clones. Delete `assets/screens/sample.png` once real art exists.
- `package.json` at repo root — `npm install` pulls `pngjs`; `npm run build:screens` regenerates the library. Pivot from Python to Node for tooling since the project's primary human is more fluent in JS.

### Changed

- **Bitmap threshold flipped to OLED-native convention.** Previously: dark pixel in PNG → lit pixel on OLED. Now: **white pixel in PNG → lit pixel on OLED, black → off.** Matches how the panel physically looks — draw with light strokes on a black canvas and your image on screen matches the display. `tools/bitmap2header.js` updated; README and the sample-PNG generator follow suit.

- `firmware/arp/lib/trigger_in/` — pure-C++ firmware Schmitt trigger (`trigger_in::Schmitt`) for rising-edge detection on a polled voltage signal. Defaults to Eurorack-standard thresholds (high = +1.5 V, low = +0.5 V, 1 V hysteresis). Constructor accepts custom thresholds; initial state is "not armed" so an idle-high jack at startup doesn't fire spuriously. Story 016 prep — wires straight into the buffered ADC reads from Story 015 when bench-built.
- 15 new GoogleTest cases in `test_trigger_in/` covering: initial-state safety, rising-edge fire-once, square-wave repeat, exact-threshold behaviour, hysteresis (ripple rejection + must-drop-below-low to re-arm), slow-ramp single-fire, reset, custom thresholds. **Total host tests: 45 → 60, all green.**

### Changed

- **Renamed the chord-anchor menu param from ROOT to KEY.** "Key" better describes what the user is dialing in — the tonal centre that anchors both the chord intervals and the scale quantisation. The selected pitch class IS the chord's root AND the scale's tonic, and musicians ask "what key is this in?", not "what root is this in?". Touchpoints: `Param::Root` → `Param::Key`, `rootPC` → `keyPC`, `ROOT_OCTAVE_BASE_MIDI` → `KEY_OCTAVE_BASE_MIDI`, `"ROOT"` → `"KEY"` in OLED + serial. Asset directory `assets/screens/root-screen/` → `key-screen/`, PNGs `root-A.png`..`root-F.png` → `key-A.png`..`key-F.png`, `screens::root_*` → `screens::key_*`. Underlying pitch-class-to-string helper `rootName()` kept as-is (it's a generic pitch-class formatter, not specifically about "root"). Bench-flashed 2026-04-26.

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
