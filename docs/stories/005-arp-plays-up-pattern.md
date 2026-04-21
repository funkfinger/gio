# Story 005: Arp Plays Up Pattern

**As a** patcher
**I want** the module to play a 4-note up-arpeggio (C3–E3–G3–C4) at a fixed tempo with calibrated V/Oct and gate output
**So that** the module makes music for the first time and the complete signal chain is validated end to end

This is the **v0.1.0 milestone**. Everything after this is additive.

## Acceptance Criteria

- [ ] `firmware/arp/lib/arp/arp.h` and `arp.cpp` added (ported from RA4M1, pure C++, no Arduino deps)
- [ ] `firmware/arp/lib/tempo/tempo.h` and `tempo.cpp` added (ported from RA4M1)
- [ ] Host tests for `arp` and `tempo` pass via `pio test -e native` (ported test files)
- [ ] `src/main.cpp` integrates arp + scales + tempo + PWM DAC + gate output:
  - Fixed 120 BPM internal clock (tempo pot not yet wired)
  - Up-order arp (C3, E3, G3, C4) — major triad + octave rooted at C3
  - Non-blocking `millis()` timing: gate on/off and step advance driven by timestamp comparison, not `delay()`
  - Each step: write MIDI note via `analogWrite(PIN_DAC_PWM, count)`, gate on for 50% of step
  - `LED_BUILTIN` flashes on gate on (beat indicator)
- [ ] Scope confirms: 4-step V/Oct staircase at 120 BPM (500 ms/step), 50% gate duty
- [ ] Audio confirms: audible 4-note up-arpeggio through Plaits or equivalent VCO + envelope
- [ ] Scope screenshots and audio recording in `requirements/`
- [ ] Bench notes added to `docs/bench-log.md`
- [ ] `arp/v0.1.0` tag created on merge

## Notes

- `lib/arp/` and `lib/tempo/` code and tests port verbatim from `xiao-ra4m1-arp`. Confirm they compile cleanly under the arduino-pico / RP2350 toolchain.
- `noteToDAC()` calls `analogWrite(PIN_DAC_PWM, dacCount)` — same call as the RA4M1's `analogWrite`, different pin.
- Gate logic: NPN common-emitter inverts — `digitalWrite(PIN_GATE, LOW)` = gate HIGH (5V at J4).
- At this stage: major scale only, fixed BPM, no encoder, no OLED, no pot reads, no CV in.

## Depends on

- Story 004

## Status

not started
