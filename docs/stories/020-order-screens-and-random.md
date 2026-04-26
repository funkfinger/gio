# Story 020: Order Screens + Random + UpDownClosed

**As a** patcher
**I want** the OLED to show a graphical bar-chart of each arp order's playback pattern (with random-bars animation when in Random mode)
**So that** I can see at a glance what the arp is doing — including a clear visual cue when randomness is in play

## Engine work

- **Renames** (semantic clarification, no behaviour change for existing code):
  - `ArpOrder::UpDown` → `ArpOrder::UpDownOpen` (palindrome WITHOUT endpoint repeat — `0,1,2,3,2,1`)
  - `ArpOrder::Skip` → `ArpOrder::SkipUp` (forward-looking for a future `SkipDown` and other finger-picking variants)
- **New variants**:
  - `ArpOrder::UpDownClosed` — palindrome WITH endpoint repeat (`0,1,2,3,3,2,1,0`, period `2N`). Both endpoints doubled — top consecutively in the middle of one cycle, bottom across the loop boundary. See `docs/decisions.md` §24 for the open/closed terminology.
  - `ArpOrder::Random` — pure random index per step (with replacement). Uses `std::rand()`; seeded in `setup()` from `millis()` ⊕ noisy `analogRead(PIN_TEMPO)`.
- **Encoder cycle**: `Up → Down → UpDownClosed → UpDownOpen → SkipUp → Random → Up` (matches Aseprite tag order in `assets/screens/order-screen/order.aseprite`).

## UI work

- **Static art** for 5 orders, drawn in Aseprite as bar-chart visualizations of each playback pattern (one bar per note, height = pitch, position = playback order). Tagged `up`, `down`, `up-down-closed`, `up-down-open`, `skip-up` → auto-exported as `order-up.png`, etc., by `tools/aseprite-export.js` → `screens::order_up`, etc.
- **Procedural Random** — when `ArpOrder::Random` is selected, instead of drawing a static bitmap, `drawRandomOrderScreen()` paints 8 vertical bars at random heights across the 32-wide canvas. Called:
  - From `renderMenu()` when ORDER becomes the active param
  - From `fireStep()` on every step advance, ONLY when `active == Param::Order && order == Random` (so the bars pulse with each note when the user is looking at them)
- **Fall-through** to text rendering remains for any order without art.

## Tests

- All existing arp tests renamed (`UpDown` → `UpDownOpen`, `Skip` → `SkipUp`).
- New tests:
  - `UpDownClosedPalindromicWithRepeatEndpoints` — verifies the `0,1,2,3,3,2,1,0` sequence
  - `UpDownClosedTwoNotesSafe` — `count=2` → `0,1,1,0,…` (period 4)
  - `UpDownClosedRepeatsTopAndBottomAcrossLoop` — counts endpoint hits over two cycles
  - `RandomReturnsAllValidIndicesOverManyCalls` — distribution sanity
  - `RandomDistributionIsRoughlyUniform` — ±20% slack across 4000 samples
  - `RandomDeterministicWhenSeeded` — same `srand` seed → same sequence
- Total host tests: 60 → 66, all green.

## Future work (deferred)

- **Highlight the active bar.** Each bar in the diagram represents a note in the chord. When a note plays, brighten/invert that bar (~one-step pulse) so the user sees which position in the order is currently sounding. Direct visual mapping of "we're playing this note now."
- **More finger-picking patterns.** `SkipDown`, `Roll`, etc. — the `SkipUp` rename anticipates this naming family.

## Acceptance criteria

### Firmware
- [x] Engine renames + additions, all tests pass on host
- [x] `orderName()` updated to match new enum (UI strings: `Up`, `Down`, `UpDn-C`, `UpDn-O`, `SkipUp`, `Random`)
- [x] Procedural random-bars renderer implemented and called per-step + on menu activation
- [x] `std::srand()` seeded in `setup()` from `millis()` ⊕ `analogRead(PIN_TEMPO)`
- [x] Forward-decl for `drawRandomOrderScreen()` added so `renderMenu()` (defined earlier) can call it

### Bench
- [ ] Cycle through ORDER param: each of the 5 static orders shows the corresponding bar-chart bitmap
- [ ] Land on Random: bars are visible and re-randomize on every step (audible note + visual jump in lockstep)
- [ ] Random pitch sequence audibly varies (no obvious repetition over many cycles)
- [ ] UpDownClosed audibly doubles the top note (you hear the same pitch twice in a row at the top of each cycle)

## Status

implemented + tests green; bench verification pending flash.

## Depends on

- Story 019 (TRIG menu) for the encoder-cycle pattern
- Aseprite auto-export pipeline (per `assets/screens/README.md`)
