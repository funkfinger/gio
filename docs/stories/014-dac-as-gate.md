# Story 014: Verify DAC Output as Gate / Trigger

**As a** bench engineer
**I want** to confirm that a DAC channel driven as a 0V/+5V square wave produces edges fast enough and clean enough to act as a real Eurorack gate
**So that** we can drop the dedicated NPN gate-driver hardware and have all 4 jacks be symmetric (any output = CV or trigger)

## Acceptance Criteria

### Firmware

- [ ] Helper: `outputs.gate(channel, on)` — calls `outputs.write(channel, on ? 5.0f : 0.0f)`. (Just a thin wrapper.)
- [ ] Test program: drive ch B as a 1 kHz square wave (500 µs high / 500 µs low). Use the loop's `millis()` for low-rate, `delayMicroseconds()` for fast edges if needed.

### Bench

- [ ] **Edge time on scope:** measure 10–90% rise and fall time of a 0V→+5V transition. Expect **< 50 µs** based on DAC8552 settling (4 µs) + TL072 slew (13 V/µs × 5 V = 0.4 µs). If observed is > 100 µs, investigate (op-amp compensation cap? scope probe loading?).
- [ ] **No overshoot or ringing** on the rising edge (would indicate output-stage instability). If present, add a small (10 pF) cap across R_fb in the op-amp stage.
- [ ] **Clean threshold crossing:** scope the output crossing 1.5V (typical Eurorack trigger threshold) — must be monotonic, no glitch, no ripple from the DAC switching.
- [ ] **Patch into a real Eurorack module's gate input** (envelope generator preferred; trigger sequencer also fine). Confirm:
  - Triggers reliably on every pulse
  - No double-fires (would indicate edge isn't monotonic enough)
  - Works at 1 Hz, 10 Hz, 100 Hz, 1 kHz pulse rates
- [ ] **Patch into a clock-divider module** that's known to be edge-sensitive (e.g. Pamela's, μClock). Confirm it counts 1:1 with our output.

## Notes

- This story exists to **validate an architectural assumption**: that we can use the DAC for both CV and triggers. If it fails (edges too slow, downstream modules unhappy), we add a Schmitt buffer (74HC14 or SN74HCT14N — both in stock) between the op-amp output and the jack on whichever channels are likely to be used as triggers. **That fallback exists; we just don't want to take it unless we have to.**
- TL072 slew rate is 13 V/µs typical, 8 V/µs minimum. Even at minimum, a 5V swing takes < 1 µs. The DAC8552 settling time of 4 µs dominates.
- If the user wants to use the output as an audio-rate signal source (clock at thousands of Hz, or LFO at 50–100 Hz), edge speed will matter more. For "gate" use (10–100 Hz typical) this is overkill.

## Depends on

- Story 013 (DAC output stage built and calibrated)

## Status

not started
