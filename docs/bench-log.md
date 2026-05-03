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

---

## 2026-04-29 — Stories 011 + 012 + 013 (channel A): SPI bring-up + DAC output stage

First post-pivot bench session. Goal: validate the entire signal chain from the XIAO firmware through SPI → DAC8552 → inverting summing amp → Eurorack-friendly bipolar jack output, on **channel A only** (channel B and the input stage deferred to next session).

**Setup:**
- XIAO RP2350 on female socket strips, plugged into Protomato breadboard (rails ±12 V / +5 V / GND verified clean).
- VREF stand-in: 10 kΩ multi-turn trimpot, divided +5 V → wiper, buffered through TL072 #1 channel A as a unity-gain follower. Channel B parked (pin 5 → GND, pin 6 → pin 7). Buffer output dialled to 4.096 V on the multimeter, locked.
- DAC8552 (MSOP-8 on a SOT/MSOP → DIP adapter): VDD=+5 V (pin 1), VREF tied to the buffered 4.096 V rail (pin 2), GND (pin 8), 100 nF decoupling at pin 1. SPI to XIAO: SCLK=D8 (pin 6), DIN=D10 (pin 7), /SYNC=D3 (pin 5). VOUTA (pin 4) → output stage. VOUTB (pin 3) not connected.
- TL072 #2 channel A: inverting summing amp with R_in = 10 kΩ from DAC8552 pin 4, R_fb = 2× 22 kΩ in series = 44 kΩ from pin 1, output through 1 kΩ series → jack J3 tip with BAT43 clamps to ±12 V. Channel B parked. Pin 8 = +12 V, pin 4 = −12 V, 100 nF on each.
- **Topology correction during the session:** original `bench-wiring.md` §6 had R_off (20 kΩ) feeding VREF into pin 2 alongside the DAC. That makes both contributions inverting → output rails to negative for any DAC value. Reworked to put VREF on **pin 3 (non-inverting)** via a divider — 22 kΩ from VREF to pin 3, 14.7 kΩ from pin 3 to GND. V_pin3 = 4.096·14.7/(22+14.7) ≈ 1.64 V, which gives `Vout = 5.4·V_pin3 − 4.4·VDAC` — a real bipolar swing.

**Static measurements (DAC pin disconnected; jumper into R_in input end):**

| Test | Predicted | Measured | Notes |
|---|---|---|---|
| Pin 3 (1IN+) | +1.64 V | **+1.628 V** | Divider correct |
| Pin 2 (1IN−) | tracks pin 3 (virtual short) | **+1.630 V** | 2 mV input offset, well within TL072 spec |
| Jumper R_in → GND, jack J3 | +8.86 V | **+8.83 V** | + offset (DAC=0 simulated) |
| Jumper R_in → VREF rail, jack J3 | −9.17 V | **−9.25 V** | − swing (DAC=4.096 simulated) |

Symmetric ±9 V swing as expected. Slight asymmetry vs. spec is from R_fb being 44 kΩ (2×22 in series) instead of the spec'd 48.7 kΩ — bench-acceptable; full ±10 V can come back when the right resistor is on hand.

**Dynamic measurement (DAC connected, smoke-test firmware running):**

Smoke-test triangle on DAC ch A, 1 Hz, 50 samples per cycle. Probed jack J3 tip on Rigol DS1Z scope, channel 1 = 5 V/div, time = 200 ms/div.

![Channel A bipolar triangle at jack J3](images/bench-channel-a-triangle-jack.png)

| Scope reading | Spec | Measured |
|---|---|---|
| Period | 1.000 s | **1.004 s** (0.4 % off — Arduino `millis()` quantization, well within tolerance) |
| Frequency | 1.000 Hz | **0.996 Hz** |
| Steps per cycle | 50 | 50 visible in staircase |
| Shape | clean symmetric triangle | clean, no glitches, no dropped samples |
| Amplitude | ±9 V | matches the static numbers above (≈ 18 V pp) |

**What this validates:**
- ✅ SPI bus arbitration (single-CS DAC8552 transaction works in isolation)
- ✅ DAC8552 24-bit frame protocol via Rob Tillaart lib + `outputs::write()` HAL
- ✅ Pot+TL072 VREF stand-in is stable enough for the DAC reference
- ✅ Inverting summing amp with offset divider (corrected topology) produces clean bipolar output
- ✅ End-to-end: XIAO firmware → SPI → DAC → analog stage → Eurorack-spec jack output

**What's still pending in this story:**
- Channel B (DAC OUTB → jack J4) — replicate channel A's wiring
- Bench validation against a real Eurorack VCO V/Oct input (audible test)
- ADC side: wire MCP3208 + smoke-test loopback (jumper J3 → MCP3208 ch 0)
- Input stage (Story 015) for J1 → ADC ch 0
- Calibration: bench-fit `outputs::setCalibration()` constants once channel B is up

**Doc fixes from this session:**
- `bench-wiring.md` §6 needs the topology correction (offset on pin 3, not pin 2) and resistor-value updates from this build.
- `decisions.md` and Story 013 likewise reference the original (broken) topology — both need the same sign correction.

---

## 2026-04-30 — Stories 012 + 013 (channel B + ADC loopback): SPI fully validated

Continuation of yesterday's session. Goal: bring up DAC channel B, then wire the MCP3208 ADC and prove the full SPI bus end-to-end via DAC → analog loopback → ADC.

### Channel B build

Removed channel B's parking wires (pin 5 → GND, pin 6 → pin 7). Added:
- 1× 10 kΩ R_in from DAC8552 pin 3 (VOUTB) to TL072 #2 pin 6
- 1× 47 kΩ R_fb (E12, single resistor — channel A used 2× 22 kΩ in series for 44 kΩ)
- Pin 5 (2IN+) → pin 3 (1IN+) — sharing channel A's offset divider, no second divider needed
- 1 kΩ output series → jack J4
- BAT43 clamp pair to ±12 V

**Static jumper test (DAC pin 3 disconnected):**

| Test | Predicted (R_fb=47k) | Measured |
|---|---|---|
| Jumper R_in → GND, jack J4 | +9.46 V | **+9.28 V** |
| Jumper R_in → VREF rail, jack J4 | −9.79 V | **−9.91 V** |

±9.5 V swing, slightly different from channel A (44 k vs 47 k R_fb difference + E12 tolerance). Bench-acceptable — both channels give clean bipolar output.

### Smoke-test firmware extended for both channels

Updated `src/main_smoketest.cpp`:
- **Channel A:** unchanged 1 Hz triangle, full DAC range
- **Channel B:** **0.5 Hz square** between 1/4·VREF and 3/4·VREF (so jack J4 swings between ≈ +4.5 V and ≈ −4.5 V)
- Two different waveforms = visually obvious independence on a 2-channel scope

Scope (yellow CH1 = J3 triangle, cyan CH2 = J4 square) confirmed:
- Channel A triangle is clean and unchanged when channel B starts driving
- Channel B square has flat plateaus (no rail dips from channel A's continuous writes)
- No CS-line crossover (writes to A only move OUTA; writes to B only move OUTB)
- No power-rail crosstalk (channel A's continuous DAC writes don't perturb channel B's static output)

### MCP3208 ADC bring-up

Wired per `bench-wiring.md` §4 (PDIP-16 on adapter, in DIP-16 socket on the breadboard):

| MCP3208 pin | Net |
|---|---|
| 9 (DGND), 14 (AGND) | GND |
| 16 (VDD) | +5 V (with 100 nF decoupling at the pin) |
| 15 (VREF) | VREF rail (4.096 V — same buffered node as DAC) |
| 10 (/CS) | XIAO D6 |
| 11 (DIN) | tap off existing MOSI line (XIAO D10) |
| 12 (DOUT) | new wire to XIAO D9 (MISO — first time we use it) |
| 13 (CLK) | tap off existing SCK line (XIAO D8) |
| 1 (CH0) | jumper directly to DAC8552 pin 4 (VOUTA) — bypassing the input scaling stage for cleanest possible loopback |

Pre-power voltage checks: VDD = +5 V, VREF = +4.096 V, GND continuity, /CS idle = +3.3 V (XIAO holding it HIGH between transactions). All good.

### Loopback test result

Captured 6 seconds of serial output during a triangle cycle:

```
t_ms    dac_a_v  dac_b_v  adc_v   adc_count
413214  1.7449   3.0720   1.7484  1731
413298  2.4330   3.0720   2.4196  2447
413509  3.3014   3.0720   3.3208  3296
413993  0.0655   3.0720   0.0640  64
414014  0.1147   1.0240   0.1130  114    ← channel B transitions to low half
414120  0.9748   1.0240   0.9752  968
```

**ADC tracks DAC OUTA within ±15 mV across the full 0..4.096 V sweep.** That's about 10–15 LSBs of the MCP3208 at 4.096 V VREF, dominated by analog stage noise and op-amp offset rather than converter accuracy:

- DAC8552 INL spec: ±4 LSB at 16-bit = ±0.25 mV (negligible)
- MCP3208 C-grade INL spec: ±2 LSB at 12-bit = ±2 mV
- VREF stability of pot+TL072 stand-in: a few mV
- Sample-and-hold settling on breadboard wiring: a few mV

Channel B's square wave also shows up cleanly in the `dac_b_v` column (3.0720 → 1.0240 V transitions land at the half-period boundary).

### What this validates

- ✅ MCP3208 24-bit-frame SPI protocol via Rob Tillaart `MCP_ADC` lib + `inputs::readRaw()` HAL
- ✅ Shared SPI bus arbitration with two peripherals (CS_DAC and CS_ADC toggle independently per transaction)
- ✅ Default ~1 MHz SPI clock (under MCP3208's 2 MHz spec at VDD=5V)
- ✅ End-to-end: firmware → SPI → DAC → analog jumper → MCP3208 → SPI → firmware
- ✅ Both DAC channels independent (no crosstalk, no CS interference)

**Story 012 acceptance criteria are met on the bench.** Story 013 channel B done. Only Story 015 (input scaling stage) remains for the platform-rebuild bench session.

### What's still pending

- Audible test: plug J3 into a Eurorack VCO V/Oct input and listen to the 1 Hz pitch sweep
- Story 015: input scaling stage for J1 → MCP3208 ch 0 (TL072 #3, with input 1 active and input 2 deferred)
- Calibration: bench-fit `outputs::setCalibration()` constants per channel for V/Oct accuracy
- Re-run loopback through the input scaling stage once it's built (jack J3 → R_series + clamp + summing amp → MCP3208 ch 0) — that's the full signal-chain round-trip

---

## 2026-04-30 (continued) — Story 015 (input scaling) + full-chain loopback

Picking up the same bench rig from earlier today. Built input 1 of the protected/scaled input stage, then closed the loop by patching J3 → J1 with a Eurorack cable and reading the round-trip in the smoke-test serial output.

### Input stage build (TL072 #3, channel A)

Wired per `bench-wiring.md` §7:
- R_series = 100 kΩ at jack J1
- BAT43 dual clamp at node A (anode→+12V, cathode→−12V; each diode flipped opposite)
- R_in2 from node A to TL072 #3 pin 6 (1IN−)... initially with **R_fb = 4.7 kΩ** per the doc
- Offset divider on pin 3: VREF → 22 kΩ → pin 3 → 14.7 kΩ → GND ≈ 1.66 V
- Channel B parked (pin 5 → GND, pin 6 → pin 7)
- TL072 #3 pin 1 (1OUT) → MCP3208 pin 1 (CH0); replaced the previous direct DAC-OUTA → CH0 loopback jumper

### Math error caught at the bench (again)

First static jumper test on the input-stage op-amp output:

| Test | Predicted (with bad math) | Measured |
|---|---|---|
| Tip floating | +1.66 V | **+1.643 V** ✓ (matches) |
| Tip → GND | +2.04 V | **+1.707 V** ✗ |
| Tip → +5 V | +0.97 V | **+1.515 V** ✗ |
| Tip → −5 V | +3.11 V | **+1.873 V** ✗ |

Slope ≈ −0.036 V/V vs. predicted −0.214 V/V — about **17 % of expected**. Op-amp output too small.

**The bug:** R_series (100 kΩ) and R_in2 (22 kΩ) are **in series** between the jack and the op-amp's virtual-ground inverting input. They look like a single 122 kΩ input resistor, not separate stages. So the actual gain is `R_fb / (R_series + R_in2) = 4.7/122 = 0.039`, not `R_fb/R_in2 = 0.214` as the doc's math claimed.

### Fix: R_fb 4.7 kΩ → 22 kΩ

Swapped R_fb on TL072 #3 from 4.7 kΩ to 22 kΩ. New gain = 22/122 = 0.180 — reasonable target for ±10 V jack range fitting into 4.096 V ADC range with margin.

Re-ran the static jumper test:

| Test | Predicted (gain 0.180, V_pin3 = 1.66) | Measured |
|---|---|---|
| Tip floating | +1.96 V | **+1.949 V** ✓ |
| Tip → GND | +1.96 V | **+1.947 V** ✓ |
| Tip → +5 V | +1.06 V | **+1.053 V** ✓ |
| Tip → −5 V | +2.86 V | **+2.860 V** ✓ |

All four readings within 10 mV of prediction. Slope = (2.860 − 1.053)/(−5 − +5) = **−0.181 V/V** vs predicted −0.180 — within 1 % across ±5 V test range.

### Round-trip loopback test

With input stage validated, patched a Eurorack cable from J3 → J1 to close the full signal chain:

```
firmware → SPI → DAC8552 → output stage (TL072 #2 ch A) → J3 → cable → J1 →
                input stage (TL072 #3 ch A) → MCP3208 → SPI → firmware
```

False start: first attempt with patch cable in J1 killed the J3 sawtooth on the scope. Cause: a leftover GND jumper from the static validation was still pinning J1, so plugging J3 in back-drove the GND jumper. Pulled the jumper, re-tried — sawtooth returns.

Smoke-test serial captured during a triangle cycle:

```
t_ms    dac_a_v  dac_b_v  adc_v   adc_count
134506  4.0468   1.0240   3.5529  3551
134675  2.6706   1.0240   2.4636  2479
134759  1.9825   1.0240   1.9275  1934
134991  0.0819   1.0240   0.4141  411
135244  1.9907   3.0720   1.9365  1950
```

**Predicted** transfer: `adc_v ≈ 0.378 + 0.792 × dac_a_v` (composing output stage's bipolar swing with the input stage's attenuated read-back). Measured slope = **0.796** vs predicted 0.792 — within 0.5 %. Per-point error ≈ 20–30 mV across the sweep.

### Where the 30 mV residual comes from

Five op-amp channels and ~10 resistors in the round trip, all carrying real-world tolerances:

- VREF dial-in: ~5 mV vs 4.096 V nominal
- TL072 input offset: spec 3 mV typ × 4 cascaded channels
- E12 resistor tolerance: ±5 % across 6+ resistors

Most of this stays put once the REF3040 lands — the residual is op-amp + resistor dominated, not VREF dominated. But it'll become **stable across temperature** instead of drifting with bench warmth.

### What this validates (cumulative across both 2026-04-30 sessions)

- ✅ **Story 012** — SPI bus + DAC + ADC sharing the bus
- ✅ **Story 013** — bipolar output stage on both DAC channels
- ✅ **Story 015** — input protection + scaling stage on input 1

**The entire post-pivot platform is bench-validated end-to-end.** Next bench session can focus on calibration (bench-fit `outputs::setCalibration()` + `inputs::setCalibration()` constants), then plug J3 into a real Eurorack VCO for the audible V/Oct test.

### Doc fixes from this session

- `bench-wiring.md` §7 has the same math bug as §6 had pre-2026-04-29 — predicted op-amp output values used `gain = R_fb/R_in2`, ignoring R_series in series. Fix by stating the correct effective input resistance and updating the predicted-values table for R_fb = 22 kΩ.

---

## 2026-05-02 — Bench rebuild after voltage misconfiguration; tempo pot moves to MCP3208

**Setup:** XIAO RP2350 #1 fried by a power-supply misconfiguration that pushed wrong voltages onto the breadboard, taking out the original MCP3208 and at least one TL072 along with it. Replaced the XIAO and brought up the analog chain piece by piece using the smoke-test firmware (1 Hz triangle on DAC OUTA, 0.5 Hz square on DAC OUTB, MCP3208 ch 0 read every loop).

### Bring-up sequence

1. **New XIAO + smoke-test flash.** Confirmed CDC enumeration on `/dev/cu.usbmodem20401`; serial stream showed `dac_a_v` triangle and `dac_b_v` square commanding correct firmware-side values. Board alive. ✓
2. **DAC8552 + VREF + output op-amps survived.** Probed DAC8552 pin 4 (VOUTA) and pin 3 (VOUTB) — both tracked the firmware's commanded values cleanly. VREF rail steady at 4.096 V. TL072 #1 (VREF buffer) and TL072 #2 (output stages) intact. Jacks J3 and J4 produce the expected ±9 V swings.
3. **First MCP3208 replacement was DOA.** Soldered a fresh chip onto a spare DIP-16 breakout. Smoke-test reads stayed at `adc_v = 0.0000, adc_count = 0` regardless of CH0 jumper position (GND, VREF rail, DAC pin 4 loopback). Disambiguating test with a known-bad chip (the previously-fried original) read identically — proving the failure mode was repeatable and chip-localized.
4. **Second MCP3208 replacement worked.** Same wiring as #3 above; immediately read clean GND→0, VREF→4095, and locked DAC OUTA loopback within ~5 mV across the full 0..4.096 V sweep. Diagnostic of #3's failure inconclusive — possibly a soldering defect or a marginal chip; tossed.
5. **Input scaling stage (§7) rebuilt.** Replaced TL072 #3 (input op-amp). Initial bring-up readings at pin 3 came in wrong:
   - First reading: 1.022 V (expected 1.66 V). Math suggested ~7 kΩ of parallel load on pin 3.
   - After divider rewiring: 0.528 V — even worse, suggesting the op-amp itself was clamping (likely fried by the same event that took the ADC).
   - Replaced TL072 #3 with a fresh chip → pin 3 = **1.643 V** (1 % off the 1.66 V design target, all from E96/E12 resistor tolerance).

### Input-stage characterization (TL072 #3 channel A, J1 → MCP3208 ch 0)

Four-point sweep at jack J1 tip; metered TL072 #3 pin 1 (1OUT):

| Jack input | Predicted pin 1 | Measured pin 1 |
|---|---|---|
| floating | 1.66 V (no current → no offset gain) | **1.650 V** ✓ |
| GND | 1.96 V | **1.948 V** ✓ |
| +5 V (stiff supply) | 1.06 V | **1.056 V** ✓ |
| −5 V (stiff supply, *not* 100 kΩ pot — that sagged due to source impedance) | 2.86 V | **2.851 V** ✓ |

Three-point linear fit (excluding floating, which is the no-current degenerate case): `V_pin1 = 1.952 − 0.1795 · V_jack`. Slope agrees with design (−0.180) to better than 1 %. Inverted for firmware:

```
CAL_IN_0_GAIN   = -5.57  (existing constant in main.cpp: -5.56)
CAL_IN_0_OFFSET = +10.88 (existing constant: +10.89)
```

**New silicon matches old silicon to <0.2 %.** No firmware change required — existing calibration constants stand.

### Full-loop validation (J3 → patch cable → J1, end-to-end)

Smoke-test firmware running, J3 tip patched into J1 tip, MCP3208 ch 0 wired to TL072 #3 pin 1. Sample row at the negative peak of the DAC triangle:

```
dac_a_v = 4.080  →  J3 ≈ -9.0 V (output stage gain ≈ -4.4)
                 →  J1 = -9.0 V (patch cable)
                 →  pin 1 ≈ +3.57 V (input stage gain ≈ -0.18)
                 →  adc_v = 3.597 V  ✓ (predicted 3.566 V; 30 mV agreement)
```

ADC tracks DAC across the full ±9 V swing within ~30 mV per point. Composite slope on the DAC↔ADC transfer matches predicted within 0.5 %.

### Tempo pot moved to MCP3208 CH1

Decided during this session to relocate the tempo pot off XIAO D0 (native ADC) onto MCP3208 CH1 (precision ADC through REF3040). Rationale:

- All analog inputs now share one reference (REF3040, ±0.2 %, 75 ppm/°C) — uniform calibration story, no mixing of XIAO native ADC and external ADC.
- **Frees D0** for J1 clock/gate digital edge detection (per `decisions.md` §23, J1 is dual-purpose: CV in *or* clock-trigger). Edge detection wants a true digital pin.
- Cost: one extra SPI read per loop (microseconds) and one breadboard wire change (pot CW from +3.3 V → VREF rail).

Updated:
- `bench-wiring.md` §3 (D0 reassigned), §8 (pot wiring), §9 (J2 moves to ch 2 since ch 1 is now tempo)
- `decisions.md` §26 (pin budget table)
- `firmware/arp/src/main.cpp`: removed `PIN_TEMPO`; added `ADC_CH_TEMPO = 1`; replaced 3× `analogRead(PIN_TEMPO)` with `inputs::readRaw(ADC_CH_TEMPO)`. Build still 2.2 % RAM / 4.6 % flash; all 66 host tests pass.

### Doc fixes from this session

- `bench-wiring.md` §7 step 3 used to say "input floating ≈ +1.96 V." That's wrong — with the jack truly floating, no current flows through R_series + R_in2, no offset gain develops, and pin 1 sits at V_pin3 ≈ +1.66 V instead. The +1.96 V reading only appears when the jack is **actively grounded**. Caught when the freshly-replaced TL072 #3 read 1.65 V on pin 1 with the jack floating — the doc said this should be 1.96 V, but the chip was actually behaving correctly. Doc updated.

### Lessons captured

- **Voltage misconfiguration on the bench will cascade-kill ICs.** Always meter the rails *before* energizing the analog chain after any wiring change.
- **Pot voltage sources have non-trivial output impedance.** The 100 kΩ pot used as a stand-in for −5 V sagged badly (~−4 V at the loaded node) because it was sourcing into 100 kΩ R_series. Fix is either a buffered source (op-amp follower) or a stiff bench supply. This is exactly why the tempo pot uses a 10 kΩ part — low source impedance, can drive an ADC sample-and-hold cleanly.
- **MISO read of "0" is ambiguous on SPI** — it could mean "alive ADC reading 0 V" or "dead chip not driving DOUT." Pull-up trick on MISO disambiguates: live chip pulls low through driven 0; dead chip lets the pull-up float MISO high → reads 4095. Worth remembering for next time.

### Tempo pot validation (continued, same session)

After the tempo-pot relocation commit, re-flashed the smoke test (now extended to also stream `tempo_v` / `tempo_count` from MCP3208 CH1 alongside the existing CH0 columns) and swept the pot:

| Position | tempo_count | tempo_v |
|---|---|---|
| Fully CCW | **0** | **0.0000 V** |
| Mid-sweep | smooth, monotonic | linear with rotation |
| Fully CW | **4090** | **4.0950 V** (5 LSB shy of full-scale 4095) |

Full 12-bit range, no jitter, no dropouts. Pot is ready for the production firmware to drive tempo selection without any further bench work.

### Encoder bring-up (continued, same session)

After the tempo-pot validation, wired the PEC11R encoder per `bench-wiring.md` §8: A→D1, B→D2, common→GND, click→D7+GND. Internal pull-ups handle idle state.

**First test:** no count change at all over a 6 s rotation window. Cause: encoder common pin not wired to GND (the middle pin on the 3-pin side). Without GND on common, A and B sit at indeterminate voltages and the decoder sees no transitions. Re-wired.

**Second test:** count moved smoothly but **CW reads as a negative delta** (got −7 over a CW rotation that should read +7). Tried physically swapping the wires twice; CW remained negative either way. **Fix in firmware:** swap `PIN_ENC_A` and `PIN_ENC_B` in both `src/main.cpp` and `src/main_smoketest.cpp` (A→D2, B→D1). Rebuilt + re-flashed. CW now reads +13 as expected.

**Final validation matrix:**

| Test | Result |
|---|---|
| Rotate CW ~13 detents | `enc_count` delta = **+13** ✓ |
| Rotate CCW ~7 detents | `enc_count` delta = **−7** ✓ |
| Three quick taps | **3 click pulses, 0 long pulses** (debouncer verified honest — one tap = one pulse) ✓ |
| One long press (~1 s) | **0 click pulses, 1 long pulse** (long-press correctly suppresses the short-click for the same gesture) ✓ |

**Lesson for the bench-wiring doc:** §8 should explicitly call out "encoder common pin must go to GND" and the A/B swap (since this encoder reads inverted with the spec'd assignment). Both updates pending the next doc-fix pass.

### OLED bring-up + encoder switched to interrupts (continued, same session)

Wired the 0.49" 64×32 SSD1306 OLED per `bench-wiring.md` §8: VCC→3V3, GND→GND, SDA→D4, SCL→D5. On-board pull-ups handle the I²C bus.

Extended the smoke-test firmware to probe for the display at boot (`OledUI::begin()` returns false if no ACK at 0x3C) and refresh "ENC = N" at 10 Hz reflecting the live encoder count. **Result:** OLED detected immediately, splash + parameter display both render correctly.

**However**, the OLED revealed a polling-rate limitation: rotating the encoder fast through ~20 detents only registered ~7 in the count display. The mathertel/RotaryEncoder library was being called from `EncoderInput::poll()` once per main-loop iteration (~50 Hz), but the loop was now doing more work per iteration (DAC writes + ADC reads + tempo read + encoder poll + serial print + 5 ms OLED I²C blast every 100 ms). Combined loop time pushed past the encoder's per-detent transition window during fast rotation.

**Fix:** switched `EncoderInput` to interrupt-driven decoding. Both A and B pins now have CHANGE ISRs that call `RotaryEncoder::tick()` the moment any transition occurs — the loop's per-iteration work no longer matters. `poll()` still calls `tick()` as belt-and-suspenders.

**Validation after the switch:** rotated the encoder fast through ~20 detents; serial captured **+21 with 22 unique values seen** (every count appears in the trace, no skipped detents). OLED tracks the count smoothly. Fast rotations now register correctly.

This change benefits the production firmware too — same `EncoderInput` library, so future arp UI menus stay responsive even when the OLED is being drawn.

**Bench platform validation status (cumulative across all 2026-04-29 → 2026-05-02 sessions):**

- ✅ XIAO RP2350 + USB CDC + smoke-test firmware
- ✅ SPI bus arbitration (DAC8552 + MCP3208 sharing SCK/MOSI/MISO + per-chip CS)
- ✅ DAC8552 + output op-amps + jacks J3/J4 (±9 V swing, both channels)
- ✅ MCP3208 + input op-amp + jack J1 (±9 V → 0..4.096 V → ADC counts within 0.5 % of design)
- ✅ Tempo pot on MCP3208 CH1 (full 12-bit range, smooth)
- ✅ Encoder + click + long-press, interrupt-driven (no missed detents at any speed)
- ✅ OLED on I²C at 0x3C (live updates at 10 Hz)
- ✅ VREF rail (REF3040 stand-in via TL072 buffer at 4.096 V)
- ⏸ Channel B input (J2) scaling stage — wires planned, not built (out of scope until Story 010 external clock or a second CV use case lands)

**The bench platform is complete enough to host the production firmware end-to-end.** Next bench session: flash production firmware and do the audible-VCO test (J3 → external Eurorack VCO, listen).
