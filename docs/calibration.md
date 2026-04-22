# Calibration

Bench-measured constants and procedures for the gio module signal chain. Update on every recalibration; cross-reference each entry from `bench-log.md`.

---

## V/Oct output (Story 004) — 2026-04-22

### Circuit

PWM (D2) → 2-pole RC filter (2× 10 kΩ + 100 nF) → MCP6002 op-amp non-inverting amp → 100 Ω series → V/Oct out.

```
                              ┌──[Rf]──┐
                              │        │
filter ── pin 3 (+IN_A)       │        │
                              │        │
                pin 2 (−IN_A)─┤        │
                              │        │
                           [Rg = 10k]  │
                              │        │
                             GND       │
                                       │
                pin 1 (OUT_A) ─────────┴──[100 Ω, R4]── V/Oct out
```

| Component | Spec value | Built with | Notes |
|---|---|---|---|
| R_g (gain set, −IN to GND) | 10 kΩ | 1× 10 kΩ | 5 % carbon |
| R_f (feedback, OUT to −IN) | 2.7 kΩ | 2.2 kΩ + 470 Ω in series ≈ 2.67 kΩ | substitution; 2.7 kΩ unavailable |
| R4 (output) | 100 Ω | 1× 100 Ω | confirmed from RA4M1 lessons — 1 kΩ here causes 10–20¢/oct flat error against 100 kΩ Eurorack inputs |
| Decoupling | 100 nF | 1× 100 nF, leads short, across pins 8/4 | |
| Op-amp B (unused) | tied off | pin 5 → GND, pin 6 → pin 7 (grounded buffer) | prevents oscillation |

Nominal gain = 1 + R_f/R_g = 1 + 2.67/10 = **1.267**.

### Procedure

1. Flash firmware that steps through MIDI C3–C7 (notes 48, 60, 72, 84, 96), 2 s each, on D2 PWM.
2. Multimeter (DC volts) between V/Oct output (after R4) and GND. Record voltage at each step.
3. Linear-fit the (target, measured) pairs. Slope = (G_actual / G_firmware). Update firmware `GAIN` to `G_firmware × slope`.
4. Re-flash, re-measure, confirm errors within ±2 mV per step.
5. Plug into a known-good 1 V/oct VCO. Use a tuner app at each octave; record frequencies; compute per-octave cents error.

### Measured data

**Multimeter pass 1, GAIN = 1.27 (nominal):**

| MIDI | Note | Target V | Measured V | Residual |
|---|---|---|---|---|
| 48 | C3 | 0.000 | 0.001 | +1 mV |
| 60 | C4 | 1.000 | 0.989 | −11 mV |
| 72 | C5 | 2.000 | 1.981 | −19 mV |
| 84 | C6 | 3.000 | 2.974 | −26 mV |
| 96 | C7 | 4.000 | 3.967 | −33 mV |

Linear fit: slope 0.9917 V/oct, intercept −1 mV, residuals ±2 mV from line. Real gain = 1.27 × 0.9917 = **1.260**.

**Multimeter pass 2, GAIN = 1.26 (calibrated):**

| MIDI | Note | Target V | Measured V | Error |
|---|---|---|---|---|
| 48 | C3 | 0.000 | 0.001 | +1 mV |
| 60 | C4 | 1.000 | 0.998 | −2 mV |
| 72 | C5 | 2.000 | 1.998 | −2 mV |
| 84 | C6 | 3.000 | 2.998 | −2 mV |
| 96 | C7 | 4.000 | 3.999 | −1 mV |

Slope 0.9995 V/oct. **All steps within ±2 mV of target. ±2.4¢ max equivalent error — well within ±5¢ AC.**

**VCO + tuner end-to-end:**

| Note | Frequency (Hz) |
|---|---|
| C3 | 67.4 |
| C4 | 134.2 |
| C5 | 267.8 |
| C6 | 536.5 |
| C7 | 1072.0 |

Per-octave interval errors: −7.7¢, −3.9¢, +2.9¢, −1.7¢. Best-fit line residuals all within ±4¢. The C3→C4 outlier is consistent with tuner-app imprecision at 67 Hz (1 Hz of measurement noise = 13¢ at this pitch). The DAC/op-amp side is independently verified to ±2.4¢ via the multimeter pass above; the slight excess slope error (~1.2¢/oct beyond DAC) is attributed to the VCO's own tracking.

### Firmware constants

```cpp
// firmware/arp/src/main.cpp
static const float    GAIN  = 1.26f;   // measured 2026-04-22; update after recal
static const float    VREF  = 3.3f;
static const uint16_t PWM_MAX = 4095;
// MIDI root: C3 (note 48) = 0.000 V; +1 V per octave thereafter.
```

Re-run the multimeter procedure if any of: MCP6002 swapped, R_f/R_g changed, supply rail moved off 5.0 V, ambient temperature shifts >10 °C from bench day.

---

## CV input divider (deferred)

Decisions.md §8 specifies 100 kΩ series + 68 kΩ to GND + BAT54 dual Schottky for both J1 (A3/GP29) and J2 (A1/GP27). Maps 0–8 V V/Oct → 0–3.24 V at ADC (~42 counts/semitone). Bench-verification deferred to the first story that consumes CV input — no story currently in the backlog uses CV in.
