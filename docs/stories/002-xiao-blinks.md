# Story 002: XIAO Blinks

**As a** developer
**I want** the PlatformIO toolchain to build and flash a minimal firmware to the XIAO RP2350
**So that** I know the end-to-end toolchain (install, board def, compile, upload, run) works before I write anything non-trivial

## Acceptance Criteria

- [ ] `firmware/arp/platformio.ini` references the earlephilhower arduino-pico platform and `board = seeed_xiao_rp2350`
- [ ] `firmware/arp/src/main.cpp` toggles the onboard LED (`LED_BUILTIN`) at ~1 Hz using Arduino `setup()` / `loop()`
- [ ] `pio run -d firmware/arp` compiles cleanly (zero errors, zero warnings from project code)
- [ ] `pio run -d firmware/arp --target upload` flashes the board via UF2 (double-tap BOOT button to enter bootloader) and the LED visibly blinks *(bench verify; record flash size)*
- [ ] CI runs `pio run -d firmware/arp` (compile-check only) and stays green
- [ ] `firmware/arp/README.md` documents build, upload (UF2 method), and any platform quirks

## Notes

- XIAO RP2350 UF2 upload: double-tap the BOOT/RST button to enter bootloader; a `RPI-RP2` drive appears; PlatformIO copies the `.uf2` file. Or use `picotool` for a more reliable CLI experience.
- `LED_BUILTIN` on the XIAO RP2350 is the onboard mono LED (active LOW per the RA4M1 pattern — confirm with the arduino-pico core for RP2350).
- The onboard NeoPixel is on `PIN_NEOPIXEL`; do not attempt to drive it here — that's a later story.
- If the earlephilhower platform needs a specific PlatformIO version or manual install, document it in `firmware/arp/README.md`.

## Depends on

- Story 001

## Status

done — flashed via UF2 (picotool), LED blinks at 1 Hz on bench. Flash: 56 KB / 2 MB.
