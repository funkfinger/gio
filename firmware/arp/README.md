# firmware/arp

PlatformIO project for the gio arpeggiator firmware targeting the Seeed XIAO RP2350.

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (CLI) or PlatformIO IDE extension
- USB-C cable

## Build

```bash
pio run -d firmware/arp
```

## Upload (UF2 method)

1. Double-tap the BOOT/RST button on the XIAO RP2350.
2. A drive named `RPI-RP2` appears on your computer.
3. Run:

```bash
pio run -d firmware/arp --target upload
```

PlatformIO copies the `.uf2` file to the drive automatically. The board resets and runs the new firmware.

**Tip:** if the drive doesn't appear, hold BOOT while plugging in USB, then release.

## Host-side unit tests

Pure-logic modules (scales, arp, tempo) are tested on the host — no board required:

```bash
pio test -d firmware/arp -e native
```

GoogleTest requires an explicit `main()` in each test file (PlatformIO 6.x does not auto-link `gtest_main`).

## Serial monitor

```bash
pio device monitor -d firmware/arp
```

Baud rate: 115200.

## Library notes

- **Adafruit SSD1306 + GFX** — OLED driver. 64×32 init, rotate 90° for portrait.
- **Encoder (Stoffregen)** — interrupt-driven quadrature encoder. Reliable on RP2350 PIO.
- **Adafruit NeoPixel** — onboard RGB LED at `PIN_NEOPIXEL`.
