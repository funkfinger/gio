# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and firmware in this repo follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Hardware uses `Rev N.M` notation, silkscreened on the PCB. See `docs/decisions.md` §13 for the full scheme.

**Convention:** this file is updated on every commit. Changes in-flight go under `## [Unreleased]`; on a firmware tag (`arp/vX.Y.Z`), those entries move under a new `## [arp/vX.Y.Z] — YYYY-MM-DD` heading.

Section keys: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`, `Docs`.

---

## [Unreleased]

### Changed

- DAC approach: replaced MCP4725 I2C DAC with RP2350 12-bit hardware PWM on D2 + 2-pole RC filter (2× 10 kΩ + 100 nF, f_c ≈ 160 Hz). Eliminates I2C bus contention between DAC and OLED, removes MCP4725 from BOM, simplifies firmware (no library needed). Ripple < 63 µV; settling time ~5 ms. Spec, decisions, and Stories 003–005 updated accordingly. D2 pin reassigned from CV In #2 to PWM V/Oct out.
- Input jacks: J1 moved from D7 (digital-only, GP1) to A3 (GP29, ADC-capable). Both jacks dual-purpose.
- Timing architecture: `millis()` state machine (non-blocking) chosen over `delay()` for gate/step timing. Allows encoder, OLED, pot, and external clock to be serviced every loop iteration. Hardware timer (Path B) deferred unless bench reveals audible jitter. Both J1 and J2 now on ADC pins with identical protection circuits (100 kΩ series + 68 kΩ to GND + BAT54 dual Schottky). Both jacks are dual-purpose: CLOCK mode (rising-edge trigger) or PITCH mode (V/Oct → quantise), selectable per jack in encoder menu. Maps 0–8V V/Oct → 0–3.24V ADC (42 counts/semitone). D7 (GP1) freed as spare digital GPIO.

### Added

- Repo scaffolded: `firmware/arp/`, `hardware/`, `docs/`, `docs/stories/` directory tree
- `LICENSE-firmware` (MIT), `LICENSE-hardware` (Apache 2.0), `LICENSE-docs` (CC-BY 4.0)
- Root `README.md` — project summary, status, toolchain, versioning, license map
- `CLAUDE.md` — build instructions and hardware summary for Claude Code sessions
- `.gitignore` covering PlatformIO, KiCad, editor, macOS metadata, build artefacts
- `.github/workflows/ci.yml` — host tests + firmware compile-check on push and PR
- `firmware/arp/platformio.ini` — earlephilhower arduino-pico platform, `seeed_xiao_rp2350` board, native test env
- `firmware/arp/src/main.cpp` — placeholder blinky (Story 002 content)
- `firmware/arp/README.md` — build, upload (UF2), test, library notes

### Docs

- `docs/generative-arp-module.md` — Rev 0.1 spec: 2HP, XIAO RP2350, MCP4725 DAC, 64×32 OLED, 1 pot, 1 encoder, 4 jacks
- `docs/decisions.md` — tooling, scope, workflow decisions; rationale for MCP4725, I2C sharing, port from RA4M1, library reuse
- `docs/stories/README.md` — story format and workflow
- `docs/stories/001` through `008` — backlog from repo scaffolding through encoder menu integration
