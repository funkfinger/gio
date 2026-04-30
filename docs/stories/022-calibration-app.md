# Story 022: Calibration app

**As a** module owner with a freshly assembled Rev 0.1 PCB
**I want** a dedicated calibration app that fits per-channel gain/offset constants and writes them to EEPROM
**So that** the production firmware produces musically accurate jack-side voltages without me ever opening a terminal

## Sequencing

This story is sequenced **after Rev 0.1 PCB manufacture and bring-up**. Calibrating against the breadboard buys nothing — we want to characterise the real silicon, real PCB-trace impedances, real assembled op-amp + resistor tolerances, real REF3040 (not the pot+TL072 stand-in). Until this story ships, the production firmware uses hardcoded calibration constants from bench measurements (musically usable, not lab-grade).

Architectural decisions are captured in `docs/decisions.md` §27 — read that first.

**Hard dependencies:**
- Story 018 (platform skeleton + app switcher) must be done — this story plugs into the App interface
- Story 018 must already reserve the §27 EEPROM layout including the calibration block (so this story doesn't have to migrate existing storage)
- Rev 0.1 PCB must be assembled and bench-verified — no point calibrating against the breadboard

## Acceptance Criteria

### App registration

- [ ] New `firmware/arp/lib/apps/calibrate/` matching the `App` interface from Story 018
- [ ] `name` field set to `"Calibrate"` (visible in app switcher)
- [ ] Registered in the `APPS[]` array in `main.cpp`

### Auto-trigger on missing data

- [ ] Boot logic in `main.cpp` reads the EEPROM calibration block on startup
- [ ] If magic byte ≠ `'GIO!'`, layout version mismatches, or CRC32 fails:
  - Log to serial (`"calibration missing/invalid; entering calibration app"`)
  - Override the "last loaded app" and load the calibrate app instead
  - After successful calibration, the next boot loads the user's actual chosen app
- [ ] Hardcoded firmware-default calibration is loaded into the HALs as a fallback so the app's own analog stages still work during the calibration ritual itself (chicken-and-egg avoidance)

### Calibration UI flow

- [ ] **Welcome screen** — OLED shows "Calibrate? CLICK = yes / LONG = cancel". User can back out.
- [ ] **Step 1: Patch prompt** — OLED shows "Patch J3 → J1, then click". Wait for click.
  - On click, run channel-A loopback sweep (next step). 
  - On long-press, abort (return to app switcher; calibration not written).
- [ ] **Step 2: Channel-A sweep** — DAC8552 ch A writes 5 evenly-spaced voltages within the safe range (e.g., 0.4 V, 1.2 V, 2.0 V, 2.8 V, 3.6 V — avoiding the rails). At each point, hold for 100 ms (settling), read MCP3208 ch 0 (averaging 16 samples for noise reduction), capture the (DAC volts, ADC volts) pair.
- [ ] **Step 3: Fit channel A** — least-squares linear regression on the 5 data points. Compute output gain/offset and input gain/offset per the §27 math (REF3040 trusted; DAC ideal; cascaded error attributed to the input stage).
- [ ] **Step 4: Patch prompt for channel B** — "Patch J4 → J2, then click". Same loopback sequence on channel B.
- [ ] **Step 5: Sanity check** — verify all 4 fitted constants are within reasonable bounds (e.g., gain in [0.5, 2.0] of nominal; offset in [-VREF/2, +VREF/2]). If any are out of range, show "Calibration failed — check patch cable" and don't write to EEPROM.
- [ ] **Step 6: Confirm + write** — show summary on OLED ("4 channels calibrated. Save?"). Click writes to EEPROM with magic + version + CRC; long-press cancels.
- [ ] **Step 7: Return to app switcher** — automatic after successful write.

### EEPROM I/O

- [ ] Read on boot: validate magic + version + CRC, load calibration into HALs
- [ ] Write on completion: pack 4× (gain + offset) floats per the §27 layout, compute CRC, write magic + version, commit
- [ ] Use arduino-pico's `EEPROM.h` (last 4 KB sector of flash; survives normal `picotool` re-flash)

### Bench verification

- [ ] Fresh module (no calibration in EEPROM) → boot → calibrate app auto-launches
- [ ] Walk through full sequence with patch cables; verify fitted constants are stable across two consecutive runs (within ±1 % of each other)
- [ ] After successful calibration, power-cycle → calibrate app does NOT re-launch; user's chosen app loads
- [ ] In the user's chosen app (e.g. arp), confirm V/Oct accuracy at jack J3 against an external multimeter — should be within ±5 cents per octave across C0–C7
- [ ] Re-trigger calibration via app switcher, complete it again, verify boot behavior still correct

### Host tests

- [ ] Linear-fit math has unit tests with synthetic data (perfect line + line + noise)
- [ ] CRC32 round-trip test
- [ ] Storage-format pack/unpack round-trip test

## Notes

- **Why 5 sweep points, not more or fewer:** 2 points minimum for a linear fit; 5 gives noise rejection without noticeably extending the calibration time. Each point needs ~100 ms settling + 16 ADC averages × ~50 µs = ~10 ms — total per channel ~600 ms. Both channels = ~1.2 s of sweep time. Add UI prompts and the whole calibration ritual is < 30 seconds.
- **Why avoid the rails in the sweep:** the output stage's swing isn't perfectly symmetric near ±10 V (op-amp output saturates a few hundred mV before the rails). Sweep points at 0.4 V and 3.6 V correspond to about ±7 V at the jack, comfortably linear.
- **What if the user doesn't have a patch cable handy:** they can long-press to abort. The hardcoded firmware-default calibration stays in effect (musically usable). Re-run via app switcher when they have a cable.
- **Why no held-button-at-boot recovery:** if the app switcher itself is broken, USB re-flash recovers everything. Adding a held-button recovery path costs code complexity for a vanishingly rare scenario. (See §27.)
- **Future extensions** (NOT v1):
  - REF3040 verification via test-point pad on the back of the PCB
  - Multimeter-anchored output calibration (for users who need lab-grade external V/Oct)
  - Periodic re-calibration prompts (currently rejected — hardware doesn't drift)
  - Per-app calibration profiles (e.g., "calibrate for PWM-style square outputs vs. clean V/Oct") — overengineered for v1

## Depends on

- Story 018 (platform skeleton, app switcher, EEPROM layout reservation)
- Rev 0.1 PCB manufactured + assembled + bench-verified

## Status

deferred — design captured (decisions.md §27), implementation gated on PCB build
