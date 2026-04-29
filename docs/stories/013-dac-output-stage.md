# Story 013: DAC Output Stage — Bipolar ±10V Swing via TL072

**As a** bench engineer
**I want** each DAC channel buffered through a TL072 op-amp stage that can swing the full Eurorack ±10V range
**So that** outputs can serve as V/Oct (0–8V positive), bipolar CV (±5V or ±10V), or fast gates (0/+5V) — all from the same circuit

## Acceptance Criteria

### Hardware

- [ ] Per-output topology — **inverting amplifier with non-inverting offset** (corrected from initial spec — see Notes):
  ```
  DAC (0..+4.096V) ──[R_in = 10k]── pin 2 (1IN−)
                                    │            ╲
                       [R_fb = 48.7k]            ─── pin 1 (1OUT)
                                    │            ╱       │
                              pin 1 ─┘                   │
                                                         │
  REF3040 (+4.096V) ─[R_div_top = 22k]── pin 3 (1IN+)    │
                  GND ─[R_div_bot = 15k]┘                │
                                                         │
                              [output, 1k series]──[BAT54S to ±12V rails]──[jack tip]
  ```
- [ ] Math: `Vout = (1 + R_fb/R_in) × V_pin3 − (R_fb/R_in) × VDAC`, where `V_pin3 = VREF × R_div_bot/(R_div_top + R_div_bot) ≈ 1.66 V`.
- [ ] **Practical target:** DAC 0 V → +9 V out; DAC 2.048 V → 0 V out; DAC 4.096 V → −9 V out. (Inverted, scale ±9 V with these E96 values.) Firmware compensates in `outputs::write()`.
  - Gain = R_fb/R_in = 48.7k/10k = **4.87**
  - V_pin3 contribution at output = (1 + 4.87) × 1.66 ≈ **+9.74 V** at DAC = 0
  - For exact ±10 V target: bench-tune R_div_top / R_div_bot to land V_pin3 right; or accept ±9 V swing (more than enough for V/Oct, plenty of margin on bipolar CV)
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

- **Topology correction (2026-04-29):** the original spec called for an inverting *summing* amp with VREF on R_off feeding pin 2 alongside VDAC. That doesn't work with a single positive reference — both contributions invert, output rails to negative. Bench-validated and corrected; offset now goes to **pin 3 (non-inverting input)** via a divider, making the offset contribution non-inverting (positive at output) while VDAC stays inverting (negative-going). See `bench-log.md` 2026-04-29 entry.
- **Why inverting topology at all:** DAC8552 is single-supply 0..VREF; getting bipolar requires combining a fixed positive offset with an inverted DAC contribution. The corrected topology is the minimum-parts way (one R_in, one R_fb, two divider resistors). The inversion is paid for in firmware (`dac_count = voltsToCount(−v + offset)`) — trivial.
- **Why R_fb = 48.7k for gain ≈ 4.87:** with VREF = 4.096 V from REF3040 (per §25), the gain that takes a 4.096 V max DAC swing to a ~10 V output range is ~4.87. 48.7 k is E96-standard; 47 k E24 is close enough; 2× 22 k in series (= 44 k, gives gain 4.4) was used for first bench validation and produces ±9 V — more than acceptable.
- **Reference voltage:** REF3040 (4.096 V, ±0.2%, 75 ppm/°C) shared with the ADC per `decisions.md` §25 — not the +5V supply rail.
- **Output AC coupling:** not used. V/Oct must be DC. Anyone who wants AC coupling can do it externally.

## Depends on

- Story 011 (rails), Story 012 (SPI working)

## Status

not started
