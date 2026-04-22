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
