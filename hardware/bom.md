# gio Rev 0.1 — BOM (JLCPCB)

Single source of truth for the parts that JLCPCB will place during PCB assembly. All choices follow the **Basic-first** rule from `docs/decisions.md` §25.

## SMD parts (placed by JLC)

| Role | Qty | Part # | LCSC | Mfr | Pkg | Class | Unit price |
|---|---|---|---|---|---|---|---|
| +5 V LDO (VDD rail) | 1 | AMS1117-5.0 | C6187 | Advanced Monolithic Systems | SOT-223 | 🟢 Basic | $0.118 |
| 4.096 V precision VREF | 1 | REF3040AIDBZR | C19415 | Texas Instruments | SOT-23 | Extended | $0.194 |
| 16-bit dual SPI DAC | 1 | DAC8552IDGKR | C136020 | Texas Instruments | MSOP-8 | Extended | $3.004 |
| 12-bit 8-ch SPI ADC | 1 | MCP3208T-CI/SL | C626764 | Microchip | SOIC-16 | Extended | $2.170 |
| Dual JFET op-amp | 2 | TL072CDT | C6961 | STMicroelectronics | SO-8 | 🟢 Basic | $0.082 |
| Schottky clamp pair | 4 | BAT54SLT1G | C19726 | onsemi | SOT-23 | Extended | $0.016 |
| Power-rail ferrite | 2 | GZ2012D601TF | C1017 | Sunlord | 0805 | 🟢 Basic | $0.023 |
| Decoupling caps (100 nF) | ~12 | TBD | TBD | TBD | 0805 | 🟢 Basic | trivial |
| Bulk caps (10 µF) | ~6 | TBD | TBD | TBD | 0805 / B-case | 🟢 Basic | trivial |
| Resistors (E24, 0805) | ~30 | TBD | TBD | TBD | 0805 | 🟢 Basic | trivial |

**Per-board chip cost:** ~$5.80
**Per-batch feeder fees (4 unique Extended parts × $3):** ~$12

## Through-hole / panel parts (hand-soldered post-shipment)

Not placed by JLC. Sourced from Thonk / Mouser / inventory.

| Role | Qty | Part | Source |
|---|---|---|---|
| 3.5 mm mono jacks | 4 | Thonkiconn PJ398SM | Thonk |
| 9 mm panel pot | 1 | Alpha 9mm linear taper | Thonk / Mouser |
| Rotary encoder w/ switch | 1 | Bourns PEC11R | Mouser / Digikey |
| Eurorack 2×5 IDC power header | 1 | Standard Doepfer pinout | Thonk / Mouser |
| 1×7 female socket (XIAO) | 2 | 0.1" pitch, through-hole | Mouser / Adafruit / Thonk |
| 0.49" 64×32 SSD1306 OLED | 1 | I²C breakout, hand-soldered | AliExpress / Adafruit |
| XIAO RP2350 daughterboard | 1 | Seeed XIAO RP2350 | Seeed / Mouser |

**Total hand-soldered joints per board:** ~60 (jacks 12, pot 3, encoder 5, power 10, OLED 4, XIAO sockets 14, plus a few jumpers / mounting). Trivial for a hobby run.

## Vetting checklist for future BOM additions

When adding any new SMD line, follow this in order:

1. Search `https://jlcpcb.com/parts/componentSearch?searchTxt=<PART>`
2. **Prefer Basic** when a functional equivalent exists
3. For Extended-only parts, prefer original manufacturer over clones in signal-path roles (references, op-amps, converters)
4. Check stock level — > 500 units is safe; lower means risk of substitution at assembly time
5. Capture LCSC part number + class + unit price in the table above
6. If the choice is non-obvious (e.g. Basic clone vs. Extended original), capture the rationale in `docs/decisions.md`
