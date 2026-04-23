# Story 016: Trigger Detection on Buffered ADC Inputs

**As a** bench engineer
**I want** firmware that reliably detects rising edges on any buffered ADC input
**So that** any input jack can be used as a trigger/clock input — same hardware, firmware-selectable mode

## Acceptance Criteria

### Firmware

- [ ] New library `firmware/arp/lib/trigger_in/`:
  - `Schmitt::poll(volts)` — feed it the latest jack voltage; returns `true` exactly once per rising edge
  - Internal state: `bool armed`; `armed=true` while signal is below low threshold; on cross-above-high, returns `true` and sets `armed=false`; resets when signal drops below low threshold again
  - Configurable thresholds: default **high = +1.5V** (standard Eurorack trigger threshold), **low = +0.5V** (1V hysteresis kills noise/ripple)
- [ ] No debounce timer — the Schmitt hysteresis IS the debounce. (10ms debounce timer kept Story 010 honest; here the analog hysteresis does the same job at the analog stage.)
- [ ] Polling: `poll()` called on every loop iteration. ADC read latency (~20 µs at 4 MHz SPI) sets the practical detection ceiling. At 1 ms loop budget, we can resolve edges down to ~1 ms apart = 1 kHz max trigger rate. Comfortable.
- [ ] Pure-logic; can be host-tested. Add `test_trigger_in.cpp` to the native test suite.

### Bench

- [ ] **Clock-mod2 source:** drive J1 from the MOD2 clock (0–10V square waves at 2–20 Hz). Confirm reliable detection at all rates, no double-fires.
- [ ] **Slow ramp test:** apply a 10-second 0V → +5V → 0V triangle to J1. Schmitt should fire exactly once on the way up, exactly once on the way down (no, zero times on the way down — only rising edges matter). Verify with serial output count.
- [ ] **Noisy signal test:** apply a +3V DC bias plus 200 mV peak-to-peak noise (bench function generator). Confirm zero false triggers (signal stays above the high threshold continuously, but within the noise floor).
- [ ] **Overlap with CV reading:** in firmware, simultaneously read the ADC value as continuous CV *and* run it through the Schmitt. Confirm both work together (same channel can serve as a triggered CV source — Mutable's "trigger transposes the held CV" pattern).

### Host tests

- [ ] `Schmitt.RisingEdgeFiresOnce` — feed pre-recorded voltage sequence, count edges
- [ ] `Schmitt.HysteresisRejectsRippleNearThreshold` — voltage hovers just above high threshold with ±100 mV ripple → exactly one edge
- [ ] `Schmitt.ResetsBelowLowThreshold` — must drop below 0.5V before next edge can fire
- [ ] `Schmitt.SlowRampDoesNotMultiTrigger` — monotonic rise from 0 to +5V → exactly one edge

## Notes

- **Why no hardware Schmitt comparator (e.g. LM393, 74HC14):** the buffered op-amp output already gives us a clean signal at the ADC; the analog hysteresis built into the firmware Schmitt is sufficient and saves a part. If high-rate audio-rate trigger detection is ever needed (> 1 kHz), revisit.
- **Threshold rationale:** Eurorack convention is "trigger" ≥ +1V on the rising edge; +1.5V is conservative. Some modules use +2.5V or +2.7V (TTL-ish). 1V hysteresis is generous.
- **Future:** for "platform" apps that want different threshold behavior (e.g. envelope follower, peak detector, beat detector), the Schmitt is one option among many — apps can also just read continuous CV via Story 015 and roll their own logic.

## Depends on

- Story 015 (input protection + buffer working)

## Status

not started
