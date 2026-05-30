# gio-stacked

A 4 HP Eurorack module — three-board stacked redesign of `hardware/gio/`.

## Status

**Planning / scaffolding.** No KiCad files yet. This directory captures the
architecture decisions before schematic work begins. Each subdirectory
(`power/`, `signal/`, `faceplate/`) will become its own KiCad project in a
subsequent session.

## Why split into three boards

The original `hardware/gio/` is a single 4 HP PCB. That layout was tight
but workable for the v0.1 feature set. The expanded feature set for this
revision — 6 jacks (was 4), tap-tempo button, same OLED + pot + encoder —
runs out of PCB area on a single-board 4 HP design once you account for the
through-hole jacks, the OLED breakout footprint, the encoder + pot bodies,
and the Eurorack power header + bulk caps + regulators.

Three stacked boards solve this by trading depth for area:

| Board | Carries |
|---|---|
| **power/** | Eurorack ±12 V header + reverse-polarity + bulk caps + +5 V LDO + REF3040 (4.096 V reference) |
| **signal/** | XIAO RP2350 + DAC8552 + MCP3208 + TL072 input/output op-amp stages + NeoPixel |
| **faceplate/** | 6× Thonkiconn jacks + PEC11 encoder + Alpha 9 mm pot + SSD1306 OLED + tactile button + panel silkscreen |

The faceplate is a real PCB (not just a milled aluminum panel) — it carries
the jacks, OLED breakout footprint, and button, and connects to the signal
board through a single 2.54 mm pin header.

## Inheritance from `hardware/gio/`

Direct re-use (schematic blocks, footprints, calibration):

- **MCU** — Seeed XIAO RP2350
- **DAC** — TI DAC8552 (12-bit, dual, SPI)
- **ADC** — MCP3208 (12-bit, 8-channel, SPI). Now uses CH3 + CH4 for the two new CV ins.
- **Reference** — TI REF3040 (4.096 V, 0.2 %)
- **Op-amps** — TL072 × 2 (±12 V rails). Same input scaling stage (100 kΩ + 22 kΩ + BAT54S clamp + 22 kΩ feedback) replicated 4× for the four CV ins. Same output stage (44 kΩ feedback + 1 kΩ jack series + BAT54S clamp) × 2 for the two outs.
- **Calibration** — copy the bench-fit constants from `firmware/arp/src/main.cpp`
  (channel-symmetric op-amp stages — see `bench-log.md` 2026-05-21).

New for this revision:

- **2 additional CV inputs** (J5, J6) — same scaling stage as J1/J2, routed to MCP3208 CH3 + CH4.
- **Tap-tempo / reset button** — tactile switch on faceplate, wired to XIAO D0 (currently free; was the analog pot pin pre-pivot).
- **Inter-board connectors** — 2.54 mm pin headers (see `docs/interconnect.md`).
- **Mechanical stack** — M2.5 standoffs at 4 corners (see `docs/stackup.md`).

## Panel layout target (4 HP × 128.5 mm)

Top to bottom:

```
┌────────────┐
│   OLED     │  64×32 SSD1306
├────────────┤
│  encoder   │  PEC11 + click
├────────────┤
│    pot     │  Alpha 9 mm
├────────────┤
│   button   │  6×6 mm tactile (tap-tempo)
├────────────┤
│  J1    J2  │  CV in 1   CV in 2 (clock in / V/oct)
│  J3    J4  │  CV in 3   CV in 4 (new — modulation dests)
│  J5    J6  │  CV out    Trigger out (V/oct, gate)
└────────────┘
```

Two columns of 3 jacks fit in 4 HP with 6 mm Thonkiconn nuts at ~7 mm
center-to-center pitch. Final positions in `docs/faceplate-layout.md`.

## Project layout

```
hardware/gio-stacked/
├── README.md                  ← this file
├── docs/
│   ├── decisions.md           ← architecture rationale, decisions log
│   ├── interconnect.md        ← board-to-board pin assignments
│   ├── faceplate-layout.md    ← panel positions, hole sizes, silkscreen
│   └── stackup.md             ← mechanical: standoffs, board heights, clearances
├── power/                     ← Power board KiCad project (TBD)
├── signal/                    ← Signal board KiCad project (TBD)
└── faceplate/                 ← Faceplate KiCad project (TBD)
```

## BOM additions vs `hardware/gio/`

- 2× Thonkiconn PJ-3.5mm jacks (6 total instead of 4)
- 1× tactile button (Omron B3F-1000 or equivalent, 6×6 mm)
- 2× 2.54 mm pin header + socket pairs (1× 2×5 power↔signal, 1× 2×14 or 2×16 signal↔faceplate)
- 4× M2.5 standoffs (length TBD — see `docs/stackup.md`)
- 8× M2.5 screws

Everything else (XIAO, DAC, ADC, op-amps, reference, OLED, encoder, pot, BAT54S, decoupling caps, resistors) is the same as `hardware/gio/bom.md` — quantities adjust per board.

## Build / fabrication plan (rough)

1. Schematic for each of the 3 boards (this is the next session's work).
2. Mechanical mockup — print 1:1 paper templates of all three boards + faceplate to verify spacing before PCB layout.
3. Place + route signal board first (densest, hardest constraint).
4. Place + route power board.
5. Place + route faceplate (mostly through-hole, easy).
6. ERC + DRC pass on all three.
7. Manufacturing files (gerbers, BOM, CPL) per board.
8. JLCPCB order (3 boards × 5 each = 15 PCBs; ~$30 with shipping).
9. Hand-assemble + bench-bring-up each board independently before stacking.

## Open questions (deferred)

- Exact standoff height — depends on tallest signal-board component (likely bulk caps; finalized at parts-placement time).
- Whether the NeoPixel stays on the XIAO board (signal stack) or migrates to the faceplate for visibility through a light-pipe.
- Whether to add a second button (encoder-already-has-click, tap-tempo separately, plus a third would need pin budget review).
