# gio — XIAO RP2350 Generative Arpeggiator

A 2 HP Eurorack generative arpeggiator module built around the Seeed XIAO RP2350.

## Project layout

```
gio/
├── firmware/arp/     PlatformIO project (Arduino framework, RP2350)
├── hardware/         KiCad project — populated after breadboard validation
├── docs/             Spec, decisions, user stories
└── CHANGELOG.md      Keep a Changelog format; updated on every commit
```

## Build and test

```bash
# Compile firmware
pio run -d firmware/arp

# Host-side unit tests (pure logic only — no Arduino deps)
pio test -d firmware/arp -e native

# Upload to board (double-tap BOOT to enter UF2 bootloader)
pio run -d firmware/arp --target upload
```

## Hardware

- **MCU:** Seeed XIAO RP2350 (RP2350A, dual Cortex-M33, 150 MHz)
- **DAC:** MCP4725 (I2C, 12-bit) — external, on I2C bus with OLED
- **Display:** 64×32 SSD1306 OLED, 0.49", I2C, mounted vertically
- **Controls:** 1× tempo pot, 1× PEC11 rotary encoder (with click)
- **Jacks:** J1 clock/gate in, J2 CV in, J3 V/Oct out, J4 gate out

## Key decisions

- **No onboard DAC** — RP2350 has no DAC; MCP4725 on I2C fills that role
- **I2C bus** on D4 (SDA) / D5 (SCL): shared by MCP4725 + SSD1306 OLED
- **Host TDD** for pure-logic modules (scales, arp, tempo) via `pio test -e native`
- **Bench verification** for HAL (DAC, gate, OLED, encoder)
- **CHANGELOG.md** updated on every commit (Keep a Changelog format)

See `docs/decisions.md` for the full rationale and `docs/stories/` for the backlog.
