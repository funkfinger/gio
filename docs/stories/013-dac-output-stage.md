# Story 013: DAC Output Stage — Bipolar ±10V Swing via TL072

**As a** bench engineer
**I want** each DAC channel buffered through a TL072 op-amp stage that can swing the full Eurorack ±10V range
**So that** outputs can serve as V/Oct (0–8V positive), bipolar CV (±5V or ±10V), or fast gates (0/+5V) — all from the same circuit

## Acceptance Criteria

### Hardware

- [ ] Per-output topology — **inverting summing amplifier** (Mutable house style):
  ```
  DAC (0..+5V) ──[R_in = 10k]──┐
                                ├──[− input of TL072]
  +5V (offset)──[R_off = 10k]──┘
                       │
  [R_fb = 40k]─────────┘
                       │
                  [output, 1k series]──[BAT43 to ±12V rails]──[jack tip]
  ```
- [ ] Math: `Vout = −(R_fb/R_in) × Vdac + (R_fb/R_off) × (−V_off)`. With R_in = R_off = 10k, R_fb = 40k, V_off = +5V: `Vout = −4×Vdac + (−20V)` → wait, sign error, see notes.
- [ ] **Practical target:** DAC 0V → +10V out; DAC 2.5V → 0V out; DAC 5V → −10V out. (Inverted, scale ±10V.) Firmware compensates for the inversion in `voltsToDacCount()`.
- [ ] Power: TL072 on ±12V (Story 011 rails).
- [ ] Output protection: **1 kΩ series + BAT43 dual Schottky to ±12V rails** at the jack. Caps short-circuit current to ~12V/1k = 12 mA, well within TL072 short-circuit-protected output.
- [ ] Both DAC channels (OUTA + OUTB) get identical output stages — symmetric I/O for the platform.

### Firmware

- [ ] Helper: `outputs.write(channel, volts)` — takes a target voltage, applies per-channel calibration constants (`gain`, `offset`), produces a DAC count, calls `dac.write()`.
- [ ] Calibration constants stored as `static const` in firmware initially (move to flash post-MVP).
- [ ] Bench-fill the calibration table: at known DAC counts (1024 / 16384 / 32768 / 49152 / 65535), measure output voltage with multimeter; least-squares-fit `gain` and `offset` per channel.

### Bench

- [ ] **V/Oct accuracy:** firmware writes a chromatic scale C0 → C8 (96 semitones) using `outputs.write(0, midiToVolts(midi))`. Multimeter at the jack reads each note within **±2 mV** of target (1 mV ≈ 1.2 cents at 1 V/oct).
- [ ] **Bipolar swing:** firmware writes ±10V triangle on ch B at 1 Hz. Scope confirms full ±10V swing, no clipping, no rail-stick at extremes.
- [ ] **Per-channel independence:** write ch A to a static V while sweeping ch B; ch A reading on multimeter must not move more than ±1 mV.
- [ ] **Output stability into typical load:** plug into a Eurorack VCO's V/Oct input (presents ~100 kΩ load); confirm voltage at jack doesn't sag.

## Notes

- **Why inverting summing amp:** DAC8552 is single-supply 0–5V output; getting bipolar requires offset + gain. The inverting summing amp is the classic minimum-parts way to do this. The inversion is paid for in firmware (`dac_count = midToCount(−v)`) — trivial.
- **Why R_fb = 40k for gain 4:** trade-off between component count (single resistor) and noise (higher gain = more amplification of input-stage noise). 40k is unobtainium in standard E12; use **39k** (E12) or two 20k in series. Confirm value at bench based on actual swing measured.
- **Reference voltage:** at Story 012 we tied DAC VREF to +5V (the LM7805 output). If V/Oct calibration drifts > ±1 mV with temperature, add a precision 4.096V reference IC (REF3040 etc) — defer until measured.
- **Output AC coupling:** not used. V/Oct must be DC. Anyone who wants AC coupling can do it externally.

## Depends on

- Story 011 (rails), Story 012 (SPI working)

## Status

not started
