# Inter-board connectors

Two 2.54 mm pin header / socket pairs join the three boards. Pin assignments
locked in here BEFORE schematic work so the boards can be developed
independently and verified against this contract.

---

## J_PWR — Power ↔ Signal (2×5, 10 pin, 2.54 mm)

Carries Eurorack rails + regulated supplies + voltage reference from the
power board to the signal board. Mirrored layout pairs each rail with an
adjacent GND for low loop area / good return paths.

```
                top of stack
                ─────────
   +12V  ●  ●  +12V        ← Eurorack +12 V (in)
   GND   ●  ●  GND
  -12V   ●  ●  -12V        ← Eurorack -12 V (in)
   GND   ●  ●  GND
   +5V   ●  ●  +VREF       ← +5 V regulated; +VREF = 4.096 V (REF3040)
                ─────────
              power board ←→ signal board
```

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 1, 2 | +12V | power → signal | Eurorack rail, pre-regulator. Decouple at consumer. |
| 3, 4 | GND  | bidirectional | |
| 5, 6 | -12V | power → signal | Eurorack rail. |
| 7, 8 | GND  | bidirectional | |
| 9    | +5V  | power → signal | Linear-regulated from +12V on the power board (XIAO USB-disconnected operation). |
| 10   | +VREF | power → signal | 4.096 V from REF3040. Shared by DAC8552 + MCP3208 for ratiometric scaling. |

**Connector parts:**
- Header (power board): 2×5 male, 2.54 mm, vertical
- Socket (signal board): 2×5 female, 2.54 mm, vertical (or shrouded for keying)
- TBD: keyed/shrouded vs un-keyed (reversibility risk → consider shrouded)

---

## J_FACE — Signal ↔ Faceplate (2×14, 28 pin, 2.54 mm)

Carries jack tip signals, jack sleeve return, panel control I/O (encoder,
pot, button), and the OLED I²C bus + power between the signal board and
the faceplate.

```
                top of stack
                ────────────
 +3V3   ●  ●  GND               ← OLED + encoder + pot supply
 SDA    ●  ●  SCL               ← I²C (OLED)
 OLED_RST ● ● ENC_A             ← OLED reset (optional, may tie high); encoder A
 ENC_B  ●  ●  ENC_CLICK         ← encoder B; encoder click
 POT_W  ●  ●  BTN1              ← pot wiper; tap-tempo button
 J1_TIP ●  ●  J2_TIP            ← CV in 1 (clock); CV in 2 (V/oct)
 J3_TIP ●  ●  J4_TIP            ← CV in 3; CV in 4
 J5_TIP ●  ●  J6_TIP            ← CV out (V/oct); Trigger out
 GND    ●  ●  GND               ← jack sleeves (commoned)
 GND    ●  ●  GND               ← spare returns
 NC     ●  ●  NC                ← spare / future expansion
 NC     ●  ●  NC                ← spare / future expansion
 NC     ●  ●  NC                ← spare / future expansion
 GND    ●  ●  GND               ← endcap GND for shield/strain relief
                ────────────
              signal board ←→ faceplate
```

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 1   | +3V3      | signal → face | Powers OLED, encoder pull-ups, pot top |
| 2   | GND       | bidirectional | |
| 3   | SDA       | bidirectional | I²C data (XIAO Wire1 SDA, GP6) |
| 4   | SCL       | signal → face | I²C clock (XIAO Wire1 SCL, GP7) |
| 5   | OLED_RST  | signal → face | Optional — most SSD1306 breakouts tie internally |
| 6   | ENC_A     | face → signal | Encoder phase A (XIAO D1) |
| 7   | ENC_B     | face → signal | Encoder phase B (XIAO D2) |
| 8   | ENC_CLICK | face → signal | Encoder button (XIAO D7) |
| 9   | POT_W     | face → signal | Pot wiper (to MCP3208 CH0) |
| 10  | BTN1      | face → signal | Tap-tempo button (XIAO D0, with internal pull-up + debounce in FW) |
| 11  | J1_TIP    | face → signal | CV in 1 / clock — to TL072 input stage ch 1 |
| 12  | J2_TIP    | face → signal | CV in 2 / V/oct — to TL072 input stage ch 2 |
| 13  | J3_TIP    | face → signal | CV in 3 (new) — to TL072 input stage ch 3 |
| 14  | J4_TIP    | face → signal | CV in 4 (new) — to TL072 input stage ch 4 |
| 15  | J5_TIP    | signal → face | CV out (V/oct) — from TL072 output stage ch A |
| 16  | J6_TIP    | signal → face | Trigger out — from TL072 output stage ch B |
| 17, 18 | GND     | bidirectional | Jack sleeves commoned |
| 19, 20 | GND     | bidirectional | Spare returns for noise/shielding |
| 21–26 | NC       | — | Spare. Reserved for future expansion (additional jack, second button, NeoPixel-on-faceplate signal, etc.) |
| 27, 28 | GND     | bidirectional | Mechanical endcap GND |

**Why 2×14:** signal count (~16 active signals) + GND spread (~8 pins) +
spare expansion (~6 pins) = 28 well-utilized pins. Keeps a single connector
per junction; alternatives (split into two smaller connectors for analog vs
digital) add mechanical complexity for no real noise win at this signal density.

**Connector parts:**
- Header (signal board): 2×14 male, 2.54 mm, vertical
- Socket (faceplate): 2×14 female, 2.54 mm, vertical
- Same TBD-shrouded question as J_PWR.

---

## Routing rules (both connectors)

- Adjacent GND on every rail/signal pair where possible (already enforced in pinouts above)
- Keep analog signals (POT_W, JxTIP) away from SCL/SDA on the connector — they're on opposite ends of J_FACE by design
- DRC: 0.2 mm trace/space minimum for cost-tier JLCPCB
- Connector footprints must align across the 3 boards (same X/Y on both sides of each junction)

## Verification before fab

- Print 1:1 paper templates of all three boards
- Stack with cardboard between (proxy for standoff height)
- Mechanically verify connector alignment — both junctions should line up vertically with the standoff holes
