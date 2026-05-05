# Bench wiring — gio platform rebuild

Single-source wiring reference for the breadboard rebuild covering Stories 012 (SPI bus), 013 (DAC output stage), 015 (input protection + scaling), plus the encoder pin migration. Use alongside `decisions.md` §25 and `hardware/bom.md`.

**Bench substitution removed (2026-05-04):** REF3040AIDBZR (SOT-23-3) now drives the VREF rail directly. The pot+TL072 stand-in is gone. VREF stability went from ±56 mV (pot+TL072 drift) to ±2 mV (essentially MCP3208 LSB noise). See §5 for the swap details.

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
| D0 | GP26 | CLOCK_IN (digital) | input | Reserved for J1 clock/gate edge detection. Freed 2026-05-02 when tempo pot moved to the MCP3208 (CH0 per the 2026-05-03 channel-layout convention). |
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

### Drop-in done — REF3040 on the bench (2026-05-04)

All of §5 above is **historical** — the pot+TL072 stand-in has been replaced. Current VREF source:

| Wire / part | From | To | Notes |
|---|---|---|---|
| 1 | REF3040 **pin 1 (VIN)** | +5 V rail | TI REF30xx series in DBZ (SOT-23-3) package |
| 2 | REF3040 **pin 2 (GND)** | GND rail | |
| 3 | REF3040 **pin 3 (VOUT)** | **VREF rail node** | Same node that feeds DAC8552 pin 2, MCP3208 pin 15, all four bias dividers, pot CW |
| 4 | 100 nF cap | REF3040 pin 1 → GND | Input decoupling, close to chip |
| 5 | **1 µF cap** | REF3040 pin 3 → GND | **Required for stability** per datasheet |

**Verified at the bench (2026-05-04):** VREF rail reads 4.094–4.096 V across a 4 s window (±2 mV total — essentially MCP3208 LSB noise). Pot+TL072 stand-in fully removed; TL072 #1 also pulled (REF3040's 25 mA output drives the ~1 mA total load directly without buffering). This matches the production PCB topology exactly.

**Note on pin numbers:** the SOT-23-3 pin assignment varies by manufacturer; the table above is per TI's REF3040 datasheet (DBZ package). Always verify against the part's datasheet before powering on.

---

## 6. DAC output stage (per channel — 2× identical)

Inverting amplifier (with offset on the non-inverting input) turns the DAC's 0..4.096 V swing into a ±9 V Eurorack-friendly bipolar output, then protects the jack with a 1 kΩ series resistor and a BAT43 clamp pair to ±12 V.

**One TL072 IC handles both channels** — channel A drives DAC OUTA → jack J3; channel B drives DAC OUTB → jack J4. No parking needed because both channels are active.

(See §5 for the TL072 DIP-8 pinout reference. We're using a *second* TL072 IC here — call it TL072 #2.)

### Topology — why offset is on pin 3, not pin 2

A naive inverting summing amp with both VDAC and VREF on the inverting input (pin 2) **doesn't work** with a single positive reference: both contributions invert, and `Vout = −(R_fb/R_in)·VDAC − (R_fb/R_off)·VREF` rails to negative for any DAC value (validated empirically 2026-04-29 — see `bench-log.md`).

The fix is to put the offset reference on the **non-inverting input (pin 3)** via a divider. That makes the offset contribution *non-inverting* (positive at the output) while VDAC stays *inverting* (negative-going at the output), giving a real bipolar swing from a single positive reference.

### Resistor values

| Symbol | Where | Value (E96 / E12) | Purpose |
|---|---|---|---|
| R_in | DAC pin 4 → pin 2 | **10 kΩ** | Sets gain magnitude with R_fb |
| R_fb | pin 1 → pin 2 | **48.7 kΩ** (or 47 kΩ E12; or 2× 22 kΩ in series = 44 kΩ) | Feedback resistor — sets gain = R_fb / R_in |
| R_div_top | VREF rail → pin 3 | **22 kΩ** | Top leg of offset divider |
| R_div_bot | pin 3 → GND | **15 kΩ** (or 14.7 kΩ E96) | Bottom leg of offset divider |
| R_jack_series | pin 1 → jack tip | **1 kΩ** | Output isolation / short-circuit current limit |

Divider sets V_pin3 = VREF · R_div_bot/(R_div_top + R_div_bot) ≈ **1.66 V** with the values above.

### Math

Inverting amp with non-inverting offset:

```
Vout = (1 + R_fb/R_in)·V_pin3 − (R_fb/R_in)·VDAC
```

With R_fb = 44 kΩ (2× 22 kΩ E12), R_in = 10 kΩ, V_pin3 = 1.66 V, VREF = 4.096 V:

```
Vout = 5.4·1.66 − 4.4·VDAC ≈ 8.96 − 4.4·VDAC
```

Target mapping:

| DAC voltage | Op-amp output (= jack) |
|---|---|
| 0 V         | **≈ +8.96 V**  (max positive) |
| 2.048 V     | **≈ 0 V**      (midpoint) |
| 4.096 V     | **≈ −9.06 V**  (max negative) |

Firmware compensates the inversion in `outputs::write()` calibration (gain ≈ −1/4.4 scaled to volts; offset ≈ +V_pin3·gain; bench-fit per channel).

If you want closer to the spec'd ±10 V (instead of ±9 V), use R_fb = 48.7 kΩ E96 — gain rises to 4.87, output swing widens proportionally.

### Wire list — channel A (DAC OUTA → jack J3)

Channel A pins on TL072 #2: pin 1 (1OUT), pin 2 (1IN−), pin 3 (1IN+).

| # | From | To | Notes |
|---|---|---|---|
| A1 | TL072 #2 pin 8 (V+) | +12 V rail | Op-amp positive supply (shared by both channels) |
| A2 | TL072 #2 pin 4 (V−) | −12 V rail | Op-amp negative supply (shared) |
| A3 | TL072 #2 pin 8 → GND | 100 nF cap | V+ decoupling |
| A4 | TL072 #2 pin 4 → GND | 100 nF cap | V− decoupling |
| A5 | VREF rail | One end of R_div_top (22 kΩ) | Top leg of the pin-3 offset divider |
| A6 | Other end of R_div_top | TL072 #2 pin 3 (1IN+) | Pin 3 is the divider's mid-point |
| A7 | TL072 #2 pin 3 (1IN+) | One end of R_div_bot (15 kΩ) | Bottom leg of the divider |
| A8 | Other end of R_div_bot | GND rail | Closes the divider — V_pin3 ≈ 1.66 V |
| A9 | DAC8552 pin 4 (VOUTA) | One end of R_in (10 kΩ) | DAC signal into the summing node |
| A10 | Other end of R_in | TL072 #2 pin 2 (1IN−) | Summing node — also receives R_fb |
| A11 | TL072 #2 pin 1 (1OUT) | One end of R_fb (44 kΩ via 2× 22 kΩ) | Feedback resistor |
| A12 | Other end of R_fb | TL072 #2 pin 2 (1IN−) | Closes the feedback loop |
| A13 | TL072 #2 pin 1 (1OUT) | One end of R_jack_series (1 kΩ) | Output isolation resistor |
| A14 | Other end of R_jack_series | Jack J3 tip | The actual ±9 V output |
| A15 | BAT43 #1 anode | Jack J3 tip | Clamp diode for positive overshoot |
| A16 | BAT43 #1 cathode | +12 V rail | When jack > +12 V + Vf, this diode conducts → pulls jack down to +12 V |
| A17 | BAT43 #2 cathode | Jack J3 tip | Clamp diode for negative overshoot |
| A18 | BAT43 #2 anode | −12 V rail | When jack < −12 V − Vf, this diode conducts → pulls jack up to −12 V |
| A19 | Jack J3 sleeve | GND rail | Standard mono-jack ground |

### Wire list — channel B (DAC OUTB → jack J4)

Channel B pins on TL072 #2: pin 5 (2IN+), pin 6 (2IN−), pin 7 (2OUT). Power and decoupling are **shared** with channel A — no need to repeat A1–A4. The pin-3 offset divider can also be **shared** with channel A's pin 5: tie pin 5 to pin 3, and skip a separate divider.

| # | From | To | Notes |
|---|---|---|---|
| B1 | TL072 #2 pin 5 (2IN+) | TL072 #2 pin 3 (1IN+) | Share channel A's offset divider |
| B2 | DAC8552 pin 3 (VOUTB) | One end of R_in (10 kΩ) | New 10 kΩ resistor |
| B3 | Other end of R_in | TL072 #2 pin 6 (2IN−) | Summing node |
| B4 | TL072 #2 pin 7 (2OUT) | One end of R_fb (44 kΩ) | New 44 kΩ (or 47 kΩ E12, or 48.7 kΩ E96) |
| B5 | Other end of R_fb | TL072 #2 pin 6 (2IN−) | Closes the loop |
| B6 | TL072 #2 pin 7 (2OUT) | One end of R_jack_series (1 kΩ) | New 1 kΩ |
| B7 | Other end of R_jack_series | Jack J4 tip | |
| B8 | BAT43 #3 anode | Jack J4 tip | Positive clamp |
| B9 | BAT43 #3 cathode | +12 V rail | |
| B10 | BAT43 #4 cathode | Jack J4 tip | Negative clamp |
| B11 | BAT43 #4 anode | −12 V rail | |
| B12 | Jack J4 sleeve | GND rail | |

> **Diode polarity sanity check:** for the positive clamp, the diode has its **anode at the jack** (the side that might overshoot up) and **cathode at the +12 V rail** (the safe ceiling). When the jack tries to go above +12 V + Vf (~+12.3 V), the diode forward-biases and dumps the excess into the rail. The negative clamp is the mirror image. If you wire a clamp backward, it'll either do nothing (just float reverse-biased forever) or short the rail to GND through the op-amp output — measure carefully before powering up.

### Schematic view (channel A; channel B is identical)

```
  DAC8552 pin 4 (VOUTA, 0..4.096V)            +12V    GND
            │                                    │      │
        ┌───┴───┐                                │      ┴── 100nF
        │ R_in  │                                │
        │ 10kΩ  │                              ┌─┴──┐
        └───┬───┘                              │ 8  │V+
            │            ┌──── R_fb 44kΩ ────┐ │    │
            │            │                   │ │    │
            │            │                   └─│ 2  │1IN−   ╲
            └──────── pin 2 (1IN−) ────────────│    │       ─── pin 1 (1OUT)
                                               │    │       ╱        │
                                               │    │                │
   VREF rail ─[R_div_top 22kΩ]─┐               │    │                │
                                ├── pin 3 ─────│ 3  │1IN+            │
                  GND ─[R_div_bot 15kΩ]─┘      │    │                │
                                               │ 4  │V−              │
                                               └─┬──┘                │
                                                 │                   │
                                                -12V                 │
                                                 │                   │
                                                 ┴── 100nF           │
                                                                     │
                                        ┌── R_jack_series (1kΩ) ─────┘
                                        │
                                        ├──[BAT43 anode→J3, cathode→+12V]── (clamp +)
                                        ├──[BAT43 cathode→J3, anode→−12V]── (clamp −)
                                        │
                                        └─── J3 tip (Eurorack OUT 1)
                                             J3 sleeve → GND

  Channel B reuses the same offset divider — pin 5 (2IN+) tied directly to pin 3.
```

### Bench procedure

1. **Build channel A only** (wires A1–A19). Leave channel B + jack J4 for after channel A passes.
2. **Power up.** First sanity check: meter pin 3 → should read **≈ +1.66 V** (the divider). Then meter pin 2 → should read the same as pin 3 within a few mV (op-amp's "virtual short" via feedback).
3. **Don't connect DAC8552 pin 4 yet.** Instead, jumper the R_in input end (where DAC pin 4 will land) to **GND** — this simulates VDAC = 0.
4. **Meter on jack J3 tip** — should read **≈ +9 V** (precise value depends on your divider and R_fb).
5. Move the jumper from GND to **VREF rail (+4.096 V)** — simulates VDAC = 4.096 V.
6. **Re-meter J3** — should read **≈ −9 V**.
7. *(Optional)* Move the jumper to a midpoint voltage near +2 V (any divider tap or matched-resistor pair) — J3 should read **≈ 0 V**.
8. **Now connect DAC8552 pin 4** to the R_in input end (wire A9). Flash the smoke-test firmware:
   ```
   pio run -d firmware/arp -e seeed-xiao-rp2350-smoketest --target upload
   ```
   The 1 Hz triangle on the DAC will produce a 1 Hz triangle at J3, swinging ≈ ±9 V.
9. Once channel A is solid, **remove the channel-B parking wires** (pin 5 to GND, pin 6 to pin 7) and add the channel-B wiring (B1–B12 above).

### Calibration

Once both channels track linearly, fill in the firmware calibration in `outputs::setCalibration(channel, gain, offset)`. The transfer function is `Vjack = (1 + R_fb/R_in)·V_pin3 − (R_fb/R_in)·VDAC`, so:

- Pick two known DAC voltages (e.g., 1.0 V and 3.0 V), measure the resulting jack voltage with a multimeter
- Solve for the linear `Vjack = gain·VDAC + offset` per channel
- The firmware applies the inverse mapping when you call `outputs::write(channel, target_jack_volts)` — it computes the DAC voltage that produces the target jack output

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

### Topology — same offset-on-pin-3 trick as §6, with attenuation

Mirror of §6's output stage in reverse: a single op-amp inverting stage with the bias offset applied through a divider on the **non-inverting input (pin 3)**, not on the inverting input. The §6 lesson applies — combining VDAC and VREF on a single inverting node with a single positive supply doesn't produce the bipolar mapping we want.

There's also a **second math trap specific to this stage**: R_series and R_in2 are *in series* between the jack and the op-amp's virtual-ground inverting input. They look like a single combined input resistor of value `R_series + R_in2`. The gain is therefore `R_fb / (R_series + R_in2)`, **not** `R_fb / R_in2` — easy to miss when the topology has two distinct-looking resistors. (Caught at the bench 2026-04-30 — see `bench-log.md`.)

### Resistor values

| Symbol | Where | Value (E96 / E12) | Purpose |
|---|---|---|---|
| R_series | jack tip → node A | **100 kΩ** | Jack input impedance + clamp current limit (±15 V abuse → 30 µA, BAT43 sees < 0.02 % of its rating) |
| R_in2 | node A → pin 2 | **22 kΩ** | Second input resistor; sets gain in series with R_series |
| R_fb | pin 1 → pin 2 | **22 kΩ** | Feedback — sets gain = R_fb / (R_series + R_in2) ≈ 0.180 |
| R_div_top | VREF → pin 3 | **22 kΩ** | Top leg of offset divider |
| R_div_bot | pin 3 → GND | **15 kΩ** (or 14.7 kΩ E96) | Bottom leg of offset divider |

Divider sets V_pin3 = VREF · R_div_bot / (R_div_top + R_div_bot) ≈ **1.66 V**.

### Math

```
Vout = (1 + R_fb/(R_series + R_in2)) · V_pin3 − (R_fb/(R_series + R_in2)) · V_jack
     = 1.180 · 1.66 − 0.180 · V_jack
     ≈ 1.96 − 0.180 · V_jack
```

Target mapping at the ADC (with VREF = 4.096 V):

| Jack input | Op-amp output → ADC | ADC count (12-bit) |
|---|---|---|
| +10 V  | ≈ +0.16 V | ≈ 160 |
| +5 V   | ≈ +1.06 V | ≈ 1060 |
| 0 V    | ≈ +1.96 V | ≈ 1960 |
| −5 V   | ≈ +2.86 V | ≈ 2860 |
| −10 V  | ≈ +3.76 V | ≈ 3760 |

Uses ~88 % of the ADC range with margin at both ends — preferred over a tighter ±10 V → 0..4.096 V mapping that would risk clipping at the rails. Firmware reverses the inversion + offset in `inputs::readVolts()` calibration; bench-fit per channel.

### Wire list — channel A (jack J1 → MCP3208 CH1)

Channel A pins on TL072 #3: pin 1 (1OUT), pin 2 (1IN−), pin 3 (1IN+).

| # | From | To | Notes |
|---|---|---|---|
| 1 | TL072 #3 pin 8 (V+) | +12 V rail | Op-amp positive supply |
| 2 | TL072 #3 pin 4 (V−) | −12 V rail | Op-amp negative supply |
| 3 | TL072 #3 pin 8 → GND | 100 nF cap | V+ decoupling |
| 4 | TL072 #3 pin 4 → GND | 100 nF cap | V− decoupling |
| 5 | VREF rail | One end of R_div_top (22 kΩ) | Top leg of pin-3 offset divider |
| 6 | Other end of R_div_top | TL072 #3 pin 3 (1IN+) | Pin 3 is the divider's mid-point |
| 7 | TL072 #3 pin 3 (1IN+) | One end of R_div_bot (15 kΩ) | Bottom leg of divider |
| 8 | Other end of R_div_bot | GND rail | Closes the divider — V_pin3 ≈ 1.66 V |
| 9 | Jack J1 tip | One end of R_series (100 kΩ) | Jack feeds the protection / scaling chain |
| 10 | Other end of R_series | "node A" (a free row on the breadboard) | Node A is the input-protection junction point |
| 11 | BAT43 #5 cathode | Node A | Clamp diode for **negative** undershoot |
| 12 | BAT43 #5 anode | −12 V rail | When node A < −12 V − Vf, this diode forward-biases and pulls node A back up |
| 13 | BAT43 #6 anode | Node A | Clamp diode for **positive** overshoot |
| 14 | BAT43 #6 cathode | +12 V rail | When node A > +12 V + Vf, this diode forward-biases and pulls node A back down |
| 15 | Node A | One end of R_in2 (22 kΩ) | Signal into the summing node via R_in2 |
| 16 | Other end of R_in2 | TL072 #3 pin 2 (1IN−) | Summing node — also receives R_fb |
| 17 | TL072 #3 pin 1 (1OUT) | One end of R_fb (22 kΩ) | Feedback resistor |
| 18 | Other end of R_fb | TL072 #3 pin 2 (1IN−) | Closes the feedback loop |
| 19 | TL072 #3 pin 1 (1OUT) | **MCP3208 pin 2 (CH1)** | J1 lives at CH1 per the 2026-05-03 channel-layout convention (CH0 = pot, CH1/CH2 = jacks). |
| 20 | Jack J1 sleeve | GND rail | Standard mono-jack ground |
| **Park unused channel B** | | | |
| 21 | TL072 #3 pin 5 (2IN+) | GND rail | Channel B input grounded |
| 22 | TL072 #3 pin 6 (2IN−) | TL072 #3 pin 7 (2OUT) | Channel B unity-gain feedback so output sits at 0 V |

When you're ready to add **channel B** (jack J2 → **MCP3208 CH2**, pin 3), remove wires 21–22 and add a parallel set of wires repeating the input/feedback portion (wires 9–19 above) using channel B's pin numbers (5/6/7) and routing the output to **MCP3208 pin 3 (CH2)**. Channel B can share channel A's offset divider via pin 5 → pin 3 — no second divider needed (mirror of the §6 channel-B trick).

> **Diode polarity sanity check (mirrored from §6):** the input clamps go on **node A**, *after* the 100 kΩ series resistor — not directly at the jack. This is intentional: the series resistor limits clamp current to safe levels (30 µA worst case), and node A is a low-current point where the clamp can hold the voltage without burning the diode out. Polarity at node A is the same logic as §6: anode-to-jack-side / cathode-to-rail for positive overshoot; cathode-to-jack-side / anode-to-rail for negative.

### Schematic view (channel A)

```
  Jack J1 tip                                  +12V        GND
        │                                        │          │
   ┌────┴─────┐                                  │          ┴── 100nF
   │ R_series │                                  │
   │  100 kΩ  │                                ┌─┴──┐
   └────┬─────┘                                │ 8  │V+
        │ (node A)                             │    │
        ├──[BAT43 anode → +12V; cathode → A]── │    │
        ├──[BAT43 cathode → −12V; anode → A]── │    │
        │                                      │    │
   ┌────┴─────┐                                │    │      ╲
   │ R_in2    │                                │ 2  │1IN−   ─── pin 1 (1OUT)
   │  22 kΩ   │                                │    │      ╱       │
   └────┬─────┘                                │    │              │
        │                  ┌── R_fb 22kΩ ──┐   │    │              │
        └──── pin 2 (1IN−) ┤                │   │ 4  │V−            │
                            │                │   └─┬──┘              │
                            └── pin 1 (1OUT)─┘     │                 │
                                                  -12V               │
                                                   │                 │
                                                   ┴── 100nF         │
                                                                     │
   VREF rail ─[R_div_top 22kΩ]─┐                                     │
                                ├── pin 3 (1IN+)                     │
                  GND ─[R_div_bot 15kΩ]─┘                            │
                                                                     │
                                              MCP3208 pin 2 (CH1) ───┘

   Channel B (unused on first session) — parked as in §5:
      TL072 #3 pin 5 (2IN+) ── GND
      TL072 #3 pin 6 (2IN−) ── TL072 #3 pin 7 (2OUT)
```

### Bench procedure

1. **Wire channel A only** (wires 1–20). Park channel B with wires 21–22.
2. **Power up.** Confirm ±12 V and the +5 V / VREF rails are still clean. Sanity-check pin 3 — should read **≈ +1.66 V** (the divider). Pin 2 should track pin 3 within a few mV (op-amp's "virtual short" via feedback).
3. **First test — jack J1 tip jumpered to GND.** Meter at TL072 #3 pin 1 (1OUT). Should read **≈ +1.96 V** — this is the bias-only output: `(1 + R_fb/(R_series+R_in2)) · V_pin3 = 1.180 · 1.66 = 1.96`. **Note:** with the jack truly floating, no current can flow through R_series + R_in2, so no offset gain develops and pin 1 sits at V_pin3 ≈ +1.66 V instead. The +1.96 V reading only appears when the jack is **actively grounded** — that's the test condition. (Caught 2026-05-02 — see `bench-log.md`.)
4. **Second test — apply +5 V to jack J1 tip** from a bench supply (or through R_series from any +5 V source).
   - Expected output at TL072 #3 pin 1: **≈ +1.06 V** (`Vout = 1.96 − 0.180·5 = 1.06`).
5. **Third test — apply −5 V to jack J1 tip.**
   - Expected output: **≈ +2.86 V** (`Vout = 1.96 − 0.180·(−5) = 2.86`).
6. **If the slope between the +5 and −5 readings is ≈ −0.18 V per V of input**, the stage is working — even if absolute offsets are off by 5–10 % due to E12 substitutions and pot dial-in.
7. **Run the smoke-test firmware.** The reported `adc_v` should sit near 1.96 V with no input. Patch a Eurorack cable from J3 → J1 to close the loop, and `adc_v` will sweep predictably with the DAC's triangle (ranging roughly 0.4 V to 3.6 V over each 1-second cycle).
8. **Calibrate** by applying two known voltages (e.g., +5 V and −5 V), reading the resulting `adc_count`, and computing the per-channel `gain` and `offset` for `inputs::setCalibration()`.
9. **Once channel A passes**, remove wires 21–22 and add the channel-B input/feedback wires per the note above.

---

## 8. OLED + encoder + tempo pot

| Device | Pin | Net |
|---|---|---|
| OLED | VCC | +3.3 V |
| OLED | GND | GND |
| OLED | SDA | XIAO D4 |
| OLED | SCL | XIAO D5 |
| Encoder | A | XIAO **D1** (no external pull-up — firmware uses `INPUT_PULLUP`) |
| Encoder | B | XIAO **D2** (same — internal pull-up) |
| Encoder | Common | **GND** — required; without it, A and B float and the decoder sees no transitions |
| Encoder switch | One side | XIAO D7 (no external — internal pull-up) |
| Encoder switch | Other side | GND |

> **Encoder A/B history:** the bench encoder originally read CW as a *negative* delta with the schematic-natural A→D1, B→D2 wiring (caught 2026-05-02). The firmware was first patched to swap the constants (`PIN_ENC_A = D2`), but on 2026-05-04 the breadboard wires were physically swapped instead so the firmware and schematic agree on **A→D1, B→D2** (CW = positive). If a future encoder reads inverted, swap the constants in `src/main.cpp` and `src/main_smoketest.cpp` rather than re-routing the breadboard or PCB.
| Tempo pot | CW | **VREF rail** (4.096 V) — *not* +3.3 V; routes through MCP3208 |
| Tempo pot | Wiper | **MCP3208 pin 1 (CH0)** — *not* XIAO D0 (changed 2026-05-02; landed at CH0 per the 2026-05-03 channel-layout convention) |
| Tempo pot | CCW | GND |

**Why tempo on the MCP3208 instead of XIAO D0:** unifies all analog inputs through the precision REF3040 reference (one calibration story, no mixing of XIAO native ADC + external ADC), and frees D0 for J1 clock/gate edge detection. Cost: one extra SPI read per loop (microseconds — negligible). Pot CW must tie to **VREF**, not +5 V or +3.3 V, so the wiper's full sweep maps cleanly to the ADC's 0..VREF range without clipping.

See §3 for the policy on pull-ups (skipped on the bench, added on the PCB only for CS lines).

---

## 9. Jack assignments (per `decisions.md` §23: generic IN/OUT labels)

| Jack | Direction | Bench connection | App use (arp) |
|---|---|---|---|
| J1 | IN 1 | Input stage ch A → MCP3208 **CH1** | CV in / clock-trigger |
| J2 | IN 2 | Input stage ch B → MCP3208 **CH2** | CV in (transpose) / clock-trigger |

**MCP3208 channel layout** (per `decisions.md` §26): CH0 = pot, CH1 = J1, CH2 = J2, CH3–CH7 = expansion.
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
5. **MCP3208 (§4)** — load `lib/inputs/`, jumper DAC OUTA directly to any free MCP3208 channel (skip the input scaling stage), confirm count tracks DAC value
6. **Output stage (§6)** — insert TL072 + resistors between DAC and J3, confirm ±10 V swing at jack
7. **Input scaling (§7)** — wire jack J1 through input stage to MCP3208 **CH1**; bench-supply known voltages, confirm `inputs.readVolts()` is within tolerance
8. **OLED + encoder + tempo pot (§8)** — re-attach UI; verify menu navigation works on the new pin assignments
9. **Loopback** — jumper J3 → J1, run a sweep, confirm closed-loop accuracy

Each step has an entry in `bench-log.md` waiting for measurements + observations.
