# Faceplate layout

4 HP Eurorack panel — 20.32 mm wide × 128.5 mm tall (3U). All positions
in mm relative to the **top-left corner of the panel**.

Iteration target: this is the v1 layout to mock up on paper. Real positions
get finalized when KiCad placement happens and we can see exact component
footprints in the editor.

## Mounting holes

Standard Eurorack pattern — 3 mm holes, M3 screws, recessed for the rails.

| Hole | X (mm) | Y (mm) |
|------|--------|--------|
| TL   | 3.0    | 3.0    |
| TR   | 17.3   | 3.0    |
| BL   | 3.0    | 125.5  |
| BR   | 17.3   | 125.5  |

Vertical spacing 122.5 mm (standard 3U). Horizontal: 14.3 mm (4 HP).

## Component placement

```
        ┌──────────────────┐  ← y = 0
        │                  │
        │     ┌──────┐     │  ← y = 8  (OLED top)
        │     │ OLED │     │
        │     └──────┘     │  ← y = 22 (OLED bottom)
        │                  │
        │      (○)         │  ← y = 35 (encoder center)
        │                  │
        │                  │
        │      (○)         │  ← y = 55 (pot center)
        │                  │
        │       •          │  ← y = 72 (button center)
        │                  │
        │   ◯       ◯      │  ← y = 88 (J1, J2 row)
        │                  │
        │   ◯       ◯      │  ← y = 100 (J3, J4 row)
        │                  │
        │   ◯       ◯      │  ← y = 112 (J5, J6 row)
        │                  │
        └──────────────────┘  ← y = 128.5
```

| Component | X (mm) | Y (mm) | Hole / footprint |
|-----------|--------|--------|------------------|
| OLED breakout (SSD1306 64×32, 0.49") | center, mounted on faceplate via 4× M2 standoffs to signal-board headers | ~15 (top of viewport) | breakout is ~25×11 mm; cutout in faceplate ~14×8 mm for the active display area only |
| Encoder (PEC11) | 10.16 (center) | 35 | 7 mm round hole + 2 mm anti-rotation hole |
| Pot (Alpha 9 mm) | 10.16 | 55 | 7 mm round hole + 2 mm anti-rotation hole |
| Tactile button (B3F-1000) | 10.16 | 72 | 4 mm round hole for tip; SMD or TH body soldered to faceplate PCB |
| J1 (CV in 1 / clock) | 6.5  | 88  | 6 mm Thonkiconn hole |
| J2 (CV in 2 / V/oct) | 13.8 | 88  | 6 mm |
| J3 (CV in 3, new)   | 6.5  | 100 | 6 mm |
| J4 (CV in 4, new)   | 13.8 | 100 | 6 mm |
| J5 (CV out, V/oct)  | 6.5  | 112 | 6 mm |
| J6 (Trigger out)    | 13.8 | 112 | 6 mm |

**Center column at X = 10.16 mm** (panel half-width). Jack columns offset
±3.66 mm gives 7.32 mm center-to-center — fits 6 mm Thonkiconn nuts with
~1.3 mm flesh between them. Tight but standard.

**Jack row pitch 12 mm** — enough for the nut + a sliver of silkscreen label
underneath each jack ("CLK", "CV", "V/O", "GATE", etc.).

## Silkscreen plan

- Module name "gio" at top, above the OLED
- Tiny label under each jack (3-letter abbreviations)
- Optional: param-name reminders next to encoder + pot
- Version "v2" or revision number at bottom

## Faceplate-PCB vs aluminum-only

This is a real PCB acting as a faceplate (FR4 with black soldermask + white
silkscreen for the "look"). Trade-offs:

- **Pro:** components mount directly (jacks, encoder, pot, button, OLED breakout headers) — no panel wiring to a separate PCB. Mechanically robust.
- **Pro:** silkscreen is easy (no anodizing / paint mask).
- **Con:** less premium feel than milled aluminum. Black soldermask + matte finish gets close but isn't identical.
- **Con:** FR4 panel can flex if the user pushes hard on the encoder — mitigated by the 4-corner standoffs to the signal board.

Decision is locked in by the three-board architecture (faceplate must be a
PCB to hold the components on its back side).

## Open questions

- OLED breakout style — solder directly to faceplate (back side), or use a
  pin header + socket so it's removable? (Removable = easier rework, but
  adds depth to the stack.)
- Indicator LED — current design has the NeoPixel on the XIAO (signal board), invisible behind the faceplate. Options: (a) light pipe to faceplate, (b) move NeoPixel to faceplate with one extra signal across J_FACE, (c) accept it as internal-only.
- Anti-rotation tabs on encoder + pot — most have one; verify orientation matches our hole pattern.
