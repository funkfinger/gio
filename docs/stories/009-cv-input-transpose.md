# Story 009: CV Input for V/Oct Transpose

**As a** patcher
**I want** to patch a V/oct CV signal into J2 and have it transpose the arpeggio
**So that** I can drive root-note motion from another sequencer or modulation source while keeping the encoder for scale, order, and a base root

## Acceptance Criteria

### Hardware

- [ ] Op-amp B of the MCP6002 (currently tied off as a grounded buffer) is repurposed as a unity-gain buffer with a 100 kΩ + 69 kΩ (47 + 22 in series) divider on its non-inverting input
- [ ] Buffer output drives A1 / GP27
- [ ] Standard mono Thonkiconn (or bench jumper) on J2; no jack-presence detection

### Firmware

- [ ] Chord stored as intervals `{0, 4, 7, 12}` from a runtime root, not hardcoded MIDI notes
- [ ] New encoder menu item `ROOT` with 12 values (C, C♯, D, … B) pinned to MIDI octave 3 (MIDI 48..59). Default = C
- [ ] Encoder click order: SCALE → ORDER → ROOT → SCALE
- [ ] NeoPixel adds a third colour: **magenta** for ROOT active (alongside green/Scale, blue/Order)
- [ ] CV path: ADC raw → voltage at J2 (accounting for divider) → semitone-snap → scale-snap (using the active scale via `quantize()`) → added to encoder root
- [ ] α-summing convention: unplugged jack reads ~0 V → 0 transpose → arp plays exactly as encoder set
- [ ] Final played note clamped to MIDI [24, 96] (C1..C7) so DAC never tries to exceed range
- [ ] Existing per-step `quantize(played_note, scale)` from Story 008 retained on top of the transpose

### Tests

- [ ] New host tests for `cv::voltsToTranspose(volts, scale)`:
  - 0 V → 0 semitones (any scale)
  - 1 V → 12 semitones (any scale)
  - 0.5 V (= 6 semis raw) under Major → snap to 5 (F, ties break down)
  - 1.5 V (= 18 semis raw) under Major → 1 octave + 5 = 17
  - Negative volts clamped to 0
  - Volts above 8 V clamped to 8 V = 96 semitones

### Bench

- [ ] Patch a spare pot (3.3 V → wiper → GND) into J2 node. Sweep slowly.
- [ ] Confirm via ear: arp transposes upward as wiper moves toward 3.3 V (~0–~16 semitones over the pot range with the divider).
- [ ] Confirm scale-snap by selecting `Pent Min` scale: transpose values that aren't in PentMin get coerced to the nearest in-scale interval.
- [ ] Confirm encoder ROOT still works (rotate while pot is at 0 → root tracks encoder).
- [ ] Note in `bench-log.md`.

## Notes

- α-summing chosen over β-with-detection after grilling. Reasons in `bench-log.md` (Story 009 entry); short version: detection hardware is fiddly with standard Thonkiconn parts, and Plaits actually uses α-summing despite my earlier mischaracterisation.
- D7 stays spare. Reserved for a future "CV ENABLE" panel toggle if α turns out to feel wrong.
- 47 kΩ + 22 kΩ in series substitutes for the 68 kΩ divider leg (Bourns 68k unavailable on bench). Stack is 69 kΩ — within 1.5 % of spec, better than typical 5 % carbon tolerance.
- This story doesn't add a `cv::` library for the ADC-to-volts conversion since that's tied to Arduino. The pure-logic snap+quantize lives in `lib/cv/`.
- BAT54 dual Schottky clamp at op-amp B's `+IN` is deferred to PCB stage. Bench source is bounded 0..3.3 V (or 0..5 V) so the op-amp inputs stay safe.

## Depends on

- Story 008

## Status

done — bench-confirmed 2026-04-22. MCP6002 op-amp B repurposed as CV buffer. α-summing (encoder ROOT + CV transpose). New `lib/cv/` library with 10 host tests. Menu `ROOT` param added; live effective-root display with `*` suffix when CV contributes; NeoPixel magenta for ROOT active. Hysteresis on CV to kill snap-boundary flutter. See `bench-log.md` for the iteration trail (includes a notable 100 Ω vs 100 kΩ resistor bug caught via serial-diag dump).
