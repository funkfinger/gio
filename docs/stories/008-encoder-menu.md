# Story 008: Encoder Menu — Scale and Arp Order

**As a** patcher
**I want** to rotate the encoder to change scale and arp order, with the OLED showing the current selection
**So that** I can navigate the module's parameters without pots or fiddly boundary zones

## Acceptance Criteria

- [ ] Encoder click cycles through parameters: Scale → Arp Order → (back to Scale)
- [ ] Encoder rotation changes the active parameter's value (wraps at boundaries)
- [ ] OLED shows: parameter name on line 1, current value name on line 2 (e.g. "SCALE" / "Pent Min")
- [ ] NeoPixel colour changes to indicate which parameter is active (green=scale, blue=order)
- [ ] Scale changes take effect immediately on the next arp step
- [ ] Order changes restart the arp pattern from step 0
- [ ] Long encoder press (>500 ms) resets arp to step 0 and flashes OLED "RESET"
- [ ] Host tests for `arp::orderFromPot` → `setOrder()` integration still pass (no regression)

## Notes

- The `scaleFromPot` / `orderFromPot` functions from the RA4M1 project are no longer the primary input path — encoder delta is. Those functions remain in `lib/scales/` and `lib/arp/` and may be repurposed for CV-in → parameter mapping later.
- Arp orders: Up, Down, UpDown, DownUp, Skip, Random (6 total, same as RA4M1).
- Scales: Major, Natural Minor, Pentatonic Major, Pentatonic Minor, Dorian, Chromatic (6 total).
- NeoPixel: use `neopixelWrite()` or `Adafruit_NeoPixel` with `PIN_NEOPIXEL`.
- Menu state machine: `current_parameter` enum, encoder delta updates value, click advances parameter.

## Depends on

- Story 007

## Status

not started
