# Story 003: PWM DAC Ramp and Scale Quantiser

**As a** bench engineer and developer
**I want** (a) the RP2350 PWM output + 2-pole RC filter to emit a visually clean linear voltage ramp, and (b) a pure-logic scale quantiser with host unit tests
**So that** the key signal path (PWM → filter → op-amp → V/Oct) is validated in isolation, and the first piece of musical logic exists with TDD scaffolding in place

Bundled in one PR because they share the same milestone: "novel subsystems proved, on their own terms."

## Acceptance Criteria — PWM DAC Ramp (bench)

- [ ] **Toolchain check:** confirm the pinned `arduino-pico` platform (see `platformio.ini`) exposes the per-pin form `analogWriteFreq(pin, freq)`. A one-line sanity build (e.g. `analogWriteFreq(PIN_DAC_PWM, 36621);` in `setup()`) must compile without fallback to the global `analogWriteFreq(freq)` overload. If compile fails, bump the platform pin intentionally and re-run all PWM DAC bench steps — do NOT silently fall back to global frequency (would change timing for any other PWM slice later).
- [ ] D2 (GP28) configured as 12-bit PWM output: `analogWriteResolution(12)`, `analogWriteFreq(PIN_DAC_PWM, 36621)` (or closest to 150 MHz / 4096)
- [ ] Firmware sweeps PWM count 0→4095 in a loop at ~10 Hz
- [ ] **Raw PWM pin (before filter):** Rigol DS1054Z shows square wave at ~36.6 kHz, duty sweeping 0%→100%, amplitude 0–3.3V
- [ ] **After 1-pole filter (R_f1 + C_f1 only):** scope shows sawtooth ramp 0→3.3V with visible ripple — confirm ripple frequency matches PWM frequency
- [ ] **After 2-pole filter (R_f1+C_f1+R_f2+C_f2):** scope shows clean 0→3.3V ramp, ripple not visible at 50 mV/div — confirm < 5 mV ripple
- [ ] Check for ADC noise coupling: read `analogRead(A0)` (tempo pot, unwired or grounded) while PWM is running — confirm ADC readings are stable (< ±5 counts / 4095)
- [ ] All measurements recorded in `docs/bench-log.md` with scope screenshots in `requirements/`

## Acceptance Criteria — Scale Quantiser (host TDD)

- [ ] `firmware/arp/lib/scales/scales.h` and `scales.cpp` added — pure C++, no `<Arduino.h>`
- [ ] `uint8_t quantize(uint8_t midiNote, Scale scale)` — nearest in-scale note; ties break downward
- [ ] All 6 scales defined: Major, Natural Minor, Pentatonic Major, Pentatonic Minor, Dorian, Chromatic
- [ ] `Scale scaleFromPot(float pot, Scale current)` with ±2% hysteresis (reused from RA4M1)
- [ ] Tests under `firmware/arp/test/test_scales/` pass via `pio test -e native`:
  - Each scale's in-scale notes pass through unchanged
  - Out-of-scale notes map to nearest in-scale tone (ties break downward)
  - Chromatic is identity for all 128 MIDI notes
  - Octave invariance: `quantize(n, s) + 12 == quantize(n + 12, s)` for all n in range
  - `scaleFromPot` zone centres, hysteresis, sweep visits all 6 in order
- [ ] CI runs `pio test -e native` and stays green

## Notes

- The `scales` library code and tests port verbatim from `xiao-ra4m1-arp`. Goal: confirm they compile and pass in this PlatformIO env.
- `analogWriteFreq(pin, freq)` in arduino-pico sets the PWM frequency for a specific pin's slice. Set it once in `setup()` before the first `analogWrite()`. Exact frequency depends on clock divider resolution — 36,621 Hz is achievable; the exact value doesn't matter much since GAIN is calibrated empirically anyway. The per-pin form matters because we do not want to globally set PWM frequency for other slices that may drive unrelated peripherals later (e.g. NeoPixel, gate shaping). See `decisions.md` §11 for the platform version pin rationale.
- GoogleTest requires an explicit `main()` in each test file.
- The op-amp scaling stage is not added in this story — the filter output feeds the scope directly. Op-amp integration is Story 004.
- If ADC noise coupling IS observed on bench, document it. Mitigation options: add a small cap to GND on ADC input pin, increase ADC averaging in firmware, or change PWM frequency. Do not try to fix it in breadboard layout — note it as a PCB routing constraint.

## Depends on

- Story 002

## Status

not started
