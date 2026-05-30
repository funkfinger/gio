# Mechanical stackup

Three boards held parallel by 4× M2.5 standoffs at the panel mounting-hole
positions. Standoff lengths chosen so the tallest component on each board
clears the next board with a few mm of margin.

```
   Eurorack rails (top)
         │
         │ 3 mm clearance to top rail
         │
   ┌─────┴─────┐  ← faceplate PCB    (front of module — y = 0)
   │           │
   │   ~15 mm  │  ← faceplate standoff: jacks (10mm body) + PCB + clearance
   │           │
   ├───────────┤  ← signal PCB
   │           │
   │   ~13 mm  │  ← signal-to-power standoff: tallest signal-board component + clearance
   │           │
   ├───────────┤  ← power PCB        (back of module — depth ≈ 30 mm)
   │           │
   └───────────┘
   Eurorack power connector (back) — depth-limited to ~40 mm
```

## Standoff selection (preliminary — refine after parts placement)

| Junction | Standoff length | Why |
|----------|-----------------|-----|
| Faceplate ↔ Signal | 14 mm | Thonkiconn body ~10 mm + 2 mm clearance + 2 mm signal-board component headroom |
| Signal ↔ Power | 13 mm | Signal-board headers/op-amps ~5 mm + power-board bulk caps ~6 mm + clearance |
| Total stack depth | ~30 mm | (faceplate PCB 1.6 + 14 + signal PCB 1.6 + 13 + power PCB 1.6) — within typical Eurorack 40 mm depth budget |

**M2.5 hex female-female standoffs** at all four corners, in brass or
stainless. Common lengths available: 10, 12, 15, 18 mm — round up to nearest
stocked length and re-verify clearance.

## Component-height assumptions (verify at placement)

**Signal board (between faceplate and power):**
- XIAO RP2350 module: ~3 mm above PCB
- DAC8552, MCP3208, TL072s: SOIC, ~2.5 mm
- 2.54 mm pin header (to faceplate): 8 mm above PCB when mated
- 2.54 mm pin header (to power): 8 mm above PCB when mated (downward)
- **Tallest:** the inter-board headers themselves at ~8 mm; clears the 13 + 14 mm standoff budget easily

**Power board (back of stack):**
- Bulk caps (radial 100 µF / 220 µF, 10 V or higher): ~10–13 mm tall
- LDO (78L05 or LM78M05 in TO-220 / SOT-223): ~10 mm if TO-220, ~3 mm if SOT-223
- REF3040 SOT-23: ~1 mm
- Eurorack 2×5 IDC header: ~9 mm
- **Tallest:** bulk caps or LDO TO-220 at ~10–13 mm; faces AWAY from signal board (toward back of module), so this height doesn't impact the inter-board gap

**Faceplate (front):**
- Thonkiconn jacks: 10 mm body depth (rear of PCB toward signal board)
- Encoder: 12–15 mm body, but encoder body protrudes FORWARD (through panel), not backward
- Pot: 9 mm body, similar — protrudes forward
- OLED breakout: ~3 mm thick PCB + 0.49" glass on the front
- Button: 6 mm body, low profile
- **Tallest rear-facing component:** Thonkiconn jacks at 10 mm — drives the faceplate-to-signal-board standoff length

## Eurorack depth budget

| Item | Depth (mm) |
|------|------------|
| Faceplate PCB thickness | 1.6 |
| Standoff faceplate ↔ signal | 14 |
| Signal PCB thickness | 1.6 |
| Standoff signal ↔ power | 13 |
| Power PCB thickness | 1.6 |
| Eurorack header (rear-facing) | 9 |
| **Total module depth** | **40.8** |

Standard Eurorack cases (Doepfer A-100) allow ~40 mm depth in the rear bay
before clearing the bus board. This is at the limit. Two ways to trim:

- Shorter signal-to-power standoff (10 mm instead of 13) if signal-board components allow → -3 mm total
- Surface-mount power components instead of TO-220 LDO → may allow 10 mm power-side standoff

Track depth carefully during placement; trim if needed.

## Anti-rotation / alignment

Both inter-board connectors are symmetrical (2×N pin headers), so they can
in principle be inserted reversed. Mitigation options:

- **Shrouded headers** with polarized key — adds cost (~$0.50/connector) but eliminates reverse-insert risk
- **Mechanical pin keying** — drill an extra alignment pin hole at a non-symmetric position
- **Silkscreen + careful labeling** — cheapest, relies on the builder

Recommend shrouded headers for the J_PWR connector (rail reversal would be
catastrophic — reverse-polarity protection on the power board would catch
it but the BAT54S/diode would dissipate the full short-circuit current
briefly). J_FACE less critical; can ship un-shrouded with clear silkscreen.

## Open questions

- Final standoff lengths — depends on actual component heights from placement
- Whether the OLED breakout sits on the faceplate (mounted to faceplate-back via low-profile header) or on the signal board with a window cutout in the faceplate
- Whether to add front-panel LEDs (would need light pipes through the faceplate)
