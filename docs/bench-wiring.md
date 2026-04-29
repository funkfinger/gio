# Bench wiring — gio platform rebuild

Single-source wiring reference for the breadboard rebuild covering Stories 012 (SPI bus), 013 (DAC output stage), 015 (input protection + scaling), plus the encoder pin migration. Use alongside `decisions.md` §25 and `hardware/bom.md`.

**Bench substitution active:** REF3040 not yet on hand → using a 10 kΩ multi-turn trimpot dialled to 4.096 V, buffered through one TL072 channel. See §5. Swap to REF3040 SOT-23 once it arrives — same node, same decoupling.

---

## 1. Bill of materials on the breadboard

Through-hole / DIP versions to keep this hand-wireable:

| Role | Part | Inventory location | Notes |
|---|---|---|---|
| MCU | Seeed XIAO RP2350 | (in hand) | On female socket strips |
| 16-bit DAC | DAC8552 (MSOP-8 on adapter) | (already soldered to breakout) | |
| 12-bit ADC | MCP3208-CI/P (PDIP-16) | (1× already soldered to breakout; spares from Digikey order) | |
| Op-amp | TL072CP (DIP-8) | `tl072cp-dual-jfet-opamp.md` | Need 2× — one for DAC outputs, one for ADC inputs + VREF buffer |
| Schottky | BAT43 (DO-35 axial) | Cab-2 / Bin-10, 50+ | 8 needed (2 per jack × 4 jacks) — bench uses singles, PCB uses BAT54S |
| OLED | 0.49" 64×32 SSD1306 I²C | (on hand) | |
| Encoder | PEC11R | (on hand) | |
| Pot (tempo) | 10 kΩ linear | (on hand) | |
| Pot (VREF trim) | 10 kΩ multi-turn trimpot | (on hand or Bourns 3296) | Bench stand-in for REF3040 |
| Power | Hivemind Protomato | (in hand) | Provides ±12 V, +5 V, GND |

Resistors / caps assumed available in standard E12 values.

---

## 2. Power distribution

Protomato delivers ±12 V, +5 V, GND directly onto breadboard rails. No on-breadboard regulation needed.

| Rail | Source | Goes to |
|---|---|---|
| **+12 V** | Protomato +12 | TL072 V+ (pin 8 of each), BAT43 anode-side clamps |
| **−12 V** | Protomato −12 | TL072 V− (pin 4 of each), BAT43 cathode-side clamps |
| **+5 V** | Protomato +5 | DAC8552 VDD, MCP3208 VDD, XIAO 5V pin, VREF trim pot CW |
| **+3.3 V** | XIAO 3V3 | (none externally — XIAO uses internally) |
| **GND** | Common | All grounds — single star point at the Protomato output |

**Decoupling caps** (place close to each IC's supply pin):
- 100 nF ceramic on each TL072 V+ and V− (4 caps total)
- 100 nF on DAC8552 VDD
- 100 nF on MCP3208 VDD
- 100 nF on REF-buffer output (the VREF rail) — see §5
- 10 µF electrolytic bulk somewhere on the +5 V rail

---

## 3. XIAO RP2350 pinout (with encoder migration)

Pin assignments after the SPI-pivot. **Encoder moves off D8/D9/D10** (now SPI) **to A1/A2/D7**.

| XIAO pin | GPIO | Net | Direction | Notes |
|---|---|---|---|---|
| D0 | GP26 | TEMPO_POT_WIPER | input (analog) | Tempo pot wiper |
| D1 | GP27 | ENCODER_A | input | Encoder A (was D8) |
| D2 | GP28 | ENCODER_B | input | Encoder B (was D9) |
| D3 | GP5 | CS_DAC | output | DAC8552 /SYNC, 10 kΩ pull-up to +3.3 V |
| D4 | GP6 | I2C_SDA | bidir | OLED SDA |
| D5 | GP7 | I2C_SCL | output | OLED SCL |
| D6 | GP0 | CS_ADC | output | MCP3208 /CS, 10 kΩ pull-up to +3.3 V |
| D7 | GP1 | ENCODER_CLICK | input | Encoder push-switch (was D10) |
| D8 | GP2 | SPI_SCK | output | Shared SPI0 clock |
| D9 | GP4 | SPI_MISO | input | Shared SPI0 MISO (from MCP3208 only) |
| D10 | GP3 | SPI_MOSI | output | Shared SPI0 MOSI |
| 5V | — | +5 V | input | From Protomato |
| 3V3 | — | +3.3 V | output | XIAO regulator output (only used for CS pull-ups) |
| GND | — | GND | — | |

Pull-up resistors: **CS_DAC and CS_ADC each get a 10 kΩ to +3.3 V** so both ICs are deselected at boot before firmware runs.

---

## 4. SPI bus wiring (shared bus, two chip-selects)

```
                ┌────────────────────────────────────────┐
                │                                         │
   XIAO         │   DAC8552 (MSOP-8 on breakout)         │
   ─────────    │   ────────────────────────────         │
   SCK  D8 ────●────── SCLK (pin 4)                      │
   MOSI D10 ───●────── DIN  (pin 5)                      │
   CS_DAC D3 ─────────  /SYNC (pin 6)                    │
                                                          │
                │   MCP3208 (PDIP-16 on breakout)        │
                │   ─────────────────────────────        │
                ●────── CLK  (pin 13)                    │
                ●────── DIN  (pin 11)                    │
   MISO D9 ─────────── DOUT (pin 12)                     │
   CS_ADC D6 ───────── /CS  (pin 10)                     │
                                                          │
                └────────────────────────────────────────┘
```

`●` = shared bus node (jumper to both ICs).

**MCP3208 power + reference pins:**
| MCP3208 pin | Net |
|---|---|
| 9 (DGND) | GND |
| 14 (AGND) | GND |
| 15 (VREF) | VREF rail (4.096 V — see §5) |
| 16 (VDD) | +5 V |

**DAC8552 power + reference pins:**
| DAC8552 pin | Net |
|---|---|
| 3 (GND) | GND |
| 8 (VDD) | +5 V |
| 7 (VREFB) | VREF rail (4.096 V) |
| 1 (VREFA) | VREF rail (4.096 V) |
| 2 (VOUTA) | → input of output-stage A op-amp (§6) |
| 6 (VOUTB) | → input of output-stage B op-amp (§6) |

---

## 5. VREF stand-in (pot + TL072 buffer)

Temporary substitute for the REF3040. Lives until the SOT-23 arrives, at which point this whole sub-circuit gets snipped out and the REF3040 drops in at the `VREF rail` node with the same 100 nF decoupling.

```
+5V ──────┬── pot CW (terminal 1)
          │
          │   ┌──── pot wiper (terminal 2) ──── + input (pin 3) of TL072 #1A
          │   │
          │   │                            ┌──── output (pin 1) ──┬── VREF rail
          │   │                            │                      ├── 100 nF to GND
          │   │                            │                      │
          │   │                  − input (pin 2) ────────────────┘
          │   │
GND ──────┴── pot CCW (terminal 3)
```

- TL072 #1A is one of two op-amp channels in the package; the other (#1B) is free for use elsewhere
- Wire as a unity-gain follower (output tied to inverting input)
- Power TL072 from ±12 V (pins 8 and 4)

**Bench procedure:**
1. Power up before connecting VREF rail to the DAC/ADC
2. Meter on the TL072 output, turn pot until reading is **4.096 V ± 5 mV**
3. Connect VREF rail to DAC8552 pins 1 and 7, and MCP3208 pin 15
4. Re-meter the buffer output — should not move (that's the whole point of the buffer)

**Why buffered, not a bare divider:** the DAC8552 R-2R ladder draws transient current on VREF as bits switch. Without a buffer, those transients pull VREF around and smear conversion accuracy.

---

## 6. DAC output stage (per channel — 2× identical)

Inverting summing amplifier turns the DAC's 0..4.096 V swing into a ±10 V Eurorack-friendly bipolar output. One TL072 channel per DAC output → use the second TL072 IC for both DAC channels.

```
DAC OUTA (0..4.096V) ──[10k, R_in]──┐
                                     ├──[− input (pin 2) of TL072 #2A]
VREF rail (4.096V) ──[20k, R_off]───┘
                       │
                  [48.7k, R_fb]──── output (pin 1)
                       │
                       └─── 1k series ──┬──── BAT43 anode → +12 V (clamps overshoot above +12)
                                        ├──── BAT43 cathode → −12 V (clamps below −12; reverse the diode)
                                        │
                                        └──── jack tip (J3 = OUT 1)
```

**Resistor values (E96 nominals; E12 substitutes in parens):**
- R_in = 10 kΩ (E12 ✓)
- R_off = 20 kΩ (use 22 kΩ E12 — gives ~9.07 V offset instead of 9.97 V, bench-tune R_fb upward to compensate, or just use as-is and accept ±9 V swing)
- R_fb = 48.7 kΩ (use 47 kΩ E12 — gives gain ≈ 4.7 instead of 4.87, swing ≈ ±9.6 V — fine for bench)

Math: Vout = +(R_fb/R_off)·VREF − (R_fb/R_in)·VDAC, target mapping:
- DAC = 0 V → out ≈ +10 V (max positive)
- DAC = 2.048 V → out ≈ 0 V
- DAC = 4.096 V → out ≈ −10 V
- Firmware compensates the inversion in `voltsToDacCount()`

**TL072 #2 channel assignments:**
- #2A → DAC OUTA → jack J3 (OUT 1)
- #2B → DAC OUTB → jack J4 (OUT 2)

**Output protection:**
- 1 kΩ series resistor between op-amp output and jack tip — limits short-circuit current to ~12 mA
- Two BAT43 Schottkies at the jack tip:
  - One with **anode at jack tip, cathode at +12 V rail** — clamps positive overshoot
  - One with **cathode at jack tip, anode at −12 V rail** — clamps negative overshoot

---

## 7. Input protection + scaling (per channel — 2× identical)

Mirror of the output stage in reverse: protected, attenuated, biased ±10 V jack input → 0..4.096 V at the ADC. One TL072 channel per ADC input → use the **first TL072 IC's spare channel #1B** for input 1, and a **third TL072** (or second TL074) for input 2 — or just commit a TL074 quad upfront for cleanliness.

(For first bench session, wire only **one input channel** through ADC ch 0 to validate the stage; replicate for ch 1 once the loopback test passes.)

```
Jack tip (J1 = IN 1)
   │
   ├── 100 kΩ series ──┬─── BAT43 anode → −12 V (cathode at node A; clamps negative)
   │                   ├─── BAT43 cathode → +12 V (anode at node A; clamps positive)
   │                   │
   │            (node A) ──── 22 kΩ ────┐
   │                                     ├──[− input (pin 6) of TL072 #1B]
   │                  VREF rail (4.096V) ─[9.4k]──┘
   │                                       │
   │                                  [4.7k, R_fb]──── output (pin 7) ──── MCP3208 ch 0 (pin 1)
```

**Resistor values:**
- R_series = 100 kΩ (sets jack input impedance + clamp current limit; max abuse current ±15 V → 30 µA, BAT43 200 mA rating gives huge margin)
- R_in2 = 22 kΩ (E12 ✓ — sets gain)
- R_off = 9.4 kΩ (use 9.1 kΩ E12 or two 4.7 kΩ in series; R_fb may need bench-tune)
- R_fb = 4.7 kΩ (E12 ✓)

Math: Vout = +(R_fb/R_off)·VREF − (R_fb/R_in2)·Vin, target mapping:
- Jack +10 V → ADC reads 0 V → count 0
- Jack 0 V → ADC reads 2.048 V → count 2048
- Jack −10 V → ADC reads 4.096 V → count 4095
- Firmware reverses the inversion in `inputs.readVolts()`

**Input protection:** 100 kΩ first-stage + BAT43 dual clamp directly at the jack — same diode polarity as outputs but mirrored (anode-to-−12 to clamp negative, cathode-to-+12 to clamp positive at node A).

---

## 8. OLED + encoder + tempo pot

| Device | Pin | Net |
|---|---|---|
| OLED | VCC | +3.3 V |
| OLED | GND | GND |
| OLED | SDA | XIAO D4 |
| OLED | SCL | XIAO D5 |
| Encoder | A | XIAO D1 (with 10 kΩ pull-up to +3.3 V — but see decisions.md §18 for RP2350-E9: use ≤ 8.2 kΩ) |
| Encoder | B | XIAO D2 (same pull-up) |
| Encoder | Common | GND |
| Encoder switch | One side | XIAO D7 (with 10 kΩ pull-up — same RP2350-E9 caveat applies if it's a pulldown setup) |
| Encoder switch | Other side | GND |
| Tempo pot | CW | +3.3 V |
| Tempo pot | Wiper | XIAO D0 |
| Tempo pot | CCW | GND |

**Note on encoder pull-ups (RP2350-E9):** the silicon errata in `decisions.md` §18 says external pull-downs must be ≤ 8.2 kΩ. Pull-**ups** are unaffected — 10 kΩ is fine for the encoder. Internal pulls in the SDK are usable as-is.

---

## 9. Jack assignments (per `decisions.md` §23: generic IN/OUT labels)

| Jack | Direction | Bench connection | App use (arp) |
|---|---|---|---|
| J1 | IN 1 | Input stage ch 1 → MCP3208 ch 0 | CV in / clock-trigger |
| J2 | IN 2 | Input stage ch 2 → MCP3208 ch 1 | CV in (transpose) |
| J3 | OUT 1 | Output stage ch A ← DAC OUTA | V/Oct |
| J4 | OUT 2 | Output stage ch B ← DAC OUTB | Gate |

Loopback test convenience: jumper **J3 → J1** during smoke-test to verify DAC writes track ADC reads through the full signal chain.

---

## 10. Build order suggestion

Wire in this sequence — test at each step before moving on:

1. **Power distribution** — verify rails on multimeter (±12, +5, GND continuity)
2. **XIAO socketed + powered** — flash the `Blink` example via USB to confirm board is alive
3. **VREF stand-in (§5)** — wire pot + TL072 buffer, dial to 4.096 V on meter
4. **SPI bus + DAC8552 (§4 + §6)** — load `lib/outputs/`, run smoke test, scope OUTA for triangle wave
5. **MCP3208 (§4)** — load `lib/inputs/`, jumper DAC OUTA → MCP3208 ch 0 directly (skip the input scaling), confirm count tracks DAC value
6. **Output stage (§6)** — insert TL072 + resistors between DAC and J3, confirm ±10 V swing at jack
7. **Input scaling (§7)** — wire jack J1 through input stage to MCP3208 ch 0; bench-supply known voltages, confirm `inputs.readVolts()` is within tolerance
8. **OLED + encoder + tempo pot (§8)** — re-attach UI; verify menu navigation works on the new pin assignments
9. **Loopback** — jumper J3 → J1, run a sweep, confirm closed-loop accuracy

Each step has an entry in `bench-log.md` waiting for measurements + observations.
