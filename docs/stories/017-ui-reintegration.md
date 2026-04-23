# Story 017: Reintegrate UI on the New SPI Platform

**As a** developer
**I want** the existing pot/encoder/OLED UI working on the new SPI-based hardware platform, with the v0.3.0 arpeggiator behaviour intact
**So that** we have a known-good "single app" baseline before introducing the platform skeleton in Story 018

## Acceptance Criteria

### Hardware

- [ ] **Pot now goes through MCP3208 ch 0** (not native A0 anymore). Pot wiper still reads 0–3.3V at the wiper; MCP3208 sees it via either the same input-protection chain as J1/J2 (overkill for an internal pot) or directly (acceptable since the pot is a known-low-impedance source). **Decision at bench:** start with direct connection; only add a buffer if ADC noise is unacceptable.
- [ ] **CV in (was J2) is now any of J1/J2** — both jacks symmetric. App firmware decides which.
- [ ] **Encoder unchanged** — still on D8/D9/D10 (GP2/GP3/GP4) GPIO. No SPI/I2C involvement.
- [ ] **OLED unchanged** — still I2C on D4/D5 (GP6/GP7). Dedicated bus, no shared peripherals on I2C.
- [ ] **NeoPixel on XIAO** — still GP22 (data) + GP23 (power-enable). Still RGBW. Future panel cutout will let it shine through (per the platform module brief); for bench, observe directly on the XIAO.

### Firmware

- [ ] New `firmware/arp/lib/hardware/` HAL aggregator — single `Hardware` struct holding `dac`, `adc`, `oled`, `encoder`, `neopixel` references. Apps receive this struct; they don't talk to peripherals directly.
- [ ] Port `tempo` lib: `pot_volts → bpm` mapping unchanged (exponential, 20–300 BPM).
- [ ] Port the arp app from `src/main.cpp` into a self-contained module that operates on the `Hardware` struct rather than reaching for global pins. Stub for Story 018's app interface (no real abstraction yet, just a clear seam).
- [ ] Story 010 external-clock-in functionality re-implemented using the new `trigger_in::Schmitt` (Story 016) on whichever ADC input is configured for clock duty.

### Bench

- [ ] Power up. OLED shows the menu. Encoder click cycles SCALE → ORDER → ROOT. Encoder rotate changes value. Long-press resets the arp.
- [ ] Pot changes BPM live, audibly.
- [ ] Patch a quantizer or another module's V/Oct into J1 or J2. Confirm the arp transposes correctly (same as Story 009 behavior).
- [ ] Patch the MOD2 clock into J1 or J2. Confirm external-clock auto-detection kicks in within 2s; OLED shows EXT; tempo pot acts as multiplier (×1/×2/×4) per Story 010.
- [ ] **Audio comparison:** record 30 seconds of v0.3.0 (current breadboard) and 30 seconds of this story's build playing the same patch. They should sound identical. (DAC8552's 16-bit resolution should sound *better* than v0.3.0's 12-bit PWM, but the arp behaviour, timing, and feel must match.)

### Host tests

- [ ] Existing tests (`scales`, `arp`, `tempo`, `cv`) still pass on `pio test -e native`.
- [ ] No regressions in the test count (currently 45).

## Notes

- This story is the **integration smoke test** for the new platform. If anything from Stories 011–016 was wrong, it shows up here. Expect bench iteration.
- The pot's noise floor on the MCP3208 (single-ended, 12-bit, 5V VREF) is roughly 1.2 mV. That's ~0.04% of full scale. Likely fine for tempo, possibly noticeable for parameters that demand fine adjustment (though encoder + menu handles those, not the pot).
- **NeoPixel through the panel:** the actual physical implementation of the LED window is a Rev 0.1 PCB concern, not bench. For now confirm the LED works as the active-param indicator (green=Scale, blue=Order, magenta=Root) per Story 008/009.

## Depends on

- Stories 011–016

## Status

not started
