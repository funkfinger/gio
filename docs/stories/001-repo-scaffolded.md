# Story 001: Repo Scaffolded

**As a** developer
**I want** the repository initialised with directory structure, licensing, CI, and baseline docs
**So that** subsequent work lands on an enforced, documented foundation

## Acceptance Criteria

- [ ] `firmware/arp/`, `hardware/`, `docs/` directories exist at the repo root
- [ ] `hardware/README.md` placeholder explains the directory is populated after breadboard validation (see decisions.md §7)
- [ ] Three `LICENSE-*` files at repo root: `LICENSE-firmware` (MIT), `LICENSE-hardware` (Apache 2.0), `LICENSE-docs` (CC-BY 4.0)
- [ ] Root `README.md` points at spec, decisions, stories, and each license
- [ ] `.gitignore` covers PlatformIO (`.pio/`, `*.pio/`), KiCad backups (`*.bak`, `*-backups/`), macOS metadata (`.DS_Store`), and editor files
- [ ] `.github/workflows/ci.yml` triggers on push and pull_request; runs host tests (`pio test -e native`) when a `[env:native]` env and `test/` directory exist; passes vacuously until firmware exists
- [ ] `CHANGELOG.md` at repo root with an `## [Unreleased]` section
- [ ] `CLAUDE.md` at repo root with build instructions and hardware summary
- [ ] Initial commit pushed to `main` on `github.com/funkfinger/gio` with CI green

## Notes

- No firmware code yet. `firmware/arp/` is empty or contains a placeholder `README.md`.
- CI is scaffolded early so later PRs don't also have to set it up.
- The `docs/generative-arp-module.md` spec and `docs/decisions.md` are part of this story — they define the contract everything else builds on.
- Ported from `xiao-ra4m1-arp` Story 001. Same structure, different repo name and hardware.

## Status

not started
