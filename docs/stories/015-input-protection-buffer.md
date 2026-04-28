# Story 015: MI-Style Input Protection + TL072 Buffer

**As a** bench engineer
**I want** each input jack to handle ±15V abuse safely and present a buffered, scaled signal to the MCP3208 ADC
**So that** every input is a "platform input" — accepts any standard Eurorack signal (0/+5V gate, 0–10V CV, ±10V LFO, V/Oct), readable as a 12-bit count

## Acceptance Criteria

### Hardware

- [ ] Per-input topology — **inverting summing amplifier** (mirrors the output stage in Story 013, in reverse):
  ```
  Jack tip ──[R_in = 100k]──┬──[BAT54S to ±12V rails]──┐
                             │   (one SOT-23 per signal)
                             │
                         [node A]──[R_in2 = 22k]───┐
                                                    ├──[− input of TL072]
            REF3040 (+4.096V) ──[R_off = 9.4k]──┘
                                            │
                            [R_fb = 4.7k]───┘
                                            │
                                [output 0..4.096V] → MCP3208 channel
  ```
- [ ] Mapping target: jack **+10 V → ADC 0 V**; **0 V → ADC 2.048 V**; **−10 V → ADC 4.096 V**. (Inverted; firmware compensates.) Resolution: 1.00 mV per ADC count → 4.88 mV per LSB at the jack (exactly half the count granularity since gain is ~0.205).
- [ ] **First-stage protection** is the 100 kΩ series + BAT54S series-pair Schottky to ±12 V rails (LCSC C19726, onsemi). Worst-case clamp current at ±15 V: 3 V / 100k = 30 µA — well within BAT54's 200 mA rating with a billion-fold safety margin.
- [ ] **Second-stage scaling** is the inverting summing amp with REF3040 (4.096 V) as the offset source — same reference the ADC uses, so any drift cancels. Gain = R_fb/R_in2 ≈ 4.7k/22k ≈ 0.214; bench-tune R_in2 (try 22.6k E96) to hit exact mapping.
- [ ] Both inputs (J1, J2) get identical circuits — symmetric platform behaviour.

### Firmware

- [ ] Helper: `inputs.readVolts(channel)` — takes channel index, calls `adc.read()`, applies inverse mapping (`(2.048 − count_volts) × ~4.88` → input volts at jack, with VREF = 4.096 V), returns float volts.
- [ ] Per-input calibration constants (`gain`, `offset`) — bench-fill from known input voltages.

### Bench

- [ ] **Survival test:** apply +15V then −15V to each jack with a bench supply. Confirm no smoke, no current spike on the rails, and that the input still reads correctly afterward.
- [ ] **DC accuracy:** apply known voltages (−10, −5, 0, +5, +10 V) to each jack via bench supply. Read via `inputs.readVolts()`. Each must be within **±10 mV** of applied (1 LSB ≈ 5 mV at jack scale).
- [ ] **V/Oct round-trip:** drive a known V/Oct from another module (or our own DAC output via Story 013) into the input; firmware reads and quantises to nearest semitone; output via OLED. Confirm correct semitones across 0–8V.
- [ ] **Input impedance:** measured > 80 kΩ at the jack (the 100 kΩ series resistor sets this). High enough not to load typical Eurorack outputs.

## Notes

- **Why op-amp buffering:** the MCP3208's sample-and-hold cap (~20 pF) wants a low-impedance source (datasheet recommends < 10 kΩ source impedance for accurate conversion). Our 100 kΩ first stage would cause settling errors without the op-amp buffer.
- **Why the inverting topology:** lets us use a single positive offset reference (+5V) to bias the bipolar input into the unipolar ADC range. The non-inverting alternative needs a virtual-ground op-amp setup, more parts.
- **Quad op-amp consolidation:** with two outputs (Story 013) and two inputs (this story) all needing op-amp channels, that's 4 op-amps = **one TL074 quad** (in stock, Cab-3/Bin-32) instead of two TL072s. Decide at bench based on physical layout convenience.
- **Trigger-mode reading:** Story 016 builds on this — same hardware, just firmware doing edge detection on the ADC reading instead of treating it as continuous CV.

## Depends on

- Story 011 (rails), Story 012 (SPI working)

## Status

not started
