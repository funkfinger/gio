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
