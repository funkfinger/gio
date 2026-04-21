# gio

A 2 HP Eurorack generative arpeggiator module built around the Seeed XIAO RP2350. Produces scale-quantised V/Oct pitch CV and gate outputs driven by a probability-layered generative engine. Open-source hardware and firmware.

Spiritual successor to [funkfinger/xiao-ra4m1-arp](https://github.com/funkfinger/xiao-ra4m1-arp), ported to the RP2350 with an encoder + OLED UI and an external I2C DAC (MCP4725) in place of the RA4M1's onboard DAC.

## Status

**Planning / pre-prototype.** No hardware fabricated yet; no firmware released. See [`docs/decisions.md`](docs/decisions.md) for the bring-up plan and [`docs/stories/`](docs/stories/) for the story-driven development backlog.

## Repo layout

```
gio/
├── firmware/arp/     PlatformIO project for the arpeggiator firmware
├── hardware/         KiCad project — populated post-breadboard validation
├── docs/             Specification, decisions, user stories
└── CHANGELOG.md      Keep a Changelog format; updated on every commit
```

## Documentation

- **[Specification](docs/generative-arp-module.md)** — hardware spec, firmware architecture, BOM, calibration
- **[Decisions](docs/decisions.md)** — tooling, scope, workflow, licensing, versioning
- **[Stories](docs/stories/)** — user-story-driven development backlog

## Toolchain

- **Firmware:** [PlatformIO](https://platformio.org/) with the earlephilhower Arduino-Pico platform (`board = seeed_xiao_rp2350`)
- **Tests:** host-side unit tests via `pio test -e native` for pure-logic modules (scales, arp, tempo). HAL is bench-verified.
- **Hardware:** KiCad (project deferred — see decisions)
- **CI:** GitHub Actions runs host tests on push and PR

## Versioning

- **Firmware:** [Semantic Versioning](https://semver.org/), scoped tags per firmware: `arp/v0.1.0`
- **Hardware:** `Rev N.M` notation, silkscreened on the PCB
- **Docs:** unversioned; history lives in `git log`

See [decisions §12](docs/decisions.md) for full rules.

## Licensing

| Area | License | File |
|---|---|---|
| Firmware | MIT | [`LICENSE-firmware`](LICENSE-firmware) |
| Hardware (KiCad, panel artwork) | Apache 2.0 | [`LICENSE-hardware`](LICENSE-hardware) |
| Documentation | CC-BY 4.0 | [`LICENSE-docs`](LICENSE-docs) |
