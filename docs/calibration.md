# Calibration

V/Oct output calibration for the gio module. Updated after each bench session.

---

## Target Voltage Table

MIDI root = 48 (C3 = 0.000 V). Op-amp gain (nominal): 1.27× (R1=10kΩ, R2=2.7kΩ).

| MIDI Note | Note | Target V/Oct | DAC count (12-bit, GAIN=1.27) |
|-----------|------|-------------|-------------------------------|
| 48 | C3 | 0.000 V | 0 |
| 60 | C4 | 1.000 V | 1076 (approx) |
| 72 | C5 | 2.000 V | 2153 (approx) |
| 84 | C6 | 3.000 V | 3229 (approx) |
| 96 | C7 | 4.000 V | 4095 (clamped) |

DAC count formula: `count = (targetV / GAIN / 3.3) × 4095`

---

## Measured GAIN

*To be filled in during Story 004 bench work.*

| Date | Resistors | Nominal gain | Measured gain | Notes |
|------|-----------|-------------|---------------|-------|
| — | R1=10kΩ, R2=2.7kΩ | 1.270× | — | pending bench |

---

## Bench Measurements

*To be filled in during Story 004.*

| MIDI | Target | Measured | Error | Within spec? |
|------|--------|----------|-------|--------------|
| 48 | 0.000 V | — | — | — |
| 60 | 1.000 V | — | — | — |
| 72 | 2.000 V | — | — | — |
| 84 | 3.000 V | — | — | — |
| 96 | 4.000 V | — | — | — |

Spec: ±2 mV (≈0.024¢) for MVP. Target ±5¢ pitch tracking.

---

## Firmware Constants

```cpp
static constexpr float GAIN     = 1.270f;   // update to measured value after Story 004
static constexpr float DAC_VREF = 3.3f;
static constexpr int   MIDI_ROOT = 48;       // C3 = 0V V/Oct
static constexpr int   DAC_MAX   = 4095;
```
