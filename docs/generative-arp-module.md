# Generative Arpeggiator — Eurorack CV/Gate Module

> **Rev 0.1 — April 2026** · 2 HP · XIAO RP2350 · MCP4725 DAC · 4 Jacks · 1 Pot · 1 Encoder · 64×32 OLED

## 1. Project Overview

A 2 HP Eurorack generative arpeggiator module producing scale-quantised V/Oct pitch CV and gate outputs, driven by a probability-layered generative engine.

Ported from [`xiao-ra4m1-arp`](https://github.com/funkfinger/xiao-ra4m1-arp) (RA4M1 with onboard DAC) to the XIAO RP2350, which has no onboard DAC. A MCP4725 I2C DAC replaces the RA4M1's internal DAC. The encoder + OLED UI from the RA4M1 design pivot is the baseline here from day one.

### 1.1 Design Goals

- Scale-quantised generative melody via MCP4725 I2C DAC → MCP6002 op-amp → Eurorack V/Oct
- Encoder + OLED UI: tempo pot for the live knob, encoder + 64×32 display for everything else
- 2 HP form factor (Hagiwo MOD2 template)
- 2 CV inputs, 1 clock/gate input, 1 V/Oct output, 1 gate output
- Host-side TDD for all pure-logic modules; bench verification for HAL
- Fully open-source hardware (KiCad) and firmware (Arduino C++)

### 1.2 Relationship to xiao-ra4m1-arp

| Element | RA4M1 version | RP2350 version (this project) |
|---|---|---|
| MCU | Seeed XIAO RA4M1 (Cortex-M4, 12-bit DAC onboard) | Seeed XIAO RP2350 (dual Cortex-M33, no DAC) |
| DAC | RA4M1 internal 12-bit DAC on D0/A0 | MCP4725 external I2C 12-bit DAC |
| UI | 3 pots → pivoted to 1 pot + encoder + OLED | 1 pot + encoder + OLED (pivot is baseline) |
| Display | 128×32 OLED (considered) | 64×32 OLED, 0.49", vertical mount |
| Logic libs | `scales`, `arp`, `tempo` — pure C++, no Arduino deps | Same libs, reused verbatim |
| Op-amp scaling | MCP6002 non-inverting, gain 1.27× | Same circuit |

---

## 2. Hardware Specification

### 2.1 Module Format

| Parameter | Value |
|---|---|
| Format | Eurorack 3U |
| Width | 2 HP (10.16 mm) |
| Depth | ~35 mm (including power header) |
| Power connector | 2×5 shrouded Eurorack header |
| Power rails used | +12V, GND (→ AMS1117-5.0 → 5V → onboard 3.3V reg) |
| Power draw (est.) | +12V: ~60 mA · -12V: ~5 mA (op-amp only) |

### 2.2 MCU — Seeed XIAO RP2350

| Attribute | Value |
|---|---|
| MCU | RP2350A, dual ARM Cortex-M33 @ 150 MHz |
| Flash | 4 MB (QSPI) |
| SRAM | 520 KB |
| ADC | 4× 12-bit (A0–A3) |
| DAC | None — external MCP4725 via I2C |
| GPIO | 26 total |
| Interfaces | I2C, SPI, UART, USB 1.1, PIO |
| Form factor | XIAO standard (21 × 17.8 mm), SMD stamp holes |
| Arduino support | Via earlephilhower arduino-pico platform |
| Price (approx.) | ~$6 USD |

### 2.3 External DAC — MCP4725

The RP2350 has no onboard DAC. The MCP4725 is a 12-bit I2C DAC that shares the I2C bus with the SSD1306 OLED.

| Attribute | Value |
|---|---|
| Part | MCP4725A0T-E/CH (SOT-23-6) or MCP4725 breakout |
| Resolution | 12-bit (4096 steps) |
| Interface | I2C, address 0x60 (A0 low) |
| Output range | 0–VDD (powered from 3.3V → 0–3.3V) |
| Output impedance | ~1 Ω (internal) |
| I2C speed | Up to 400 kHz (fast mode) |
| EEPROM | Onboard — power-on output value stored |
| Library | Adafruit MCP4725 |

### 2.4 Pin Assignment

| XIAO Pin | RP2350 GPIO | Direction | Function | Notes |
|---|---|---|---|---|
| D0 / A0 | GP26 | In (analog) | Tempo pot | 12-bit ADC; wiper 0–3.3V |
| D1 / A1 | GP27 | In (analog) | CV In #1 (J2) | 12-bit ADC; 0–5V → 0–3.3V via divider |
| D2 / A2 | GP28 | In (analog) | CV In #2 (future) | 12-bit ADC; spare modulation input |
| D3 / A3 | GP29 | In (analog) | Spare analog | 4th ADC — reserved |
| D4 | GP6 | I2C SDA | I2C bus (DAC + OLED) | MCP4725 addr 0x60, SSD1306 addr 0x3C |
| D5 | GP7 | I2C SCL | I2C bus (DAC + OLED) | 400 kHz fast mode |
| D6 | GP0 | Out | Gate Out (J4) | 3.3V logic → 5V via NPN transistor |
| D7 | GP1 | In (digital) | Clock / Gate In (J1) | Protected digital input |
| D8 | GP2 | In (digital) | Encoder A (phase A) | INPUT_PULLUP; interrupt-driven |
| D9 | GP3 | In (digital) | Encoder B (phase B) | INPUT_PULLUP; interrupt-driven |
| D10 | GP4 | In (digital) | Encoder click | INPUT_PULLUP |
| Internal | — | Out | Onboard NeoPixel | `WS2812_DATA_PIN` / `PIN_NEOPIXEL` |
| Internal | — | Out | `LED_BUILTIN` | Onboard mono LED, beat indicator |

**ADC-capable pins:** A0–A3 (four total). One consumed by tempo pot; A1 consumed by CV in #1; A2 and A3 are spare.

**I2C:** hardware I2C0 on D4/D5. Both MCP4725 and SSD1306 share this bus — different addresses.

### 2.5 Input Circuits

#### 2.5.1 Gate / Clock Input (J1) — Digital

Eurorack gate: 0–5V or 0–10V. RP2350 GPIO max 3.3V. Protection:
- 3.3 kΩ series resistor
- Schottky diode clamp to 3.3V rail (BAT54)
- Schottky diode clamp to GND
- Firmware threshold: >1.5V = HIGH

#### 2.5.2 CV Input (J2) — Analog

0–5V Eurorack input scaled to 0–3.3V via 100 kΩ / 220 kΩ divider (ratio ≈ 0.314 — wait, 100k/(100k+220k) ≈ 0.3125, maps 5V → 1.5V — actually this needs recalculating).

> **Note:** Use 150 kΩ / 100 kΩ divider: ratio = 100/(150+100) = 0.4, maps 5V → 2.0V. Or use 68 kΩ / 100 kΩ: ratio = 100/(68+100) ≈ 0.595, maps 5V → 2.97V (close to 3.3V ceiling — needs verification on bench). **Exact divider values to be confirmed during Story 004 breadboard work.** Protection diodes as per J1.

### 2.6 V/Oct Output Stage

MCP4725 outputs 0–3.3V (12-bit). Op-amp scales to Eurorack V/Oct.

#### 2.6.1 Target Output Range

**C3 (MIDI 48, 0V) to C7 (MIDI 96, 4V)** — four octaves, 0–4V. Same as RA4M1 design.

#### 2.6.2 Op-Amp Scaling Circuit

Op-amp: **MCP6002** (single-supply, rail-to-rail, 5V powered). Non-inverting amplifier.

| Component | Value | Function |
|---|---|---|
| U2 | MCP6002-I/P DIP-8 | Dual op-amp, single-supply, rail-to-rail |
| R1 | 10 kΩ | Input resistor |
| R2 | 2.7 kΩ | Gain resistor — sets gain ≈ 1.27× to map 0–3.3V → 0–4.2V |
| R4 | 100 Ω | Output series resistor (short-circuit protection, minimal loading error) |
| C1 | 100 nF | Op-amp supply decoupling |
| C2 | 10 nF | Op-amp output stability / HF rolloff |

Gain formula: `Vout = Vdac × (1 + R2/R1)`. With R1=10k, R2=2.7k: nominal gain 1.27×.

Firmware DAC formula (same as RA4M1 version):
```cpp
float targetVolts = (midiNote - MIDI_ROOT) / 12.0f;
float dacVolts    = targetVolts / GAIN;
uint16_t dacCount = (uint16_t)((dacVolts / DAC_VREF) * DAC_MAX);
```

Resolution: 3.3V × 1.27 / 4096 ≈ 1.02 mV/step at V/Oct output. One semitone = 83.3 mV ≈ 82 steps — same as RA4M1.

> **Note:** The MCP4725 has ~6–7 µs settling time — negligible at Eurorack audio rates. No glitching expected.

### 2.7 Gate Output Stage (J4)

Same NPN level-shift as RA4M1 version:
- Base: D6 via 1 kΩ resistor
- Collector: +5V rail via 10 kΩ pull-up
- Emitter: GND
- Gate high ≈ 4.9V. Rise/fall <1 µs.

### 2.8 OLED Display — 64×32 SSD1306, 0.49"

| Item | Value |
|---|---|
| Display | 64×32 monochrome OLED, 0.49" diagonal |
| Controller | SSD1306 (I2C address 0x3C) |
| Interface | I2C, shared bus with MCP4725 |
| Mounting | Vertical in panel — 32px wide × 64px tall from user's perspective (display rotated 90°) |
| Library | Adafruit SSD1306 + Adafruit GFX |
| Role | Shows active parameter name + value; beat pulse indicator |

> **Note:** The SSD1306 at 64×32 initialises with `display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)` and dimensions `(64, 32)`. Rotated 90° in software to use as 32-wide × 64-tall portrait strip in the panel window.

### 2.9 Rotary Encoder

| Item | Value |
|---|---|
| Part | PEC11R series (12mm body, detented) or equivalent |
| Pins | A → D8, B → D9, click → D10 |
| Library | `Encoder.h` (Paul Stoffregen) or `RotaryEncoder.h` |
| Pullups | INPUT_PULLUP on all three pins |
| Debounce | Software (50 ms minimum click gap) |

### 2.10 Tempo Pot

| Item | Value |
|---|---|
| Ref | RV1 |
| Value | 100 kΩ linear |
| Pin | D0/A0 |
| Range | 40–300 BPM, exponential curve (each 1/3 rotation doubles BPM) |

### 2.11 Onboard NeoPixel (RGB Feedback)

The XIAO RP2350 has an onboard WS2812B NeoPixel on a dedicated internal pin (`PIN_NEOPIXEL`). No external LED required.

| Parameter | Value |
|---|---|
| Access | `PIN_NEOPIXEL` (arduino-pico core) |
| Library | Adafruit NeoPixel |
| Role | Colour encodes active encoder menu parameter (green=scale, blue=order, red=chaos, …) |

### 2.12 Power Supply

Same as RA4M1 version: AMS1117-5.0 from +12V, Eurorack 2×5 header, reverse-polarity Schottky on +12V.

---

## 3. Panel & PCB Layout (2 HP)

### 3.1 Component Layout (Top to Bottom)

```
┌──────┐
│ OLED │  64×32 display, rotated 90°, portrait strip
│      │  (32px wide × 64px tall active area in panel)
├──────┤
│ POT  │  Tempo (9mm bushing)
├──────┤
│ ENC  │  Rotary encoder (13mm body, click)
├──────┤
│[J][J]│  J1: Clock In  |  J3: V/Oct Out
│[J][J]│  J2: CV In #1  |  J4: Gate Out
└──────┘
```

2 HP = 10.16 mm panel width. Jack bushings (PJ301M-12: 6mm) fit two per row. Pot bushing 9mm extends behind panel.

---

## 4. Firmware Architecture

### 4.1 Overview

Arduino C++ targeting the XIAO RP2350 via the earlephilhower arduino-pico platform. Cooperative loop; encoder handled by interrupt or polling in a tight sub-loop.

### 4.2 Module Structure

| File | Responsibility |
|---|---|
| `src/main.cpp` | Setup, main loop, encoder read, button handling, clock tick |
| `lib/scales/scales.h/.cpp` | Scale definitions, `quantize()` — reused from RA4M1 project |
| `lib/arp/arp.h/.cpp` | Arp state machine, order modes — reused from RA4M1 project |
| `lib/tempo/tempo.h/.cpp` | BPM mapping — reused from RA4M1 project |
| `lib/dac_out/dac_out.h/.cpp` | MCP4725 write wrapper, `noteToDAC()`, calibrated GAIN |
| `lib/gate_out/gate_out.h/.cpp` | Gate output timing, duty cycle |
| `lib/oled_ui/oled_ui.h/.cpp` | SSD1306 wrapper, parameter display, beat pulse |
| `lib/encoder_input/encoder_input.h/.cpp` | Encoder delta + click, debounce |
| `lib/clock_in/clock_in.h/.cpp` | External clock interrupt, BPM estimation |

### 4.3 Generative Engine

Same as RA4M1 version. Parameters accessible via encoder menu:

| Parameter | Control | Behaviour |
|---|---|---|
| Scale | Encoder menu | 6 scales, quantises all output |
| Arp order | Encoder menu | Up/Down/UpDown/DownUp/Skip/Random |
| Chord root | Encoder menu | MIDI 36–72 (C2–C6) |
| Chaos | Encoder menu (future) | Pitch walk probability |
| Tempo | Tempo pot (always live) | 40–300 BPM, exponential |

### 4.4 Encoder UX

- **Rotate:** change current parameter's value
- **Short press:** cycle to next parameter (wraps)
- **Long press (>500 ms):** context action (reset sequence, future: settings)
- **OLED:** always shows active parameter name + current value
- **NeoPixel:** colour = which parameter is active

### 4.5 Clock & Timing

Same as RA4M1: internal clock from pot, external clock rising edge on J1. Each step = 16th note at BPM.

---

## 5. Calibration Procedure

1. Connect multimeter to J3 (V/Oct out).
2. Power on. Hold encoder button 3s to enter calibration mode (OLED shows "CAL").
3. Module outputs MIDI 48 (C3 = 0.000V target). Adjust firmware `GAIN` or hardware trim until multimeter reads 0.000V ±2 mV.
4. Module then outputs MIDI 84 (C7 = 4.000V target). Verify.
5. Short-press encoder to exit and save to RP2350 flash (LittleFS or EEPROM emulation).

---

## 6. Bill of Materials (Breadboard / Rev 0.1)

| Ref | Component | Value / Part | Qty |
|---|---|---|---|
| U_MCU | MCU board | Seeed XIAO RP2350 | 1 |
| U_DAC | I2C DAC | MCP4725 breakout or SOT-23-6 | 1 |
| U_AMP | Op-amp | MCP6002-I/P DIP-8 | 1 |
| U_REG | LDO | AMS1117-5.0 SOT-223 | 1 |
| U_DISP | OLED | 64×32 SSD1306, 0.49", I2C | 1 |
| D_PROT | Protection diode | 1N5819 SOD-123 | 1 |
| D_CLAMP | Clamp diodes | BAT54 SOD-323 | 4 |
| Q1 | NPN transistor | BC548C or 2N3904 TO-92 | 1 |
| J1–J4 | 3.5mm jacks | PJ301M-12 | 4 |
| J_PWR | Power header | 2×5 2.54mm shrouded | 1 |
| RV1 | Tempo pot | B100K Alpha RD901F 9mm | 1 |
| ENC1 | Rotary encoder | PEC11R (detented, with click) | 1 |
| R1 | Resistor | 10 kΩ 0402 | 1 |
| R2 | Resistor | 2.7 kΩ 0402 1% | 1 |
| R4 | Resistor | 100 Ω 0402 | 1 |
| R5 | Resistor | 1 kΩ 0402 (transistor base) | 1 |
| R6 | Resistor | 10 kΩ 0402 (gate pull-up) | 1 |
| R7, R8 | Resistors | 3.3 kΩ 0402 (jack protection) | 2 |
| R_CV | Resistors | CV divider (values TBD on bench) | 2 |
| C1 | Cap | 100 nF 0402 (op-amp supply) | 1 |
| C2 | Cap | 10 nF 0402 (op-amp output) | 1 |
| C3 | Cap | 100 µF electrolytic (AMS1117 in) | 1 |
| C4 | Cap | 10 µF electrolytic (AMS1117 out) | 1 |
| C5, C6 | Caps | 100 nF 0402 (rail decoupling) | 2 |
| — | XIAO socket | 7-pin 2.54mm female header | 2 |
| — | Panel | 2HP PCB panel | 1 |

---

## 7. Open Questions & Next Steps

- [ ] Confirm exact CV divider resistor values on bench (Story 004)
- [ ] Confirm MCP4725 I2C address and library init on RP2350 Arduino core
- [ ] Confirm SSD1306 64×32 library init and rotation
- [ ] Confirm PEC11 encoder library choice (interrupt vs polling)
- [ ] Confirm software serial EEPROM / LittleFS for calibration storage
- [ ] KiCad schematic and PCB (deferred until breadboard validates)
- [ ] Investigate `BOOTSEL` button as secondary input (RP2350 exposes it via SDK)

---

## 8. References

- [xiao-ra4m1-arp](https://github.com/funkfinger/xiao-ra4m1-arp) — predecessor project
- [Seeed XIAO RP2350 wiki](https://wiki.seeedstudio.com/XIAO_RP2350/)
- [earlephilhower/arduino-pico](https://github.com/earlephilhower/arduino-pico) — Arduino core for RP2350
- [MCP4725 datasheet](https://www.microchip.com/en-us/product/MCP4725) — Microchip
- [SSD1306 datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
- [Hagiwo MOD2](https://note.com/solder_state/n/nce8f7defcf98) — physical template for 2HP layout
