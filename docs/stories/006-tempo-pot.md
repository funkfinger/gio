# Story 006: Tempo Pot

**As a** patcher
**I want** to turn the tempo pot and have the arp speed up and slow down in real time
**So that** I can set the tempo by feel without menus

## Acceptance Criteria

- [ ] `lib/tempo/tempo.h/.cpp` already exists (Story 005) — no changes needed if ported
- [ ] `src/main.cpp` reads `analogRead(PIN_TEMPO)` each loop iteration, maps to BPM via `bpmFromPot()`
- [ ] Tempo range: 40–300 BPM, exponential curve (every 1/3 of pot rotation doubles BPM)
- [ ] At BPM=120: step period = 125 ms (16th note); gate = 62.5 ms. Scope-verify timing.
- [ ] Turning pot from min to max sweeps audibly from slow crawl to fast 16th notes
- [ ] Host tests for `bpmFromPot` and `stepMsFromBpm` pass (already covered by ported tempo tests)

## Notes

- The tempo library is already ported and tested in Story 005. This story is just wiring `analogRead` into the main loop.
- The RP2350 ADC is 12-bit (0–4095 at `analogReadResolution(12)`). The `bpmFromPot` function takes a normalised float [0.0, 1.0] — divide raw by 4095.
- Pot wiring: CCW=GND, wiper=D0/A0, CW=3V3.

## Depends on

- Story 005

## Status

not started
