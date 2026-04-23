# Story 012: SPI Bring-up — DAC8552 + MCP3208 on Shared Bus

**As a** bench engineer
**I want** the XIAO RP2350 to talk to both the DAC8552 (16-bit dual SPI DAC) and the MCP3208 (8-channel SPI ADC) on a shared SPI bus
**So that** every subsequent story has a working write-output / read-input primitive to build on

## Pin map (confirmed pre-bench)

Verified from the arduino-pico variant header for `seeed_xiao_rp2350`:

| Function | XIAO pin | RP2350 GPIO | Notes |
|---|---|---|---|
| SPI SCK | D8 | GP2 | `PIN_SPI0_SCK` |
| SPI MISO | D9 | GP4 | `PIN_SPI0_MISO` |
| SPI MOSI | D10 | GP3 | `PIN_SPI0_MOSI` |
| CS — DAC8552 | D3 | GP5 | `PIN_SPI0_SS` (default Arduino SS pin) |
| CS — MCP3208 | D6 | GP0 | Any GPIO works for CS |

All four SPI signal pins are on SPI0's hardware default set, so `SPI.begin()` works without `SPI.setSCK/setTX/setRX`.

**Encoder pin migration (consequence of taking D8/D9/D10 for SPI):**

| Function | v0.3.0 pin | Rev 0.1 pin |
|---|---|---|
| Encoder A | D8 (GP2) | A1 (GP27) |
| Encoder B | D9 (GP3) | A2 (GP28) |
| Encoder click | D10 (GP4) | D7 (GP1) |

mathertel/RotaryEncoder is polling-based, so this is a pure wiring change — only pin `#define`s in `main.cpp` change.

## Library decision (pre-bench)

Use existing well-tested Arduino libraries instead of rolling our own SPI frame packers:

- **`robtillaart/DAC8552`** (MIT, v0.5.2) — bench-validated by user in `~/Git/Electronics/eurorack/dac8852-test/`. API: `DAC8552 dac(CS_PIN); dac.begin(); dac.setValue(channel, uint16_value);`
- **`robtillaart/MCP_ADC`** (MIT) — covers MCP3001/3002/3004/3008/3201/3202/3204/**3208**. API: `MCP3208 adc; adc.begin(CS_PIN); int v = adc.analogRead(channel);`

This means our `lib/spi_bus/` doesn't exist; instead `lib/outputs/` and `lib/inputs/` wrap these libraries with the calibration math and HAL interface.

## Acceptance Criteria

### Hardware

- [ ] DAC8552 on hand-soldered protoboard (already done — confirmed 2026-04-23).
- [ ] MCP3208 in DIP-16 directly on the breadboard.
- [ ] Shared SPI0 bus wired per pin map above:
  - SCK (D8) → both ICs' clock pins
  - MOSI (D10) → MCP3208 DIN, DAC8552 DIN
  - MISO (D9) → MCP3208 DOUT (DAC8552 has no MISO)
  - CS_DAC (D3) → DAC8552 /SYNC
  - CS_ADC (D6) → MCP3208 /CS
- [ ] CS lines pulled high by 10 kΩ to +3.3V at boot (so ICs idle deselected).
- [ ] **VREF strategy v1:** both ICs use +5V as VREF (tied to VDD). Resolution: DAC = 5V / 65536 = **76 µV/step**; ADC = 5V / 4096 = **1.22 mV/step**.
- [ ] Decoupling: 100 nF on each IC's VDD; 100 nF on each IC's VREF.

### Firmware

- [ ] Add libraries to `firmware/arp/platformio.ini`:
  ```
  lib_deps =
      robtillaart/DAC8552
      robtillaart/MCP_ADC
  ```
- [ ] New `firmware/arp/lib/outputs/` HAL wrapper:
  - `outputs::write(channel, volts)` — apply per-channel gain/offset, call `dac.setValue()`
  - `outputs::gate(channel, on)` — convenience wrapper around `write(channel, on ? 5.0f : 0.0f)`
- [ ] New `firmware/arp/lib/inputs/` HAL wrapper:
  - `inputs::readVolts(channel)` — call `adc.analogRead()`, apply per-channel gain/offset, return jack-side volts
  - `inputs::readRaw(channel)` — passthrough for diagnostics
- [ ] Smoke test in a temporary `main.cpp`: write a triangle on DAC ch A (0 → 65535 → 0 over 1 s); read MCP3208 ch 0 (jumpered directly to DAC OUTA); print both to serial; confirm ADC tracks DAC roughly linearly.

### Bench

- [ ] Loopback test: jumper DAC OUTA → MCP3208 ch 0; OUTB → ch 1. Sweep each DAC channel; verify each ADC channel tracks.
- [ ] Verify all 8 MCP3208 channels read independently — short each in turn to GND, confirm only that channel reads ~0.
- [ ] Scope SCK during a transaction; verify clock rate. Start at 1 MHz, push to 4 MHz, confirm correctness.
- [ ] Confirm both CS lines toggle independently (scope both during back-to-back transactions).

## Notes

- **DAC8552 datasheet quirk:** 24-bit serial frame = 8 control + 16 data. Rob Tillaart's library handles all of this — `setValue(channel, value)` is the whole API surface needed.
- **MCP3208 quirk:** start bit + single/diff + 3 channel bits + null + 12 data. Rob Tillaart's `MCP_ADC` library handles it.
- **Why both libraries from the same author:** consistency in API style, both MIT-licensed, both actively maintained, both already platform-resolved.
- **Why pre-pull `outputs` / `inputs` HAL into Story 012:** keeps the smoke test from needing to know about the underlying libraries; later stories (013, 015) just deepen the calibration math behind the same interface.

## Depends on

- Story 011 (Protomato rails verified)

## Status

prep complete (pin map confirmed, library choice locked, encoder migration documented). Bench session pending parts arrival (MCP3208 ordered, OLED en route).
