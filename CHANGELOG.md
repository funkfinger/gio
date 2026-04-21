# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and firmware in this repo follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Hardware uses `Rev N.M` notation, silkscreened on the PCB. See `docs/decisions.md` §13 for the full scheme.

**Convention:** this file is updated on every commit. Changes in-flight go under `## [Unreleased]`; on a firmware tag (`arp/vX.Y.Z`), those entries move under a new `## [arp/vX.Y.Z] — YYYY-MM-DD` heading.

Section keys: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`, `Docs`.

---

## [Unreleased]

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
