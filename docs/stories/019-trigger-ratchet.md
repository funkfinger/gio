# Story 019: Trigger Ratcheting (TRIG menu)

**As a** patcher
**I want** the gate output to fire 1, 2, 3, or 4 times per arp note (selectable from the encoder menu)
**So that** I can add musical variation — rolls, ratchets, and fast subdivisions — without re-patching anything

## Behaviour

Each arp step still produces **one pitch.** Within that step, the gate fires N evenly-spaced times instead of once. Each sub-gate is `(stepMs / N) * 0.5` long (50% duty per sub-step).

| TRIG | Gates per note | Musical character |
|---|---|---|
| 1 | One (current behaviour) | Standard arp |
| 2 | Two | Note repeats once mid-step |
| 3 | Three | Triplet roll |
| 4 | Four | Fast roll / 16ths-inside-quarter |

At 120 BPM 16ths (`stepMs = 125 ms`), TRIG 4 = each gate ~16 ms long — well above any module's trigger threshold. Sub-step floor of 5 ms prevents pathological behaviour at very fast tempos.

## Menu Integration

- **Encoder cycle:** `SCALE → ORDER → ROOT → TRIG → SCALE`
- **NeoPixel** colour for TRIG: yellow/amber (joins green/Scale, blue/Order, magenta/Root)
- **OLED** shows `TRIG` and the digit (1, 2, 3, 4)
- Long-press still resets the arp; no override needed

## External-clock interaction

Ratcheting composes with the existing ×1/×2/×4 multiplier:

- **Multiplier** = how many notes per external pulse (note advancement scope)
- **TRIG** = how many gates per note (within-note scope)

So external clock + multiplier ×2 + TRIG 3 = **6 gates per incoming pulse.** No special-case logic needed — the sub-gate scheduler runs identically in both clock modes; only the source of `noteMs` changes (`stepMs` from pot vs. `extInterpolatedInterval` from measured period).

## Implementation

- New globals in `firmware/arp/src/main.cpp`: `ratchet`, `subStepMs`, `subGateMs`, `lastSubGateMs`, `subGatesFired`
- Removed: `gateMs` (replaced by `subGateMs` — at ratchet=1 the value is identical, so legacy timing is preserved)
- New helper `recalcSubTiming(noteMs)` — central place that maps a note length + ratchet to per-gate timing. Called from `refreshTempoFromPot()`, the external-clock branch of `fireStep()`, and the encoder rotation handler.
- New helper `fireSubGate()` — raises gate without changing DAC. Called from the loop when more sub-gates remain in the current note.
- Loop changes:
  - Gate-off check now uses `lastSubGateMs` and `subGateMs`
  - New: sub-gate fire check between gate-off and clock-source selection — fires if `subGatesFired < ratchet` and one `subStepMs` has elapsed since the last sub-gate's start

## Acceptance criteria

### Firmware

- [x] `Param::Trigger` added to menu; encoder cycles include it
- [x] Encoder rotation changes `ratchet` in 1..4 with wrap-around
- [x] OLED shows `TRIG` and the digit; live update on rotate
- [x] NeoPixel turns yellow when TRIG is the active param
- [x] `recalcSubTiming()` called whenever ratchet or note length changes
- [x] Sub-gate floor of 5 ms prevents stranded gates at extreme tempos

### Bench

- [x] Ratcheting audibly works at TRIG 2/3/4 — bench-verified 2026-04-24.
- [ ] Re-verify edge behaviour (mid-note encoder change, external-clock composition) on the soldered platform build (Stories 011–017). Skipping on the breadboard since unrelated octave-jitter from intermittent contacts makes "did the gate behave" hard to isolate from "did the pitch hold steady."

## Future stories

- **020 (deferred):** Random ratchet — probability-weighted variation in trigger count per step (e.g. 50% chance of 2x, 25% chance of 3x, 25% straight). Same hardware, separate menu param or "random mode" toggle.

## Status

done — bench-confirmed 2026-04-24. Octave-jitter observed during the session attributed to loose breadboard contacts (V/oct path), not firmware; will confirm clean on the soldered Rev 0.1 build.

## Depends on

- `arp/v0.3.0` (encoder menu, NeoPixel, OLED rendering already in place)
