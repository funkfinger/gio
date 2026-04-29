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
| D3 | GP5 | CS_DAC | output | DAC8552 /SYNC |
| D4 | GP6 | I2C_SDA | bidir | OLED SDA |
| D5 | GP7 | I2C_SCL | output | OLED SCL |
| D6 | GP0 | CS_ADC | output | MCP3208 /CS |
| D7 | GP1 | ENCODER_CLICK | input | Encoder push-switch (was D10) |
| D8 | GP2 | SPI_SCK | output | Shared SPI0 clock |
| D9 | GP4 | SPI_MISO | input | Shared SPI0 MISO (from MCP3208 only) |
| D10 | GP3 | SPI_MOSI | output | Shared SPI0 MOSI |
| 5V | — | +5 V | input | From Protomato |
| 3V3 | — | +3.3 V | output | XIAO regulator output (unused on the bench; routed on PCB for CS pull-ups) |
| GND | — | GND | — | |

**Pull-ups — bench skips them, PCB adds them:**

- **Encoder (A/B/click)** — no externals needed. `encoder_input.cpp` configures `INPUT_PULLUP` so the RP2350's internal pull-ups (~50–80 kΩ) handle idle state. Same on bench and PCB.
- **CS_DAC / CS_ADC** — skip on the bench. Adds 2× 10 kΩ to +3.3 V on the PCB as belt-and-suspenders to keep both SPI ICs deselected during the ~200–500 ms power-on → bootloader → firmware-init window. The risk without them is essentially zero (SPI ICs need clocked frames to do anything), but two 0805 resistors cost nothing on the board.
- **RP2350-E9 errata is irrelevant here** — it only affects pull-*downs*. Every pull on this design is a pull-up.

---

## 4. SPI bus wiring (shared bus, two chip-selects)

```
                ┌────────────────────────────────────────┐
                │                                         │
   XIAO         │   DAC8552 (MSOP-8 on breakout)         │
   ─────────    │   ────────────────────────────         │
   SCK  D8 ────●────── SCLK (pin 6)                      │
   MOSI D10 ───●────── DIN  (pin 7)                      │
   CS_DAC D3 ─────────  /SYNC (pin 5)                    │
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

**DAC8552 power + reference pins** (per TI datasheet SLAS430A, MSOP-8 DGK package):

| DAC8552 pin | Net |
|---|---|
| 1 (VDD) | +5 V |
| 2 (VREF) | VREF rail (4.096 V) — **single shared reference** for both channels |
| 3 (VOUTB) | → input of output-stage B op-amp (§6) |
| 4 (VOUTA) | → input of output-stage A op-amp (§6) |
| 8 (GND) | GND |

---

## 5. VREF stand-in (pot + TL072 buffer)

Temporary substitute for the REF3040. Lives until the SOT-23 arrives, at which point this whole sub-circuit gets snipped out and the REF3040 drops in at the `VREF rail` node with the same 100 nF decoupling.

### TL072 (DIP-8) pinout reference

The TL072 is a **dual** op-amp — two independent channels in one 8-pin package, plus shared power. We use channel A here for the VREF buffer; channel B is free for other uses (e.g., one of the input/output stages later).

```
              ┌──┬──┐
   1OUT (A) ──┤1 ⌣ 8├── V+   (+12 V)
   1IN− (A) ──┤2   7├── 2OUT (B)
   1IN+ (A) ──┤3   6├── 2IN− (B)
        V− ──┤4   5├── 2IN+ (B)
   (−12 V)   └─────┘
```

- **Channel A**: pins 1 (OUT), 2 (IN−), 3 (IN+) — used for VREF buffer below
- **Channel B**: pins 5 (IN+), 6 (IN−), 7 (OUT) — unused in §5; available
- **Power**: pin 8 (V+) → +12 V, pin 4 (V−) → −12 V — shared by both channels

### Pot terminal convention

A standard 3-terminal pot has terminals numbered **1, 2, 3** stamped on the body. Looking at the shaft from the front:

- **Terminal 1 (CCW end)** — leftmost when shaft is fully counter-clockwise
- **Terminal 2 (wiper)** — middle pin, voltage scales with shaft position
- **Terminal 3 (CW end)** — rightmost

For a **multi-turn trimpot** (Bourns 3296 etc.) the terminals are labeled the same. CW direction increases voltage at the wiper when terminal 3 is tied to +5 V.

### Wire list

Build this as a simple checklist on the breadboard. Every wire goes between exactly two named points.

| # | From | To | Notes |
|---|---|---|---|
| 1 | +5 V rail | Pot terminal 3 (CW) | Top of divider |
| 2 | GND rail | Pot terminal 1 (CCW) | Bottom of divider |
| 3 | Pot terminal 2 (wiper) | TL072 pin 3 (1IN+) | Divided voltage into the buffer's non-inverting input |
| 4 | TL072 pin 1 (1OUT) | TL072 pin 2 (1IN−) | Feedback wire — makes the buffer unity-gain (output follows input) |
| 5 | TL072 pin 8 (V+) | +12 V rail | Op-amp positive supply |
| 6 | TL072 pin 4 (V−) | −12 V rail | Op-amp negative supply |
| 7 | TL072 pin 1 (1OUT) | **VREF rail** node | The clean, buffered VREF — feed this to DAC8552 pin 2 and MCP3208 pin 15 |
| 8 | 100 nF cap | TL072 pin 8 → GND | Power decoupling, V+ side |
| 9 | 100 nF cap | TL072 pin 4 → GND | Power decoupling, V− side |
| 10 | 100 nF cap | VREF rail → GND | Reference decoupling at the buffer output |

### Schematic view

```
                                       +12V    GND
                                         │      │
                                         │      ┴── 100nF (decoupling, pin 8)
                                         │
+5V ──[pot CW, term 3]                  ┌┴─┐
              │                         │ 8│V+
            [pot]──[wiper, term 2]──────│3 │1IN+ (channel A)
              │                         │  │      ╲
GND ──[pot CCW, term 1]            ┌────│2 │1IN−   ─── pin 1 (1OUT)
                                   │    │  │      ╱       │
                                   │    │  │              │
                                   │    │ 4│V−            │
                                   │    └┬─┘              │
                                   │     │                │
                                   │    -12V              │
                                   │     │                │
                                   │     ┴── 100nF (decoupling, pin 4)
                                   │                      │
                                   └──── feedback ────────┤
                                       (pin 1 → pin 2)    │
                                                          │
                                              VREF rail ──┴── 100nF to GND
                                                          │
                                                          ├── DAC8552 pin 2 (VREF)
                                                          └── MCP3208 pin 15 (VREF)
```

### Bench procedure

1. **Build with VREF rail disconnected from the DAC/ADC.** Wires 1–6 + 8–9 only at first.
2. **Power up** the Protomato. Confirm ±12 V and +5 V rails on the multimeter.
3. **Meter on TL072 pin 1** (the buffer output). Turn the pot's screw until the reading is **4.096 V ± 5 mV**.
   - If the voltage moves opposite to expectations as you turn, swap pot terminals 1 and 3 — you've got CW and CCW reversed for your particular pot.
   - Multi-turn trimpots take ~25 turns end-to-end; expect to spin a lot.
4. **Add wire 7 + cap 10** — connect the buffered VREF to the rail going to DAC8552 pin 2 and MCP3208 pin 15.
5. **Re-meter the TL072 output.** Should not move from 4.096 V — that's the whole point of the buffer (the IC inputs are high-Z and the buffer absorbs any transient currents).
6. **Mark the trimpot screw position** with a Sharpie or lock the screw if it has one. Vibration can drift it.

### Why a buffer (not a bare divider into the converter VREF pins)

The DAC8552's R-2R ladder draws transient current on its VREF pin every time the output code changes. With a bare resistor divider, the divider's output impedance (a few kΩ) would let those transients pull VREF around — by tens of millivolts during fast updates. The TL072 follower presents a low-Z source (≈10 Ω at audio rates), absorbs the transient, and keeps the reference rock-stable.

Same logic applies to the MCP3208: each conversion samples a 20 pF capacitor onto VREF for ~1.5 µs. A buffered reference settles cleanly between conversions; a bare divider doesn't.

### Drop-in path for REF3040

When the SOT-23 arrives, all of §5 collapses to:

- Cut wires 1–9 (everything except wire 7 and cap 10)
- Solder REF3040 onto a SOT-23 → DIP adapter (or directly to the breadboard with fly-wires)
- Connect: REF3040 pin 1 (GND) → GND; pin 2 (IN) → +5 V; pin 3 (OUT) → **VREF rail node**
- Wire 7 and cap 10 stay as-is — same node, same decoupling
- Re-meter the VREF rail; should read 4.096 V ± 8 mV (REF3040 is ±0.2% spec)
- Free up the TL072's channel A for use in the input/output stages

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
| Encoder | A | XIAO D1 (no external pull-up — firmware uses `INPUT_PULLUP`) |
| Encoder | B | XIAO D2 (same — internal pull-up) |
| Encoder | Common | GND |
| Encoder switch | One side | XIAO D7 (no external — internal pull-up) |
| Encoder switch | Other side | GND |
| Tempo pot | CW | +3.3 V |
| Tempo pot | Wiper | XIAO D0 |
| Tempo pot | CCW | GND |

See §3 for the policy on pull-ups (skipped on the bench, added on the PCB only for CS lines).

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
