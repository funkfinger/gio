# Story 013: DAC Output Stage — Bipolar ±10V Swing via TL072

**As a** bench engineer
**I want** each DAC channel buffered through a TL072 op-amp stage that can swing the full Eurorack ±10V range
**So that** outputs can serve as V/Oct (0–8V positive), bipolar CV (±5V or ±10V), or fast gates (0/+5V) — all from the same circuit

## Acceptance Criteria

### Hardware

- [ ] Per-output topology — **inverting summing amplifier** (Mutable house style):
  ```
  DAC (0..+4.096V) ──[R_in = 10k]───┐
                                     ├──[− input of TL072]
  REF3040 (+4.096V) ─[R_off = 20k]──┘
                       │
  [R_fb = 48.7k]───────┘
                       │
                  [output, 1k series]──[BAT54S to ±12V rails]──[jack tip]
  ```
- [ ] Math: `Vout = −(R_fb/R_in) × Vdac + (R_fb/R_off) × V_off` (offset goes to + input via divider, or use a unity-gain follower of −V_off into the summing node — bench-tune the topology).
- [ ] **Practical target:** DAC 0 V → +10 V out; DAC 2.048 V → 0 V out; DAC 4.096 V → −10 V out. (Inverted, scale ±10 V.) Firmware compensates in `voltsToDacCount()`.
  - Gain = R_fb/R_in = 48.7k/10k = **4.87** (was 4 when VREF was 5 V — see `decisions.md` §25)
  - Offset contribution = (R_fb/R_off) × 4.096 V = (48.7/20) × 4.096 ≈ **+9.97 V** at DAC = 0
  - All values are E96-standard; bench-tune to hit exact ±10 V if needed
- [ ] Power: TL072 on ±12V (Story 011 rails).
- [ ] Output protection: **1 kΩ series + BAT54S series-pair Schottky to ±12V rails** at the jack (LCSC C19726, onsemi). One SOT-23 per signal — see `hardware/bom.md`. Caps short-circuit current to ~12V/1k = 12 mA, well within TL072 short-circuit-protected output.
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
- **Why R_fb = 48.7k for gain ≈ 4.88:** with VREF = 4.096 V from REF3040 (per §25), we need slightly more gain than the old 4× to hit the same ±10 V swing. 48.7k is E96-standard; 47k (E24) is close enough for bench, gives ±9.63 V swing. Bench-tune if exact ±10 V is required.
- **Reference voltage:** REF3040 (4.096 V, ±0.2%, 75 ppm/°C) shared with the ADC per `decisions.md` §25 — not the +5V supply rail.
- **Output AC coupling:** not used. V/Oct must be DC. Anyone who wants AC coupling can do it externally.

## Depends on

- Story 011 (rails), Story 012 (SPI working)

## Status

not started
