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

# Upload to board. Picotool soft-resets via USB into BOOTSEL automatically —
# no button-press needed for normal re-flashes. Manual BOOTSEL is only required
# if the running firmware is broken or unresponsive (see firmware/arp/README.md).
pio run -d firmware/arp --target upload
```

## Hardware

- **MCU:** Seeed XIAO RP2350 (RP2350A, dual Cortex-M33, 150 MHz)
- **DAC:** RP2350 12-bit PWM on D2 → 2-pole RC filter (2× 10 kΩ + 100 nF) → MCP6002 op-amp
- **Display:** 64×32 SSD1306 OLED, 0.49", I2C, mounted vertically
- **Controls:** 1× tempo pot, 1× PEC11 rotary encoder (with click)
- **Jacks:** J1 clock/gate in, J2 CV in, J3 V/Oct out, J4 gate out

## Key decisions

- **No onboard DAC** — RP2350 PWM on D2 + 2-pole RC filter used instead; simpler than I2C DAC
- **I2C bus** on D4 (SDA) / D5 (SCL): dedicated to SSD1306 OLED only (no bus sharing)
- **Host TDD** for pure-logic modules (scales, arp, tempo) via `pio test -e native`
- **Bench verification** for HAL (DAC, gate, OLED, encoder)
- **CHANGELOG.md** updated on every commit (Keep a Changelog format)

See `docs/decisions.md` for the full rationale and `docs/stories/` for the backlog.

## Parts inventory

When bench parts are needed, check the user's inventory at `~/Git/binkey-data/parts/` before asking them to order. It's a directory of markdown files — one per part family — with quantities and cabinet/bin locations (e.g. `cabinet-2-bin-2`). Prefer exact values from inventory; substitute when the exact value isn't stocked and document the substitution in the bench-log.
