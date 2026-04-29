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
| 11 | TL072 pin 5 (2IN+) | GND rail | **Park unused channel B** — input held at 0 V |
| 12 | TL072 pin 6 (2IN−) | TL072 pin 7 (2OUT) | **Park unused channel B** — unity-gain feedback so output settles at 0 V |

> **Why wires 11 + 12:** the TL072 is a dual op-amp; if you wire only channel A, the unused channel B's inputs (pins 5 and 6) are floating. Floating CMOS-style inputs pick up noise that puts the IC into uncontrolled saturation or oscillation, and that disturbance couples into channel A through shared substrate and power pins — degrading the VREF you just worked to clean up. Standard fix: tie the unused channel as a grounded unity-gain follower (input to GND, output to inverting input). Two short wires, no extra parts.

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

   Channel B (unused) — parked as grounded unity-gain follower:

      GND ──── pin 5 (2IN+)
              ┌─ pin 7 (2OUT) ─── (no external connection — sits at 0 V)
              │
              └── pin 6 (2IN−)
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

Inverting summing amplifier turns the DAC's 0..4.096 V swing into a ±10 V Eurorack-friendly bipolar output, then protects the jack with a 1 kΩ series resistor and a BAT43 clamp pair to ±12 V.

**One TL072 IC handles both channels** — channel A drives DAC OUTA → jack J3; channel B drives DAC OUTB → jack J4. No parking needed because both channels are active.

(See §5 for the TL072 DIP-8 pinout reference. We're using a *second* TL072 IC here — call it TL072 #2.)

### Resistor values (E96 nominals; E12 substitutes in parens)

- **R_in = 10 kΩ** (E12 ✓ — same value works either way)
- **R_off = 20 kΩ** (E96; or 22 kΩ E12 — gives ~9.07 V offset instead of 9.97 V; either bench-tune R_fb upward to compensate, or accept slightly less than ±10 V swing)
- **R_fb = 48.7 kΩ** (E96; or 47 kΩ E12 — gives gain ≈ 4.7 instead of 4.87, swing ≈ ±9.6 V at the jack; fine for bench)
- **R_jack_series = 1 kΩ** (output protection)

### Math

Inverting summing-amp transfer function:
```
Vout = (R_fb/R_off)·VREF − (R_fb/R_in)·VDAC
```

Target mapping (with E96 values, VREF = 4.096 V):

| DAC voltage | Op-amp output | Jack output |
|---|---|---|
| 0 V         | +9.97 V  | +9.97 V (≈ +10 V) |
| 2.048 V     | 0 V      | 0 V |
| 4.096 V     | −9.97 V  | −9.97 V (≈ −10 V) |

Firmware compensates the inversion in `outputs::write()` calibration (gain = −1, offset = +VREF/2 by default; bench-fit to actual values).

### Wire list — channel A (DAC OUTA → jack J3)

Channel A pins on TL072 #2: pin 1 (1OUT), pin 2 (1IN−), pin 3 (1IN+).

| # | From | To | Notes |
|---|---|---|---|
| A1 | TL072 #2 pin 8 (V+) | +12 V rail | Op-amp positive supply (shared by both channels) |
| A2 | TL072 #2 pin 4 (V−) | −12 V rail | Op-amp negative supply (shared) |
| A3 | TL072 #2 pin 8 → GND | 100 nF cap | V+ decoupling |
| A4 | TL072 #2 pin 4 → GND | 100 nF cap | V− decoupling |
| A5 | TL072 #2 pin 3 (1IN+) | GND rail | Non-inverting input grounded — sets summing-amp virtual-ground reference |
| A6 | DAC8552 pin 4 (VOUTA) | One end of R_in (10 kΩ) | DAC signal into the summing node via R_in |
| A7 | Other end of R_in | TL072 #2 pin 2 (1IN−) | "Summing node" — also receives R_off and R_fb |
| A8 | VREF rail | One end of R_off (20 kΩ) | Reference into the summing node via R_off |
| A9 | Other end of R_off | TL072 #2 pin 2 (1IN−) | Same summing node |
| A10 | TL072 #2 pin 1 (1OUT) | One end of R_fb (48.7 kΩ) | Feedback resistor |
| A11 | Other end of R_fb | TL072 #2 pin 2 (1IN−) | Closes the feedback loop |
| A12 | TL072 #2 pin 1 (1OUT) | One end of R_jack_series (1 kΩ) | Output isolation resistor |
| A13 | Other end of R_jack_series | Jack J3 tip | The actual ±10 V output |
| A14 | BAT43 #1 anode | Jack J3 tip | Clamp diode for positive overshoot |
| A15 | BAT43 #1 cathode | +12 V rail | When jack > +12 V + Vf, this diode conducts → pulls jack down to +12 V |
| A16 | BAT43 #2 cathode | Jack J3 tip | Clamp diode for negative overshoot |
| A17 | BAT43 #2 anode | −12 V rail | When jack < −12 V − Vf, this diode conducts → pulls jack up to −12 V |
| A18 | Jack J3 sleeve | GND rail | Standard mono-jack ground |

### Wire list — channel B (DAC OUTB → jack J4)

Channel B pins on TL072 #2: pin 5 (2IN+), pin 6 (2IN−), pin 7 (2OUT). Power and decoupling are **shared** with channel A — no need to repeat A1–A4.

| # | From | To | Notes |
|---|---|---|---|
| B1 | TL072 #2 pin 5 (2IN+) | GND rail | Non-inverting input grounded |
| B2 | DAC8552 pin 3 (VOUTB) | One end of R_in (10 kΩ) | New 10 kΩ resistor |
| B3 | Other end of R_in | TL072 #2 pin 6 (2IN−) | Summing node |
| B4 | VREF rail | One end of R_off (20 kΩ) | New 20 kΩ resistor |
| B5 | Other end of R_off | TL072 #2 pin 6 (2IN−) | Same summing node |
| B6 | TL072 #2 pin 7 (2OUT) | One end of R_fb (48.7 kΩ) | New 48.7 kΩ |
| B7 | Other end of R_fb | TL072 #2 pin 6 (2IN−) | Closes the loop |
| B8 | TL072 #2 pin 7 (2OUT) | One end of R_jack_series (1 kΩ) | New 1 kΩ |
| B9 | Other end of R_jack_series | Jack J4 tip | |
| B10 | BAT43 #3 anode | Jack J4 tip | Positive clamp |
| B11 | BAT43 #3 cathode | +12 V rail | |
| B12 | BAT43 #4 cathode | Jack J4 tip | Negative clamp |
| B13 | BAT43 #4 anode | −12 V rail | |
| B14 | Jack J4 sleeve | GND rail | |

> **Diode polarity sanity check:** for the positive clamp, the diode has its **anode at the jack** (the side that might overshoot up) and **cathode at the +12 V rail** (the safe ceiling). When the jack tries to go above +12 V + Vf (~+12.3 V), the diode forward-biases and dumps the excess into the rail. The negative clamp is the mirror image. If you wire a clamp backward, it'll either do nothing (just float reverse-biased forever) or short the rail to GND through the op-amp output — measure carefully before powering up.

### Schematic view (channel A; channel B is identical)

```
  DAC8552 pin 4 (VOUTA, 0..4.096V)              +12V    GND
            │                                      │      │
        ┌───┴───┐                                  │      ┴── 100nF
        │ R_in  │                                  │
        │ 10kΩ  │                                ┌─┴──┐
        └───┬───┘                                │ 8  │V+
            │     ┌───────────────┐              │    │
   VREF ────┤     │   R_fb 48.7kΩ │              │ 3  │1IN+ ── GND
   rail     │     └──┬──────┬─────┘              │    │
   (4.096V) │        │      │                    │    │      ╲
        ┌───┴────────┴──────┴──── pin 2 (1IN−) ──│ 2  │1IN−   ─── pin 1 (1OUT)
        │ R_off                                  │    │      ╱       │
        │ 20kΩ                                   │    │              │
        └───┬───── ✗  (no — see wire A8/A9)      │ 4  │V−            │
            │                                    └─┬──┘              │
           GND                                     │                 │
                                                  -12V               │
                                                   │                 │
                                                   ┴── 100nF         │
                                                                     │
                                       ┌───── R_jack_series (1kΩ) ───┘
                                       │
                                       ├──[BAT43 anode→J3, cathode→+12V]── (clamp +)
                                       ├──[BAT43 cathode→J3, anode→−12V]── (clamp −)
                                       │
                                       └──── J3 tip (Eurorack OUT 1)
                                            J3 sleeve → GND
```

### Bench procedure

1. **Build channel A only** (wires A1–A18). Leave the other DAC channel + jack J4 for after channel A passes.
2. **Power up**, but **don't connect the DAC8552 OUTA wire (A6) yet.** Instead, jumper the R_in input end to GND (i.e., simulate VDAC = 0).
3. **Meter on jack J3 tip** — should read ≈ +10 V (whatever your R_off / R_fb ratio gives).
4. Move the jumper from GND to +VREF (i.e., simulate VDAC = 4.096 V).
5. **Re-meter J3** — should read ≈ −10 V.
6. Move the jumper to a +2.048 V source (a divider tap, or carefully tap the wiper of the VREF trimpot if accessible).
7. **Re-meter J3** — should read ≈ 0 V.
8. **Now connect wire A6 (DAC OUTA)** and run the smoke-test firmware. The triangle wave on the DAC should produce a triangle on J3 with the same amplitude relationship.
9. Repeat steps 1–8 for channel B → jack J4.

### Calibration

Once both channels track linearly, fill in the firmware calibration in `outputs::setCalibration(channel, gain, offset)`:

- Pick two known DAC voltages (e.g., 1.0 V and 3.0 V), measure the resulting jack voltage with a multimeter
- Solve the two equations for the actual `gain` (≈ −R_fb/R_in) and `offset` (≈ +(R_fb/R_off)·VREF) per channel
- Update the `setCalibration()` calls in your firmware

---

## 7. Input protection + scaling (per channel — 2× identical)

Mirror of the output stage in reverse: a protected, attenuated, biased ±10 V jack input → 0..4.096 V at the ADC.

**Topology, in order from jack to ADC:**

1. 100 kΩ series resistor at the jack (limits clamp current under abuse)
2. BAT43 dual clamp to ±12 V rails right after the series resistor (at "node A")
3. Inverting summing amplifier (R_in2 in, R_off offset from VREF, R_fb feedback)
4. Op-amp output → MCP3208 channel pin

**IC allocation:** one TL072 channel per ADC input. We'll use a **third TL072 IC (TL072 #3)** — channel A for input 1, channel B for input 2. (TL072 #1's channel A is the VREF buffer; channel B is parked. TL072 #2 is fully consumed by the DAC outputs.)

**For first bench session: wire input 1 only.** Channel B on TL072 #3 stays parked until you've confirmed input 1 tracks correctly via known voltages.

(See §5 for TL072 DIP-8 pinout.)

### Resistor values (E96 nominals; E12 substitutes in parens)

- **R_series = 100 kΩ** (jack input impedance + clamp current limit; max abuse at ±15 V → 30 µA → BAT43 sees < 0.02 % of its 200 mA rating)
- **R_in2 = 22 kΩ** (E12 ✓ — sets the inverting-amp gain)
- **R_off = 9.4 kΩ** (use 9.1 kΩ E12, or two 4.7 kΩ in series — bench-tune R_fb if exact mapping matters)
- **R_fb = 4.7 kΩ** (E12 ✓)

### Math

```
Vout = (R_fb/R_off)·VREF − (R_fb/R_in2)·Vin
```

With the values above and VREF = 4.096 V:

| Jack input | Op-amp output → ADC | ADC count (12-bit) |
|---|---|---|
| +10 V | ≈ 0 V    | ≈ 0 |
| 0 V   | ≈ 2.048 V | ≈ 2048 |
| −10 V | ≈ 4.096 V | ≈ 4095 |

Firmware reverses the inversion in `inputs::readVolts()` calibration (gain ≈ −R_in2/R_fb scaled to volts; offset = +(R_in2/R_off)·VREF; bench-fit per channel).

### Wire list — channel A (jack J1 → MCP3208 ch 0)

Channel A pins on TL072 #3: pin 1 (1OUT), pin 2 (1IN−), pin 3 (1IN+).

| # | From | To | Notes |
|---|---|---|---|
| 1 | TL072 #3 pin 8 (V+) | +12 V rail | Op-amp positive supply |
| 2 | TL072 #3 pin 4 (V−) | −12 V rail | Op-amp negative supply |
| 3 | TL072 #3 pin 8 → GND | 100 nF cap | V+ decoupling |
| 4 | TL072 #3 pin 4 → GND | 100 nF cap | V− decoupling |
| 5 | TL072 #3 pin 3 (1IN+) | GND rail | Non-inverting input grounded — virtual-ground reference for the summing amp |
| 6 | Jack J1 tip | One end of R_series (100 kΩ) | Jack feeds the protection / scaling chain |
| 7 | Other end of R_series | "node A" (a free row on the breadboard) | Node A is the input-protection junction point |
| 8 | BAT43 #5 cathode | Node A | Clamp diode for **negative** undershoot |
| 9 | BAT43 #5 anode | −12 V rail | When node A < −12 V − Vf, this diode forward-biases and pulls node A back up |
| 10 | BAT43 #6 anode | Node A | Clamp diode for **positive** overshoot |
| 11 | BAT43 #6 cathode | +12 V rail | When node A > +12 V + Vf, this diode forward-biases and pulls node A back down |
| 12 | Node A | One end of R_in2 (22 kΩ) | Signal into the summing-amp input via R_in2 |
| 13 | Other end of R_in2 | TL072 #3 pin 2 (1IN−) | Summing node |
| 14 | VREF rail | One end of R_off (9.4 kΩ) | Reference into the summing node via R_off |
| 15 | Other end of R_off | TL072 #3 pin 2 (1IN−) | Same summing node |
| 16 | TL072 #3 pin 1 (1OUT) | One end of R_fb (4.7 kΩ) | Feedback resistor |
| 17 | Other end of R_fb | TL072 #3 pin 2 (1IN−) | Closes the feedback loop |
| 18 | TL072 #3 pin 1 (1OUT) | MCP3208 pin 1 (CH0) | The buffered, scaled, biased input voltage feeds ADC channel 0 |
| 19 | Jack J1 sleeve | GND rail | Standard mono-jack ground |
| **Park unused channel B** | | | |
| 20 | TL072 #3 pin 5 (2IN+) | GND rail | Channel B input grounded |
| 21 | TL072 #3 pin 6 (2IN−) | TL072 #3 pin 7 (2OUT) | Channel B unity-gain feedback so output sits at 0 V |

When you're ready to add **channel B** (jack J2 → MCP3208 ch 1), remove wires 20–21 and add a parallel set of wires repeating 5–18 with channel B's pin numbers (5/6/7) and using MCP3208 pin 2 (CH1) instead of pin 1.

> **Diode polarity sanity check (mirrored from §6):** the input clamps go on **node A**, *after* the 100 kΩ series resistor — not directly at the jack. This is intentional: the series resistor limits clamp current to safe levels (30 µA worst case), and node A is a low-current point where the clamp can hold the voltage without burning the diode out. Polarity at node A is the same logic as §6: anode-to-jack-side / cathode-to-rail for positive overshoot; cathode-to-jack-side / anode-to-rail for negative.

### Schematic view (channel A)

```
  Jack J1 tip                                +12V        GND
        │                                      │          │
   ┌────┴─────┐                                │          ┴── 100nF
   │ R_series │                                │
   │  100 kΩ  │                              ┌─┴──┐
   └────┬─────┘                              │ 8  │V+
        │ (node A)                           │    │
        ├──[BAT43 anode → +12V; cathode← ]── │ 3  │1IN+ ── GND
        ├──[BAT43 cathode → −12V; anode→ ]── │    │
        │                                    │    │      ╲
   ┌────┴─────┐                              │ 2  │1IN−   ─── pin 1 (1OUT)
   │ R_in2    │                              │    │      ╱       │
   │  22 kΩ   │                              │    │              │
   └────┬─────┘                              │ 4  │V−            │
        │                                    └─┬──┘              │
   VREF ┤                                      │                 │
   rail │                                     -12V               │
        │     ┌──── R_fb 4.7kΩ ───┐            │                 │
        │     │                    │           ┴── 100nF         │
   ┌────┴─────┴┐                   │                             │
   │ R_off     │                   │                             │
   │  9.4 kΩ   │                   │                             │
   └────┬──────┘                   │                             │
        │                          │                             │
        └──── pin 2 (1IN−) ────────┴─────────────────────────────┘
                                                                 │
                                                                 │
                         MCP3208 pin 1 (CH0) ───────────────────┘

   Channel B (unused on first session) — parked as in §5:
      TL072 #3 pin 5 (2IN+) ── GND
      TL072 #3 pin 6 (2IN−) ── TL072 #3 pin 7 (2OUT)
```

### Bench procedure

1. **Wire channel A only** (wires 1–19). Park channel B with wires 20–21.
2. **Power up.** Confirm ±12 V and the +5 V / VREF rails are still clean (the new IC shouldn't disturb them).
3. **First test — input grounded.** With nothing connected to jack J1 (or jack J1 tip jumpered to GND), meter on TL072 #3 pin 1 (1OUT). Should read **≈ +2.048 V** (the offset from VREF). If you see something near 0 V or rail-stuck, double-check wires 5 (1IN+ to GND) and 13/15 (summing-node connections).
4. **Second test — apply +5 V to jack J1 tip** from a bench supply (or through R_series from any +5 V source — be gentle).
   - Expected output at TL072 #3 pin 1: **≈ +1.02 V** ((9.4/4.7)·4.096 − (22/4.7·)5 → wait, math: math: Vout = (4.7/9.4)·4.096 − (4.7/22)·5 = 2.048 − 1.068 = 0.980 V — close enough; the resistor approximations matter)
5. **Third test — apply −5 V to jack J1 tip.**
   - Expected output: **≈ +3.12 V** (Vout = 2.048 − (4.7/22)·(−5) = 2.048 + 1.068 = 3.116 V)
6. **If steps 4 and 5 give roughly symmetric departures from the 2.048 V midpoint**, the stage is working — even if exact values are off by 5–10 % due to E12 substitutions.
7. **Run the smoke-test firmware** with input scaling enabled. The reported `adc_v` should track the applied jack voltage (modulo the inversion).
8. **Calibrate** by applying two known voltages (e.g., +5 V and −5 V), reading the resulting `adc_count`, and computing the per-channel `gain` and `offset` for `inputs::setCalibration()`.
9. **Once channel A passes**, remove wires 20–21 and replicate 5–18 for channel B → jack J2 → MCP3208 CH1 (pin 2).

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
