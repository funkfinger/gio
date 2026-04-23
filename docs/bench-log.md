# Bench Log

Measurements from hardware bring-up, ordered chronologically. Referenced by stories and calibration notes.

---

## 2026-04-21 — Story 003: PWM DAC ramp through 2-pole RC filter

**Setup:** XIAO RP2350 on breadboard, firmware sweeping `analogWrite(D2, 0..4095)` at ~10 Hz, 12-bit, `analogWriteFreq(36621)`. RC filter: D2 → 10 kΩ → node A → 10 kΩ → node B, 100 nF from each node to GND. No op-amp.

**Observations (visual scope inspection, no screenshots captured):**

| Probe point | Result | Pass? |
|---|---|---|
| D2 raw | ~36.6 kHz square, duty visibly sweeping 0→100%, 0–3.3 V swing. Edges show breadboard ringing — cosmetic. | ✓ |
| After 1-pole (node A) | Sawtooth ramp 0→3.3 V with PWM ripple riding on it. Falling edge has visible exponential discharge curve (cap discharging through 10 kΩ when duty snaps 4095→0). Expected; τ ≈ 1 ms. | ✓ |
| After 2-pole (node B) | Smooth ramp, no visible ripple at 50 mV/div. | ✓ |
| ADC noise on A0 with PWM running | **Deferred to PCB stage.** Breadboard coupling is not predictive of PCB layout; will revisit during board bring-up. See decisions.md §3, §7. | — |

**Conclusion:** PWM → 2-pole RC filter signal path is bench-validated. Op-amp scaling stage (Story 004) is the next addition.

---

## 2026-04-22 — Story 004: V/Oct op-amp scaling and calibration

**Setup:** MCP6002-I/P breadboarded after the 2-pole RC filter. Gain network: R_f = 2.2 kΩ + 470 Ω in series (= 2.67 kΩ, 2.7 kΩ unavailable in kit), R_g = 10 kΩ → nominal gain 1.267. R4 = 100 Ω output. 5 V from XIAO 5V pin, 100 nF decoupling close to chip. Op-amp B tied off as grounded buffer (pin 5 → GND, pin 6 → pin 7).

**Calibration result:** firmware `GAIN` updated from nominal 1.27 → measured **1.26** after first pass. Second pass confirms all 5 octave steps land within ±2 mV of target (≈±2.4¢). End-to-end through a VCO + tuner shows ≤±4¢ best-fit residual; one octave reads −7.7¢ interval error attributed to tuner-app measurement noise at 67 Hz. Full data tables in `calibration.md`.

**Conclusion:** V/Oct signal chain is bench-validated and within musical tracking spec. Module is now capable of driving a Eurorack VCO. Ready for Story 005 (first arp).

**Deferred:** ADC noise check (Story 003 carryover), CV input divider bench-verify (no story uses CV in yet), scope screenshots in `requirements/` (bench-log entries serving as record).

---

## 2026-04-22 — Story 005: arp plays up pattern (v0.1.0 milestone)

**Setup:** Story 004 V/Oct chain unchanged. Added BC548 NPN gate driver: D6 (GP0) → 1 kΩ base resistor → BC548 base; emitter → GND; collector → 10 kΩ pullup to 5V → J4. Firmware-side `GATE_INVERTED = true` so `gateWrite(true)` produces gate HIGH at the jack. V/Oct out patched to VCO V/Oct in; gate out patched to VCO/envelope gate in; VCO out → monitor.

**Result:** audible C3 → E3 → G3 → C4 up-arpeggio at 120 BPM (500 ms/step, 50 % gate duty). Onboard LED flashes in sync with each note. Sounds correct on bench.

**Conclusion: the module makes music for the first time.** End-to-end signal chain validated — scales/arp/tempo logic + PWM DAC + RC filter + op-amp scaling + NPN gate driver + LED beat indicator all working together.

**Library tests:** 34/34 host tests passing (`scales` 13, `arp` 11, `tempo` 10) via `pio test -e native`.

**Deferred:** scope screenshots and audio recording of the arpeggio (bench-log entry serving as record).

---

## 2026-04-22 — Story 006: tempo pot for live BPM control

**Setup:** B-taper pot (10 kΩ used) wired between 3V3 and GND with wiper to A0 (D0/GP26). No filter cap; firmware samples ADC at step boundaries only.

**Firmware:** `analogRead(A0)` at every step advance → `tempo::potToBpm()` (exponential 20..300 BPM) → `tempo::bpmToStepMs(bpm, 4)` for 16th-note subdivision.

**Range adjustment:** spec called for 40–300 BPM but bench feel was that 40 BPM 16ths still felt too fast at the slow end. Dropped `BPM_MIN` to 20. New slow extreme is ~750 ms/note (~1.3 notes/sec), which matches "ambient crawl" expectations. Story AC and tempo tests updated; per-third ratio is now 15^(1/3) ≈ 2.47× (still feels evenly distributed across pot rotation).

**Result:** pot turn translates immediately to perceived tempo change; arp speeds up and slows down smoothly across the full range. LED beat indicator stays in sync. No audible jitter or stuttering.

**Library tests:** 35/35 passing (added 1 new exponential-ratio test).

---

## 2026-04-22 — Story 007: OLED + encoder bring-up

**Setup:** SSD1306 0.91" 128×32 OLED on I2C (SDA=D4/GP6, SCL=D5/GP7, VCC=3V3, GND=GND, addr 0x3C). PEC11 encoder: A→D8 (GP2), B→D9 (GP3), Click→D10 (GP4), common→GND. Internal pullups on all encoder pins (no external Rs). Built-in 4.7 kΩ pullups on the OLED breakout — no external Rs needed.

**Display substitution:** bench bring-up uses the 0.91" 128×32 in landscape. Final design uses the 0.49" 64×32 in portrait. Swap is one config block in `lib/oled_ui/oled_ui.h` — `OLED_WIDTH`, `OLED_HEIGHT`, `OLED_ROTATION`. No code changes elsewhere.

**Firmware:** standalone bring-up (no arp). Encoder rotation increments/decrements an int counter; OLED shows live value. Encoder click flashes onboard LED + prints "CLICK" to USB serial.

**Result:** all four ACs verified — splash "HELLO" on power-up, counter responds to rotation in real time, click triggers LED+serial. No bus glitches; OLED refresh is fast enough to feel instant when turning the knob.

**Encoder library decision:** `mathertel/RotaryEncoder` chosen (polling-based, RP2350-compatible). Replaces `paulstoffregen/Encoder` which was removed back in Story 002 due to AVR-only register macros. Recorded in `decisions.md` §17.

**Library tests:** still 35/35 passing — `oled_ui` and `encoder_input` are HAL modules (no host tests; bench-verified only per story note).

---

## 2026-04-22 — Story 008: encoder menu (full integration)

**Setup:** all prior hardware in place — V/Oct chain, BC548 gate driver, tempo pot, OLED, encoder. No new wiring. Onboard RGB LED used for active-param indication (no external NeoPixel).

**Scope:** flat menu model (no submenu). Encoder click cycles which parameter is active (Scale → Order → Scale). Encoder rotate changes the active param's value. Long-press (≥500 ms) resets the arp.

**Audible behaviour:** the existing `lib/scales/quantize()` is now applied to each note before it goes to the DAC, so the menu's Scale selection genuinely changes what plays. Pent Min snaps E→Eb in the chord; Major leaves the major triad as-is; Chromatic is identity.

**Bench iterations during the story:**

1. **Encoder skipping every other detent** — initial latch mode `TWO03` was wrong for our PEC11. Switched to `FOUR0` (still skipping intermittently), then `FOUR3` — clean one-detent-per-option. Documented as the default in `decisions.md` §17.
2. **Long-press only fires on release** — refactored `EncoderInput` so the long-press latches the moment the threshold is crossed mid-hold; release of a long-press gesture is silent (no spurious short-click). User now feels tactile RESET feedback while still holding.
3. **NeoPixel dark at boot** — three issues stacked: (a) brightness was 24/255, too dim to see; bumped to 64. (b) discovered onboard LED is on **GPIO22 (data) + GPIO23 (power-enable)** per Seeed wiki — the variant header doesn't expose either; hardcoded both. (c) it's RGBW (4 bytes/pixel), not RGB; switched to `NEO_GRBW`. (d) needed a 20 ms settle delay between enabling power and the first `pixel.show()`.

**Result:** Scale and Order both selectable via encoder; OLED shows live state; LED green on Scale active, blue on Order; long-press resets cleanly. All Story 008 ACs satisfied.

**BOOTSEL quirk noted:** double-tap BOOT button doesn't reliably enter UF2 mode on this XIAO RP2350. Reliable method: hold BOOT while plugging in USB. Documented in `firmware/arp/README.md`.

**Library tests:** 35/35 still green (`Order1324` test renamed to `Skip` in `test_arp`).

---

## 2026-04-22 — Story 009: CV input for V/oct transpose (α-summing)

**Setup:** repurposed MCP6002 op-amp B (previously tied-off grounded buffer) as a unity-gain buffer after a 100 kΩ + 69 kΩ (47 + 22 in series) divider on J2. Buffer output → A1/GP27. Pot on bench (0..3.3 V) substituting for a real Eurorack source.

**Design path** (pre-coding grill): settled on α-summing over β-with-detection after realising the switched-jack detection wiring is fiddly and that Plaits actually uses α. Encoder ROOT sets the base; CV adds on top. Unplugged behaviour is identical to plugged-0 V. No new hardware part needed.

**Firmware delta:**
- Chord stored as intervals `{0, 4, 7, 12}` instead of absolute MIDI, summed with a runtime `root_midi = encoder root + CV transpose`.
- New menu item `ROOT` with 12 pitch classes pinned to MIDI octave 3. NeoPixel magenta when active.
- `lib/cv/cvVoltsToTranspose()` — pure-logic helper, host-tested (10 new cases; now 45/45 green total).
- Hysteresis wrapper around CV reads (½-semitone deadband) to kill ADC-noise flutter at snap boundaries.
- Live OLED update of the effective ROOT (encoder + CV, mod 12) with `*` suffix when CV is contributing.
- Played note clamped to MIDI [24, 96] to keep everything inside the DAC's range.

**Bench iterations:**
1. **"Pot doesn't change ADC" + "pitch too high"** — resolved as a loose breadboard connection plus a leftover jumper. Repaired.
2. **"High end of pot tops out as a single note"** — serial diagnostics showed transpose values of 48–50 semitones at ~75 % pot rotation, far higher than spec. Root cause: the "100 kΩ" series resistor was actually **100 Ω** (Brown-Black-**Brown** vs Brown-Black-**Yellow**). With a 100 Ω top leg and 69 kΩ bottom, divider ratio collapsed from the spec 0.408 to ~0.999 — nearly all wiper voltage made it through. Swapped in the correct 100 kΩ; transpose now scales cleanly across the pot range.
3. **UpDown "feels wrong"** — turned out to be ADC flutter near scale-snap boundaries; hysteresis wrapper (½-semitone deadband) fixed it.

**Result:** pot sweep now audibly transposes the arp through scale-snapped roots across the full rotation. Per-note quantize preserved — different scales give recognisably different chord "shapes" as you sweep. Effective ROOT on OLED updates live. Encoder ROOT still fully functional when CV is at 0 V.

**Tests:** 45/45 host tests green.

**Deferred:** BAT54 clamp on op-amp B input (Rev 0.1 PCB). Wrap-vs-clamp for overshooting played notes — clamp chosen as predictable.

---

## 2026-04-23 — Story 010: external clock input on J1 (with side quest)

**Setup:** new firmware reads J1 (D3 / GP5) via `digitalRead` for rising-edge detection. Auto-switches to external mode within 2 s of detected edges; falls back to internal tempo on timeout. Tempo pot becomes a clock divider (÷1, ÷2, ÷4) when external is active. OLED shows `INT` or `EXT` tag.

**Pin reassignment from spec:** the spec called for J1 on `A3` / `GP29`, but `GP29` is **not exposed** on the XIAO RP2350 (only GP0–GP7 top + GP9–GP12, GP16–GP17, GP20–GP21 bottom-pads). All ADC-capable pins (GP26/27/28) are taken (tempo pot, CV in, PWM out). J1 is therefore digital-only on this build; pitch mode on J1 is deferred until an ADC pin is freed (e.g. by replacing the tempo pot with encoder navigation).

**Side quest (separate session, same day):** built `firmware/clock-mod2/` to drive a HAGIWO MOD2 board as a bench clock source. Hit a board defect on the MOD2 — JP2 (solder bridge across C18 for AC/DC coupling) doesn't actually route to C18's pads on this PCB rev. Workaround: solder a wire across C18 directly. Documented in `firmware/clock-mod2/README.md` and `decisions.md` deferred section.

**RP2350-E9 silicon bug — main quest blocker:**
1. Built the spec'd 100 kΩ + 100 kΩ divider on J1.
2. With MOD2 patched in, `digitalRead(D3)` was stuck at HIGH. No edges ever detected.
3. Multimeter at D3 showed "low" sitting at ~2.13 V — well above the GPIO HIGH threshold (~1.6 V), so the GPIO never saw a true LOW.
4. Verified MOD2 output was a clean 0–8 V square wave (scope), GND was shared between boards, divider continuity was correct, divider input (post-resistor-A) was 0 V at low.
5. Disconnected D3 from the divider — divider output dropped cleanly to 0 V. So D3 itself was sourcing/holding ~2 V into the divider.
6. Tried `INPUT_PULLDOWN` instead of `INPUT` — no change.
7. Soldered in a fresh XIAO RP2350 (suspected ESD damage). Same result.
8. **User identified the silicon errata: [RP2350-E9](https://hackaday.com/2024/08/28/hardware-bug-in-raspberry-pis-rp2350-causes-faulty-pull-down-behavior/)** — when input buffer is enabled with internal pulldown, the pad latches at ~2.2 V. Pi Foundation workaround: external pulldown must be ≤ 8.2 kΩ.
9. **Fix:** swapped the divider from 100 kΩ + 100 kΩ → **10 kΩ + 10 kΩ**. Same 1:2 ratio (firmware unchanged) but now the bottom-leg 10 k is low enough to overpower the latched silicon-bug voltage. Edges detected immediately.

Total bench time on the RP2350-E9 hunt: ~90 minutes. Documented in `decisions.md` §18 so we don't repeat the error on Rev 0.1 PCB. Rule: any GPIO input that needs a pulldown gets an external resistor ≤ 8.2 kΩ.

**Result:** gio now correctly tracks external clock from MOD2. EXT badge appears on OLED. LED blinks per pulse. Divider zone hysteresis works. Disconnect cable → falls back to internal tempo within 2 s.

**Deferred:** pitch mode on J1 (needs an ADC pin freed up); BAT43 clamp on J1 input (Rev 0.1 PCB).
