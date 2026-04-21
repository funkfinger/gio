# Story 004: V/Oct Tracks Across Octaves

**As a** bench engineer
**I want** the MCP4725 → MCP6002 op-amp signal chain, breadboarded, to produce a V/Oct output that tracks within ±5¢ across C3–C7
**So that** the module can drive a VCO musically, proving the scaling circuit works with real components before committing to a PCB

## Acceptance Criteria

- [ ] MCP6002-I/P (DIP-8) breadboarded with R1=10kΩ, R2=2.7kΩ (gain ~1.27× nominal)
- [ ] Firmware outputs MIDI notes C3, C4, C5, C6, C7 on MCP4725, 2 seconds each, with serial output of target voltage
- [ ] Signal chain: RP2350 I2C → MCP4725 VOUT → MCP6002 non-inverting amp → J3 (V/Oct out)
- [ ] Multimeter reads each target voltage within ±2 mV (0.000V, 1.000V, 2.000V, 3.000V, 4.000V)
- [ ] `GAIN` constant in firmware set to measured op-amp gain (nominal 1.27, expect ~1.261 with 5% resistors)
- [ ] Pitch at VCO (Plaits or similar) tracks within ±5¢ verified via tuner app
- [ ] Calibration data and measured GAIN recorded in `docs/calibration.md`
- [ ] CV input divider resistor values confirmed: 0–5V Eurorack → 0–3.3V ADC. Values recorded in calibration.md and spec updated.
- [ ] Scope screenshots of V/Oct staircase in `requirements/`

## Notes

- Same circuit as RA4M1 project (R1=10k, R2=2.7k, MCP6002 single-supply 5V, R4=100Ω output).
- Confirmed in the RA4M1 project: R4=1kΩ causes 10–20¢/octave flat error against 100kΩ Eurorack inputs. Use 100Ω.
- MCP4725 settling time ~7 µs — well within the 2s step dwell. No settling artifact expected.
- CV divider: target is 5V → 3.0–3.3V max at ADC pin. Options: 100k/220k (maps 5V → 1.5V — too conservative), 100k/68k (maps 5V → 2.97V). Bench-verify whichever is used.
- I2C speed: 400 kHz is fine for DAC writes; OLED at 100 kHz if mixed. Confirm no bus contention.

## Depends on

- Story 003

## Status

not started
