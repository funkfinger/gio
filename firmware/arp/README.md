# firmware/arp

PlatformIO project for the gio arpeggiator firmware targeting the Seeed XIAO RP2350.

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (CLI) or PlatformIO IDE extension
- USB-C cable

## Build

```bash
pio run -d firmware/arp
```

## Upload

```bash
pio run -d firmware/arp --target upload
```

That's it — **no button-press needed for normal re-flashes.** PlatformIO uses `picotool` as the default upload protocol for this board, and the running firmware exposes a USB endpoint that lets `picotool` send a "reboot to bootloader" request. The board soft-resets, picotool flashes, then it reboots into the new firmware.

You only need manual BOOTSEL when:

- **First-flash of a fresh XIAO** (no firmware to soft-reset from yet).
- **Firmware is hung / crashed before USB came up** (rare — you'd notice the board not appearing in `ls /dev/tty.usbmodem*`).
- You ever flash bare metal or strip out the picotool USB endpoint.

### Manual BOOTSEL (when needed)

The "double-tap BOOT" trick advertised by Seeed is **unreliable on the XIAO RP2350 units we've used** — bench-confirmed across many flashes during Stories 002–010. The method that always works:

1. Unplug USB.
2. Hold the BOOT button down.
3. Plug USB in (still holding BOOT).
4. Release BOOT — `RP2350` mounts as a drive.

Then run the upload command as above (in this state, picotool detects BOOTSEL directly without needing the soft-reset path).

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

- **Adafruit SSD1306 + GFX** — OLED driver. Three-constant config block in `lib/oled_ui/oled_ui.h` (`OLED_WIDTH`, `OLED_HEIGHT`, `OLED_ROTATION`) handles the swap between bench (0.91" 128×32 landscape) and final design (0.49" 64×32 portrait).
- **mathertel/RotaryEncoder** — polling-based quadrature encoder. RP2350-compatible. `paulstoffregen/Encoder` is incompatible (AVR-only register macros). LatchMode = `FOUR3` for our PEC11 build (decisions §17).
- **Adafruit NeoPixel** — onboard RGB LED on **GPIO22** (data) + **GPIO23** (power-enable, hold HIGH). LED is **RGBW** — initialise with `NEO_GRBW + NEO_KHZ800`. Neither pin is exposed by the arduino-pico variant header; both are hardcoded in `src/main.cpp`.
