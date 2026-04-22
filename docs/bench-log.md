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
