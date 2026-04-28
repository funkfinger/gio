# Story 011: Power Rails — ±12V, +5V, +3.3V Bench Stack

**As a** bench engineer
**I want** clean ±12V / +5V / +3.3V rails on the breadboard, mirroring what the Rev 0.1 PCB will eventually provide
**So that** every subsequent platform-rebuild story (DAC, ADC, op-amps, XIAO, OLED) starts from the same known-good supply

## Hivemind Protomato (this build)

Bench rails are provided by a [Hivemind Protomato](https://hivemindsynthesis.com/shop/p/protomato-16hp-eurorack-diy-circuit-creation-platform-kit) — a 16HP Eurorack DIY breadboarding platform that brings ±12V (and a regulated +5V) onto breadboard tie-points with reverse-polarity protection already baked in. This collapses Story 011 from "build a power supply" to "verify the Protomato gives us what we need."

The bespoke regulator + reverse-protection circuit described in the original criteria is what the Rev 0.1 PCB will need; it's documented here for future reference but not bench-built now.

## Acceptance Criteria

### Bench (Protomato verification)

- [ ] Plug Protomato into Eurorack PSU. Multimeter at its breadboard rails:
  - **+12.0 V ± 0.5 V**
  - **−12.0 V ± 0.5 V**
  - **+5.0 V ± 0.1 V** (if Protomato regulates locally — confirm from kit docs)
  - GND continuity across all GND tie-points
- [ ] Scope each rail under typical load (XIAO + DAC + ADC + 1 op-amp running). **Ripple < 50 mV peak-to-peak.** If Protomato's onboard 5V reg is noisy, fall back to bench-built LM7805 from +12V.
- [ ] Confirm the Protomato's reverse-polarity protection covers a flipped Eurorack header (per kit docs — don't actually flip it unless docs claim safety).
- [ ] Document the Protomato pinout / tie-point mapping in a bench-log entry so future stories know where to clip leads.

### For Rev 0.1 PCB (deferred — not bench-built here)

The PCB will need its own power section, since the Protomato won't be onboard. The 10-pin Doepfer Eurorack header carries ±12 V + GND only — no +5 V — so the board must generate +5 V locally from +12 V.

Reference design captured for the schematic:
- Eurorack 2×5 shrouded header, Doepfer pinout (red stripe = −12 V).
- Reverse-polarity series Schottky on +12 V (Rev 0.1 PCB-only — Protomato handles this on the bench).
- **+5 V rail: AMS1117-5.0 (LCSC C6187, JLC Basic, SOT-223)** — 1 A LDO from +12 V. ~1.3 V dropout is fine with 7 V of headroom. Per-board cost negligible (~$0.12) and no JLC feeder fee. See `hardware/bom.md`.
- ±12 V routed directly to TL072s (no regulation). +5 V to AMS1117 → DAC8552 VDD / MCP3208 VDD / XIAO 5V-pin (XIAO regulates internally to 3.3 V).
- **VREF (precision 4.096 V): REF3040AIDBZR (LCSC C19415, TI, SOT-23)** — separate from the +5 V supply rail per `decisions.md` §25. Both DAC and ADC tie their VREF pins to this.
- 100 nF ceramic at every IC supply pin; 10 µF bulk per rail per IC cluster; **600 Ω@100 MHz ferrite bead (GZ2012D601TF, LCSC C1017, JLC Basic) on each ±12 V rail** before the analog cluster.
- Common ground star point.

## Notes

- The +5V rail powers VDD only — VREF is generated separately by a REF3040 (4.096 V, ±0.2%) per `decisions.md` §25. The LDO's accuracy and noise have no direct path to V/Oct precision. For bench bring-up the Protomato's +5V is fine for VDD; the REF3040 is added on a small breakout for the precision-reference role.
- We have LM7905 (−5V) in stock if a future story needs a regulated negative low-voltage rail. Not needed for this stack.
- Eurorack power draw budget for this module (estimated): +12V ~80 mA (XIAO + DAC + ADC + LEDs), −12V ~10 mA (op-amps only), +5V via Eurorack power bus is **not used** — we generate +5V locally from +12V to avoid bus contention.

## Depends on

- Nothing. This is the foundation.

## Status

reduced to verification — Protomato kit handles bench rails; Rev 0.1 PCB power section captured as reference for future schematic work.
