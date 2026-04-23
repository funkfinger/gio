# Story 010: External Clock Input on J1

**As a** patcher
**I want** to patch a clock signal into J1 and have the arpeggiator step on each rising edge
**So that** gio can sync to the rest of my rack instead of free-running on its internal tempo

## Acceptance Criteria

### Hardware

- [ ] J1 wired through a divider to **D3 / GP5** (digital pin). Bench: 100 kΩ series + 100 kΩ to GND (1:2 ratio).
- [ ] **Pin reassignment from spec.** Spec called for J1 on GP29 (A3, ADC-capable) but the XIAO RP2350 doesn't expose GP29 — only GP26/GP27/GP28 are ADC-capable, and all three are used (tempo pot / CV in / PWM out). J1 is digital-only on this build; pitch mode on J1 is deferred until an ADC channel is freed. Spec doc to be amended.
- [ ] No op-amp buffer for J1.
- [ ] No diode clamp (the 100 kΩ series + RP2350 internal clamp diodes are sufficient for known 5–10 V sources).

### Firmware

- [ ] Edge detection on D3 / GP5 via `digitalRead`. The GPIO's built-in input threshold (~1.6 V) handles hysteresis cleanly enough for clock-rate signals.
- [ ] `clockMode` is **auto-detected**: external if any rising edge in the last 2 seconds, else internal. No menu item; cable presence governs behaviour.
- [ ] When external: arp advances **once per rising edge** (1 pulse = 1 step). Gate length on the output adapts to half the last inter-edge interval (so gate-off sits in the middle of the next quiet stretch).
- [ ] When internal: existing tempo-pot behaviour (Story 006) unchanged.
- [ ] Tempo pot when external acts as a clock divider — bottom third = ÷1, middle = ÷2, top = ÷4. Hysteresis on the boundaries.
- [ ] OLED: small "EXT" indicator visible when external clock is active. Disappears within ~2 s of cable being unplugged.

### Bench

- [ ] Patch the `clock-mod2` module's J6 → gio J1 via the divider. Power both via Eurorack.
- [ ] Confirm: arp follows the MOD2 clock (LED blinks in sync, audible pitch advances per pulse).
- [ ] Sweep MOD2 POT1 — gio's arp tracks rate changes immediately.
- [ ] Sweep gio's tempo pot — arp speed divides by 1× / 2× / 4×, audibly.
- [ ] Unplug the cable — arp falls back to internal tempo within ~2 s. Plug back in — re-locks immediately.

## Notes

- The J1 ADC pin (A3 / GP29) is read in the main loop, not at step boundaries — needs to catch every rising edge regardless of arp timing.
- Polling rate: every loop iteration, so kHz range. Plenty for clock rates up to a few hundred Hz.
- Op-amp buffering deferred: J1 may eventually want the same buffered topology as J2 (CV in) for pitch mode, but we have no spare op-amp in the MCP6002. Future PCB: add a second MCP6002 if J1's pitch mode is wanted.

## Depends on

- Story 009 (op-amp B already used by CV in; we're not changing that).

## Status

done — bench-confirmed 2026-04-23. J1 wired through a 10 kΩ + 10 kΩ divider (changed from spec'd 100 k + 68 k to defeat RP2350-E9 silicon errata; see `decisions.md` §18) into D3 / GP5. Pin reassigned from spec'd `A3 / GP29` (not exposed on XIAO RP2350) — J1 is digital-only for now. Auto-detect of external clock works; tempo pot acts as divider when external active; OLED shows INT/EXT badge. Bench-driven by a HAGIWO MOD2 running our `firmware/clock-mod2/` clock source.
