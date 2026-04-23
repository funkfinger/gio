# gio — Eurorack CV/Gate Platform Module

> **Rev 0.1 — April 2026** · 2 HP · XIAO RP2350 · DAC8552 + MCP3208 SPI · TL072 buffers · ±12V Eurorack
>
> **First-class app: generative arpeggiator.** Module is platform-architected; the arp is one of several apps that share the hardware.

---

## 1. Project Overview

gio is a 2 HP Eurorack platform module. The hardware is general-purpose CV/gate I/O (2 inputs, 2 outputs, all dual-purpose CV-or-trigger), driven by a Seeed XIAO RP2350. Firmware is structured as an **app loader** with multiple apps selectable at runtime via the encoder; the first-class app is a generative arpeggiator with scale quantisation, multiple arp orders, encoder-driven menu, V/oct CV-in transpose, and external-clock sync.

Earlier in the project, gio was scoped as a single-purpose arp using the RP2350's native ADCs and a PWM-DAC chain. After bench-validating that architecture (Stories 001–010, tagged `arp/v0.3.0`), the design pivoted to the platform-module approach to make the same hardware re-usable across apps. The PWM-DAC is replaced by a TI **DAC8552** (16-bit dual SPI DAC); the native ADCs are replaced by a **MCP3208** (8-channel SPI ADC); a single MCP6002 single-supply op-amp is replaced by **TL072** dual JFET op-amps running directly off Eurorack ±12V. All four jacks become symmetric: every input can be CV or trigger, every output can be CV or gate. See `docs/decisions.md` §19–§23 for the rationale on each leg of the pivot.

### 1.1 Design Goals

- **Platform first, apps second.** Hardware exposes a simple, symmetric I/O surface; firmware hosts swappable apps.
- **Mutable Instruments–style I/O protection** — series resistors + Schottky clamps to ±12V rails + op-amp buffering on every signal path.
- **2 HP form factor** (Hagiwo MOD2 panel template).
- **4 jacks, all dual-purpose:** 2 inputs (CV or trigger), 2 outputs (CV or gate).
- **Eurorack-native voltage handling:** ±10V swing on inputs and outputs.
- **Open hardware (KiCad)** and **open firmware (Arduino C++ / earlephilhower arduino-pico)**.
- **Host-side TDD** for pure-logic modules; **bench verification** for the analog signal chain.

### 1.2 Apps shipping with Rev 0.1 firmware

| App | Behaviour | Notes |
|---|---|---|
| **arp** | Generative arpeggiator (port of `arp/v0.3.0` to platform HAL) | Default-loaded |
| **clock_out** | Adjustable square-wave clock source on OUT1 | Bench-tooling app, also useful in patches |

Future-app candidates (no commitment): LFO, S&H, slew limiter, quantizer, envelope generator, V/oct calibrator, Turing-machine sequencer.

---

## 2. Hardware Specification

### 2.1 Module Format

| Parameter | Value |
|---|---|
| Format | Eurorack 3U |
| Width | **2 HP** (10.16 mm) |
| Depth | ~35 mm including power header |
| Power connector | 2×5 shrouded Eurorack header (Doepfer pinout, red stripe = −12V) |
| Power rails used | +12V, −12V, GND. **+5V is generated locally** from +12V via LM7805 (no draw on Eurorack +5V bus) |
| Power draw (est.) | +12V: ~80 mA · −12V: ~10 mA · +5V (local): n/a |

### 2.2 MCU — Seeed XIAO RP2350

| Attribute | Value |
|---|---|
| MCU | RP2350A, dual ARM Cortex-M33 @ 150 MHz |
| Flash | 4 MB (QSPI) |
| SRAM | 520 KB |
| ADC | 4× 12-bit native (3 exposed: GP26/27/28). **Not used for signal I/O in Rev 0.1** — input ADC is via MCP3208. The native ADC stays available for the tempo pot. |
| DAC | None onboard. Output via MCP3208's sister DAC8552 over SPI. |
| GPIO | 11 broken-out pins (D0–D10) |
| Interfaces | I2C, SPI (multiple sets), UART, USB 1.1, PIO |
| Form factor | XIAO standard (21 × 17.8 mm), SMD stamp holes + castellations |

> **Pin-availability gotcha (carried over from `arp/v0.3.0`):** despite the RP2350 chip nominally having GP29, the XIAO RP2350 board variant **does not break out GP29.** Only GP0–7, GP16–17, GP20–21, and GP26–28 are pin-accessible from the board edges. Plus the internal NeoPixel pair (GP22 data + GP23 power-enable). **Never spec a GP29 connection on this board.**

### 2.3 Signal-Chain Architecture

**Inputs (2 jacks → MCP3208):**

```
J_IN ──[100k series]──┬── BAT43 to +12V ──┐
                      ├── BAT43 to −12V ──┘
                      │
                      └──[TL072 inverting summing amp, gain −1/4 + offset]──→ MCP3208 chN
                          (maps ±10V at jack → 0–5V at ADC)
```

**Outputs (2 jacks ← DAC8552):**

```
DAC8552 chN (0–5V) ──[TL072 inverting summing amp, gain −4 + offset]──→ [1k series]──┬── BAT43 to +12V ──┐── J_OUT
                                                                                       └── BAT43 to −12V ──┘
                          (maps 0–5V at DAC → ±10V at jack, inverted; firmware compensates)
```

**Tempo pot:** read by XIAO native ADC (GP26 / A0). Bypasses the MCP3208 to save bandwidth and one ADC channel; the pot is a known-low-impedance source so direct reading is fine.

**OLED:** I2C on dedicated bus (D4=SDA / D5=SCL). No bus sharing.

**SPI bus:** shared between DAC8552 and MCP3208. Each gets a dedicated CS line. Default SPI clock 4 MHz (push to 8+ MHz if bench reveals headroom).

### 2.4 Pin Assignment (confirmed against variant header)

Verified from `~/.platformio/packages/framework-arduinopico/variants/seeed_xiao_rp2350/pins_arduino.h`. SPI0 default pins are baked into the chip; we use them rather than reassigning at runtime.

| XIAO Pin | RP2350 GPIO | Direction | Function | Notes |
|---|---|---|---|---|
| D0 / A0 | GP26 | In (analog) | Tempo pot wiper | Native 12-bit ADC; low-impedance source, no buffer |
| D1 / A1 | GP27 | In (digital) | Encoder A | Polled — any GPIO works |
| D2 / A2 | GP28 | In (digital) | Encoder B | Polled |
| D3 | GP5 | Out | SPI CS — DAC8552 | This is `PIN_SPI0_SS` — can be used as default CS directly |
| D4 | GP6 | Out | I2C SDA (OLED) | Dedicated I2C bus; SSD1306 0x3C |
| D5 | GP7 | Out | I2C SCL (OLED) | 400 kHz fast mode |
| D6 | GP0 | Out | SPI CS — MCP3208 | Any GPIO works for CS |
| D7 | GP1 | In (digital) | Encoder click | Polled |
| D8 | GP2 | Out | SPI SCK | `PIN_SPI0_SCK` — hardware SPI0 default |
| D9 | GP4 | In | SPI MISO | `PIN_SPI0_MISO` — hardware SPI0 default |
| D10 | GP3 | Out | SPI MOSI | `PIN_SPI0_MOSI` — hardware SPI0 default |
| Internal | GP22 | Out | NeoPixel data | RGBW; via panel light pipe (see §3) |
| Internal | GP23 | Out | NeoPixel power-enable | Hold HIGH |

**Encoder pin migration from `arp/v0.3.0`:**

| Function | v0.3.0 pin | Rev 0.1 pin | Reason |
|---|---|---|---|
| Encoder A | D8 (GP2) | A1 (GP27) | D8 is now SPI SCK |
| Encoder B | D9 (GP3) | A2 (GP28) | D9 is now SPI MISO |
| Encoder click | D10 (GP4) | D7 (GP1) | D10 is now SPI MOSI |

`mathertel/RotaryEncoder` is polling-based, so the new pins are purely a wiring change — no firmware logic changes beyond the `#define` constants.

**Bus assignment summary:**
- **I2C0 (D4/D5):** OLED only — dedicated, no sharing
- **SPI0 (D8/D9/D10):** DAC8552 + MCP3208 — shared, separate CS lines
- **Native ADC:** tempo pot only (GP26)

All 11 broken-out XIAO pins (D0–D10) have a job. No spares; D11–D18 (inner pads, harder to access) reserved for future expansion if Rev 0.2 needs them.

**Alternate considered + rejected:** moving I2C off D4/D5 to free SPI elsewhere. Rejected because I2C0 has fewer flexible pin options on the broken-out XIAO pins than SPI0 does, and the encoder is the easiest thing to relocate (no peripheral block dependency).

### 2.5 Input Stage (J1, J2 — Identical)

Per-jack circuit:

```
Jack tip ──[R_s = 100k]──┬──[BAT43 → +12V]──┐ (clamps over-voltage)
                          ├──[BAT43 → −12V]──┘
                          │
                          ├──[R_in = 4.7k]───── − input of TL072 ─── [R_fb = 4.7k] ─── output to MCP3208
                          │                       + input ─── (offset network from +5V via R_off = 9.4k)
                          │
                       (TL072 unity-buffer-style with summer)
```

| Parameter | Value |
|---|---|
| First-stage R_s | 100 kΩ (limits clamp current to ~30 µA at ±15V abuse) |
| Clamp diodes | BAT43 ×2 to ±12V rails |
| Op-amp | TL072 (one channel per input, 2 inputs = 1 TL072) |
| Mapping | jack +10V → ADC 0V; 0V → 2.5V; −10V → 5V (inverted; firmware compensates) |
| Input impedance | ~80 kΩ (the 100 kΩ first stage) |
| ADC resolution at jack | 5V at ADC ÷ 4096 ÷ 0.25 ≈ **4.9 mV / count** (sufficient for V/oct quantisation: 17 counts/semitone) |

**Trigger detection** is firmware-only: a Schmitt-trigger state machine reads the ADC value and detects rising edges crossing the +1.5V threshold (with +0.5V hysteresis to reject noise). See Story 016.

### 2.6 Output Stage (J3, J4 — Identical)

Per-jack circuit:

```
DAC8552 chN ──[R_in = 10k]──┬── − input of TL072 ──[R_fb = 40k]── + output ──[R_o = 1k]──┬── BAT43 → +12V
                            │                                                              ├── BAT43 → −12V ──── Jack tip
   +5V ──[R_off = 10k]──────┘                                                              │
                                                                                          (clamps shorts to rails)
```

| Parameter | Value |
|---|---|
| DAC | DAC8552 16-bit dual-channel SPI; VREF tied to +5V |
| DAC resolution | 5V ÷ 65536 ≈ **76 µV / count** at DAC out |
| Op-amp | TL072 (one channel per output, 2 outputs = 1 TL072) |
| Output mapping | DAC 0V → +10V at jack; 2.5V → 0V; 5V → −10V (inverted; firmware compensates) |
| At-jack resolution | 20V ÷ 65536 ≈ **305 µV / count** (≈ 0.4 cents at V/oct — well below human pitch perception) |
| Output impedance | 1 kΩ (the series resistor; protects against shorts to rails) |
| Settling time | ~5 µs (DAC settle 4 µs + op-amp slew dominated by DAC) |
| Edge time as gate | < 50 µs for a 0V→+5V transition (Story 014 verifies) |

**Both outputs are dual-purpose:** apps write CV via `outputs.write(channel, volts)` or gates via `outputs.gate(channel, on)`. Same hardware, no mode switch.

### 2.7 OLED Display — 0.49" 64×32 SSD1306

| Item | Value |
|---|---|
| Display | 64×32 monochrome OLED, 0.49" diagonal |
| Controller | SSD1306 (I2C address 0x3C) |
| Interface | I2C, dedicated bus on D4/D5 |
| Mounting | Vertical in panel — 32 px wide × 64 px tall from user perspective (rotated 90° in software) |
| Library | Adafruit SSD1306 + Adafruit GFX |
| HAL wrapper | `firmware/arp/lib/oled_ui/` — three-constant config block (`OLED_WIDTH`, `OLED_HEIGHT`, `OLED_ROTATION`) for one-line bench/final swap |

> **Bench note:** during Stories 003–010, a 0.91" 128×32 SSD1306 was used because the 0.49" panel wasn't on hand. The HAL wrapper makes the swap a one-line config change. The 0.49" 64×32 is the **target part for Rev 0.1** — fits the 2 HP panel width.

### 2.8 Rotary Encoder

| Item | Value |
|---|---|
| Part | PEC11L-4120F-S0020 (in stock, Cab-2/Bin-32) — 20 PPR mechanical, with detents, push switch, 6 mm flatted shaft |
| Pin assignment | A → A1 (GP27), B → A2 (GP28), click → D3 (GP5). Moved off D8/D9/D10 to free SPI. |
| Library | mathertel/RotaryEncoder, polling-based, `LatchMode = FOUR3` |
| Click debounce | 50 ms (firmware) |
| Long-press threshold | 500 ms — opens the platform's app-switcher menu by default; apps can override |

### 2.9 Tempo Pot (Live "Knob")

| Item | Value |
|---|---|
| Ref | RV1 |
| Value | 100 kΩ linear taper, panel mount |
| Stock | Cab-2/Bin-7, qty 20+, B100K |
| Pin | D0 / A0 (native ADC, GP26) |
| Use | Apps interpret the pot freely; in the arp app, it's tempo (20–300 BPM exponential). In other apps, it might be rate, pitch, mix, etc. — purely software. |

### 2.10 Onboard NeoPixel (RGB Indicator) + Panel Light Pipe

XIAO RP2350 has an onboard WS2812B NeoPixel (RGBW variant). On the Rev 0.1 panel PCB, a **pinhole or light-pipe cutout** above the LED's location lets it shine through the panel — a built-in user-feedback indicator with zero added BOM cost.

| Parameter | Value |
|---|---|
| Pixel data pin | GP22 (XIAO internal) |
| Power-enable pin | GP23 (XIAO internal — must be HIGH for LED to light) |
| Library | Adafruit NeoPixel, init with `NEO_GRBW + NEO_KHZ800` |
| Role | Per-app indicator (in arp: green = Scale, blue = Order, magenta = Root). Platform-level: brief flash on app switch. |

### 2.11 Power Supply

Eurorack ±12V → reverse-polarity Schottky on +12V → LM7805 generates +5V locally. ±12V routed directly to TL072s. +5V powers DAC8552 (VDD + VREF), MCP3208 (VDD + VREF), and the XIAO 5V pin (which regulates internally to 3.3V). No 3.3V regulator on the PCB; no draw on the Eurorack +5V bus.

| Component | Value | Function |
|---|---|---|
| Eurorack header | 2×5 shrouded boxed | Doepfer pinout, red stripe = −12V |
| D_PROT | 1N5818 Schottky DO-41 | +12V reverse polarity protection |
| U_REG | LM7805 TO-220 | +12V → +5V |
| C_in | 10 µF electrolytic | Reg input |
| C_out | 10 µF electrolytic + 100 nF ceramic | Reg output |
| Per-IC decoupling | 100 nF ceramic | At every IC supply pin |

---

## 3. Panel & PCB Layout (2 HP)

2 HP = 10.16 mm panel width. PJ-3001F Thonkiconn jacks have a 6 mm panel-side bushing; pot is 9 mm; encoder is 7 mm; OLED active area is ~11 × 5.5 mm. Everything fits a single vertical column.

### 3.1 Component layout (top to bottom)

```
┌──────┐
│ OLED │  64×32 display, rotated 90°, portrait strip
├──────┤
│  •   │  NeoPixel light pipe (pinhole over XIAO's onboard LED)
├──────┤
│ POT  │  Tempo / param knob (9 mm bushing, B100K)
├──────┤
│ ENC  │  Rotary encoder + click (7 mm bushing, PEC11L)
├──────┤
│ J1   │  IN 1 — CV or trigger
├──────┤
│ J2   │  IN 2 — CV or trigger
├──────┤
│ J3   │  OUT 1 — CV or gate
├──────┤
│ J4   │  OUT 2 — CV or gate
└──────┘
```

### 3.2 Panel labels — generic, app-agnostic

Because gio is a platform module, panel silkscreen does **not** label jacks by app-specific function. Generic labels:

- `IN 1`, `IN 2` (or just `1` / `2` with a ▷ glyph indicating input)
- `OUT 1`, `OUT 2` (or just `1` / `2` with a ◁ glyph indicating output)
- `KNOB` for the pot
- (Encoder is unlabelled; user discovers by turning it)
- (OLED tells the user what each control/jack is doing in the current app.)

Distinguishing inputs from outputs visually: different jack colour (e.g. white for IN, black for OUT — Mutable convention) or position (inputs above outputs). Final colour scheme decided at panel-design time.

### 3.3 Mechanical stack

Two boards: **panel PCB + main PCB.** Defer a third (separate power) board unless layout reveals it's needed.

- **Panel PCB:** silkscreen graphics, jack/pot/encoder/OLED cutouts, light-pipe pinhole over the XIAO LED. No active electronics. Affixes to main PCB via the pot/encoder bushings + spacers.
- **Main PCB:** all jacks, controls, OLED, XIAO socket, op-amps, DAC, ADC, regulator, Eurorack power header, all passives.

**XIAO orientation:** mounted component-side toward the panel so the onboard LED lines up with the panel light pipe. USB-C connector faces into the rack; **side or rear notch** in the main PCB allows USB cable access without pulling the module. Bench-friendly for development; not user-facing in normal Eurorack use.

---

## 4. Firmware Architecture

### 4.1 Overview

Arduino C++ targeting the XIAO RP2350 via the earlephilhower arduino-pico platform, pinned to `framework-arduinopico@4.4.0` via `maxgerhardt/platform-raspberrypi`. PlatformIO build, host-side TDD via `pio test -e native`.

**Two-layer firmware:**

```
┌─────────────────────────────────────────────────┐
│  Apps:  arp · clock_out · (future apps)         │
├─────────────────────────────────────────────────┤
│  Platform:  app loader · settings persistence   │
├─────────────────────────────────────────────────┤
│  HAL:  dac · adc · oled · encoder · pot ·       │
│        neopixel · trigger_in (Schmitt)          │
├─────────────────────────────────────────────────┤
│  Drivers:  spi_bus · I2C · GPIO                 │
└─────────────────────────────────────────────────┘
```

### 4.2 Library structure

| File | Responsibility |
|---|---|
| `lib/spi_bus/` | SPI initialisation; DAC8552 driver (`dac.write(channel, value16)`); MCP3208 driver (`adc.read(channel) → uint16`) |
| `lib/outputs/` | `outputs.write(channel, volts)` and `outputs.gate(channel, on)`, with per-channel calibration |
| `lib/inputs/` | `inputs.readVolts(channel)` with per-channel calibration |
| `lib/trigger_in/` | `Schmitt::poll(volts)` — firmware Schmitt with 1.5V high / 0.5V low thresholds |
| `lib/oled_ui/` | SSD1306 wrapper, parameter display |
| `lib/encoder_input/` | mathertel/RotaryEncoder wrapper, debounce, long-press detection |
| `lib/scales/` | Scale definitions, `quantize()` — pure logic, ported from RA4M1 project |
| `lib/arp/` | Arp state machine, order modes — pure logic |
| `lib/tempo/` | BPM mapping — pure logic |
| `lib/cv/` | V/oct → semitone math — pure logic |
| `lib/hardware/` | Aggregator struct passed to apps; provides DAC/ADC/OLED/encoder/pot/neopixel handles |
| `lib/platform/` | App interface (`struct App`), app loader, app switcher UI, settings-to-flash |
| `apps/arp/` | Generative arpeggiator app |
| `apps/clock_out/` | Square-wave clock source app |
| `src/main.cpp` | Setup, app registry, top-level loop |

### 4.3 The App interface

```cpp
struct App {
  const char* name;                          // shown in app switcher menu
  void (*setup)(Hardware& hw);               // called once when app is loaded
  void (*loop)(Hardware& hw);                // called every main-loop iteration
  void (*onEncoderShortPress)(Hardware& hw); // optional; nullptr = ignored
  void (*onEncoderLongPress)(Hardware& hw);  // optional; nullptr = "open app switcher"
};
```

Apps are isolated: they receive the `Hardware` struct, they call HAL methods, they don't reach for global pins. App-internal state lives in static-storage variables in the app's translation unit.

### 4.4 App switcher UX

- Default behaviour for `Encoder long-press (≥500 ms)` is to open the platform's app switcher menu.
- App switcher: OLED shows app name + cursor; rotate to scroll; click to load; long-press to cancel.
- Loading an app → calls new app's `setup()` → enters its `loop()`.
- Apps can override long-press for app-specific actions (e.g. arp's "reset"). Apps that override long-press lose the default-switcher gesture; alternative gateways TBD per app.

### 4.5 Settings persistence

Last-loaded app index is persisted to RP2350 flash via arduino-pico's `EEPROM.h` (emulated EEPROM). On boot, the saved app loads automatically. App-private settings (e.g. arp's last scale/order) are out of scope for the platform skeleton; each app that needs persistence gets a follow-up story.

### 4.6 Calibration & timing

- **Per-channel calibration** for inputs and outputs: gain + offset constants, hardcoded for Rev 0.1 (move to flash post-MVP).
- **Main loop:** non-blocking `millis()` state machine. ADC polled every iteration (sub-ms latency). DAC writes happen at app-determined rates.
- **No interrupts** in Rev 0.1 — encoder polled, ADC polled, gates written from main loop.

---

## 5. Calibration Procedure

For each output channel:
1. Connect multimeter to the jack.
2. Boot into a calibration utility (future story; for Rev 0.1, run a hand-rolled sweep from `main.cpp`).
3. Write known DAC counts (1024 / 16384 / 32768 / 49152 / 65535) → record measured voltages.
4. Least-squares-fit `gain` and `offset`; write to firmware constants.

For each input channel:
1. Apply known voltages from a bench supply (−10, −5, 0, +5, +10 V).
2. Read `inputs.readVolts(channel)`.
3. Least-squares-fit; write constants.

Target: ±10 mV at jack across the full ±10V range.

---

## 6. Bill of Materials (Rev 0.1)

| Ref | Component | Value / Part | Qty | Stock |
|---|---|---|---|---|
| U1 | MCU board | Seeed XIAO RP2350 | 1 | (need to confirm) |
| U2 | DAC | TI DAC8552 16-bit dual SPI, VSSOP-8 | 1 | ✅ Cab-3/Bin-37, qty 4 |
| U3 | ADC | Microchip MCP3208 8-ch SPI, DIP-16 | 1 | 🛒 ordered |
| U4, U5 | Op-amp | TL072CP dual JFET, DIP-8 | 2 | ✅ Cab-3/Bin-32, qty 25 |
| U6 | LDO | LM7805 TO-220 | 1 | ✅ Cab-3/Bin-23 |
| U7 | OLED | 64×32 SSD1306, 0.49", I2C | 1 | 🛒 ordered |
| D1 | Reverse protect | 1N5818 DO-41 Schottky | 1 | ✅ Cab-2/Bin-10 |
| D2–D9 | Clamp diodes | BAT43 DO-35 Schottky (4 jacks × 2 diodes) | 8 | ✅ Cab-2/Bin-10, qty 50+ |
| J1–J4 | Jacks | PJ-3001F Thonkiconn 3.5mm mono | 4 | ✅ Cab-7/Bin-9, qty 30+ |
| J_PWR | Power | 2×5 shrouded Eurorack header | 1 | 🛒 ordered |
| RV1 | Pot | B100K linear, 9mm bushing, panel mount | 1 | ✅ Cab-2/Bin-7, qty 20+ |
| ENC1 | Encoder | PEC11L-4120F-S0020 (with click) | 1 | ✅ Cab-2/Bin-32, qty 5 |
| R_in_in | Input series R | 100 kΩ 0805 | 2 | ✅ SMT R kit |
| R_in_op | Input op-amp R | 4.7 kΩ + 9.4 kΩ network per input | 4 + 2 | ✅ SMT R kit |
| R_out_op | Output op-amp R | 10 kΩ + 40 kΩ (or 39 kΩ + 1 kΩ) network per output | 4 + 2 | ✅ SMT R kit |
| R_out_o | Output series R | 1 kΩ 0805 | 2 | ✅ Cab-? SMT 1 kΩ reel |
| C_dec | Decoupling | 100 nF ceramic 0805 | 10+ | ✅ kit |
| C_bulk | Bulk | 10 µF electrolytic | 4+ | ✅ kit |
| C_in_reg | Reg input cap | 10 µF + 100 nF | 1 + 1 | ✅ kit |
| — | XIAO socket | 7-pin 2.54mm female header | 2 | ✅ kit |
| — | Panel | 2 HP PCB panel | 1 | (KiCad output) |

---

## 7. Open Questions & Next Steps

- [ ] **Bench-confirm SPI pin mapping** in Story 012; update §2.4 if encoder reassignment needs to differ from the proposed plan.
- [ ] **Confirm DAC8552 VREF source** — start with +5V LM7805 output; revisit with a precision 4.096V reference (REF3040 etc) only if bench reveals temperature drift > ±1 mV at V/oct.
- [ ] **Output topology + values** — Story 013 may bench-tune the gain/offset network resistor values away from the textbook 10k/40k/+5V values.
- [ ] **OLED I2C pull-ups** — most SSD1306 modules have onboard pull-ups; verify on the 0.49" model when it arrives.
- [ ] **Eurorack +5V bus** — confirmed not used; document in PCB ERC.
- [ ] **NeoPixel light pipe** — pick a panel cutout style (clear plastic insert vs. clear-resin-fill vs. open pinhole) at panel-design time.
- [ ] **App-private settings persistence** — separate story after platform skeleton ships.
- [ ] **KiCad schematic + PCB** — gated on Stories 011–018 bench-completing.

---

## 8. References

- [xiao-ra4m1-arp](https://github.com/funkfinger/funkfinger/xiao-ra4m1-arp) — predecessor project (RA4M1, single-purpose arp)
- [Seeed XIAO RP2350 wiki](https://wiki.seeedstudio.com/XIAO_RP2350/)
- [earlephilhower/arduino-pico](https://github.com/earlephilhower/arduino-pico)
- [DAC8552 datasheet](https://www.ti.com/lit/ds/symlink/dac8552.pdf)
- [MCP3208 datasheet](https://www.microchip.com/en-us/product/MCP3208)
- [TL072 datasheet](https://www.ti.com/lit/ds/symlink/tl072.pdf)
- [SSD1306 datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
- [Mutable Instruments hardware reference](https://github.com/pichenettes/eurorack) — input/output protection patterns
- [Hagiwo MOD2](https://note.com/solder_state/n/nce8f7defcf98) — physical 2HP panel template
- [Hivemind Protomato](https://hivemindsynthesis.com/shop/p/protomato-16hp-eurorack-diy-circuit-creation-platform-kit) — bench breadboarding platform used during Stories 011+
