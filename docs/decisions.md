# Design & Process Decisions

Captured from the project kick-off. These settle scope, tooling, and workflow for the first revision of the module. Source of truth for decisions that aren't obvious from the code.

---

## Context

This repo (`gio`) is a 2 HP Eurorack generative arpeggiator based on the XIAO RP2350, specified in [`generative-arp-module.md`](./generative-arp-module.md). It is a spiritual successor to [`xiao-ra4m1-arp`](https://github.com/funkfinger/xiao-ra4m1-arp), adopting the design pivot (encoder + OLED, 2HP, more CV inputs) as the baseline rather than a retrofit.

---

## Decisions

### 1. Monorepo with three top-level areas

```
gio/
├── firmware/arp/     PlatformIO project for the arpeggiator firmware
├── hardware/         KiCad project (schematic, PCB, panel) — added post-v0.1
└── docs/             Spec, decisions, stories
```

**Why:** hardware and firmware revs are tightly coupled on a single-PCB module. Pin assignments are the contract between them and should version together.

### 2. PlatformIO as the build system

Using the earlephilhower arduino-pico platform:

```ini
platform = https://github.com/earlephilhower/arduino-pico.git
board    = seeed_xiao_rp2350
framework = arduino
```

**Why PlatformIO:** scriptable builds, `pio test -e native` for host-side unit tests, library management, CI-friendly.

**Fallback:** `arduino-cli` if PlatformIO hits a wall with the RP2350 board definition.

### 3. PWM DAC instead of external I2C DAC (MCP4725)

The RP2350 has no onboard DAC. Rather than adding an external MCP4725 I2C DAC, V/Oct output uses the RP2350's 12-bit hardware PWM peripheral on D2 (GP28) with a 2-pole passive RC low-pass filter feeding the MCP6002 op-amp.

**PWM DAC parameters:**
- PWM frequency: 150 MHz / 4096 ≈ 36.6 kHz (12-bit wrap, no clock divider)
- Filter: 2× (10 kΩ + 100 nF), f_c ≈ 160 Hz, −40 dB/decade above cutoff
- Ripple at 36.6 kHz: ≈ 63 µV (< 0.1¢) — well below the 2 mV spec
- Settling time (5τ): ≈ 5 ms — fine at 50 ms/step (300 BPM, 16th notes)
- Firmware write: `analogWrite(PIN_DAC_PWM, count)` — no library needed

**Why not MCP4725:**
- MCP4725 shares I2C bus with the OLED → potential bus contention during OLED refresh (~5.8 ms for a full 64×32 frame)
- Extra IC, extra BOM cost, extra library dependency
- PWM + RC filter uses components already in the passives BOM (same 10 kΩ / 100 nF values as op-amp network)
- `analogWrite()` updates are faster than an I2C transaction (~15–20 µs)

**Risk to validate on bench (Story 003):** 36.6 kHz PWM switching noise on D2 may couple into adjacent ADC pins (A0 tempo pot, A1 CV in) on breadboard. Scope the ADC readings while PWM is active. If coupling is a problem on PCB, route PWM trace away from analog inputs and add ground pour between them.

### 4. I2C bus dedicated to SSD1306 OLED

Hardware I2C0 on D4/D5, used exclusively by the SSD1306 OLED (address 0x3C). No bus sharing — the PWM DAC approach eliminates the MCP4725 from the I2C bus entirely.

**Benefit:** OLED can refresh at will without blocking DAC updates. Full 64×32 frame refresh takes ~5.8 ms at 400 kHz — this no longer affects V/Oct timing at all.

### 5. MVP (v0.1) is deliberately minimal

First working firmware:
- Internal clock only (tempo pot drives it)
- Major scale only
- Up-arp, 4 notes (root, 3rd, 5th, octave)
- PWM on D2 → 2-pole RC filter → MCP6002 op-amp → V/Oct out, calibrated
- Gate out via NPN transistor stage
- OLED shows tempo BPM only
- Encoder click cycles through arp orders
- No chaos, no CV in processing, no calibration mode, no NeoPixel

**Why:** the one genuinely novel subsystem is RP2350 PWM → 2-pole RC filter → MCP6002 → Eurorack V/Oct. Prove that before stacking features.

### 6. Reuse logic libraries from xiao-ra4m1-arp verbatim

`lib/scales/`, `lib/arp/`, `lib/tempo/` are pure C++ with no Arduino dependencies. They port without modification. Their existing GoogleTest suites run on the RP2350 native env unchanged.

**Why:** they were TDD'd and bench-validated on the RA4M1 project. No reason to rewrite them.

### 7. KiCad deferred until breadboard signal chain validates

Same rule as RA4M1 project: don't open KiCad until DAC → MCP6002 → multimeter matches the calibration voltage table within ±2 mV.

**New risk for this project:** 36.6 kHz PWM switching on D2 may couple noise into adjacent ADC pins. Confirm on breadboard (Story 003) before committing to PCB traces.

### 8. Dual-purpose input jacks — same hardware, firmware-selectable mode

J1 (A3/GP29) and J2 (A1/GP27) use identical protection and divider circuits: 100 kΩ series + 68 kΩ to GND + BAT54 dual Schottky. This maps 0–8V V/Oct → 0–3.24V at ADC and provides safe clamping for negative or over-voltage signals.

Both jacks can operate as either a **trigger/clock input** (`digitalRead()`, rising-edge detection above ~1.5V) or a **pitch CV input** (`analogRead()` → inverse divider → semitone → `quantize()`). Mode is selected per jack in the encoder menu (CLOCK / PITCH).

**Why:** the requirement was one trigger in and one V/Oct in. Rather than dedicate each jack in hardware — which would require different circuits and fix the panel semantics forever — both jacks get identical hardware and the split is done in firmware. Patcher can reassign either jack. D7 (GP1, previously the clock-only digital input) is freed.

**Why 100 kΩ + 68 kΩ:** the 100 kΩ series resistor limits clamp diode current to < 70 µA even at 10V. The 68 kΩ lower leg maps 0–8V → 0–3.24V, covering 8 octaves of V/Oct with 42 ADC counts/semitone — sufficient for scale quantisation. A lower series resistor (e.g., 3.3 kΩ as in the RA4M1 design) would give faster rise time but the 100 kΩ + 100 pF cable capacitance RC = 10 µs — fine at 300 BPM (50 ms/step).

### 9. Gate / clock timing: `millis()` state machine (non-blocking)

The main loop uses `millis()` timestamps instead of `delay()` for all gate and step timing. Each iteration checks whether enough time has passed to transition state (gate on → gate off → next step), then reads pots, polls the encoder, updates the OLED if needed, and checks for external clock edges — without blocking.

```cpp
// sketch of the pattern
if (millis() - stepStart >= stepMs) {
    stepStart = millis();
    // advance arp, write DAC, gate on
}
if (millis() - stepStart >= gateOnMs) {
    // gate off (if not already)
}
```

**Why not `delay()`:** the RA4M1 firmware blocked for up to 125 ms per half-step. With an encoder, OLED, two ADC reads, and external clock detection in the loop, a blocked loop drops encoder turns, misses clock edges, and delays pot reads by up to 250 ms.

**Why not hardware timer interrupt or dual-core (yet):** `millis()` is the simplest path. The worst-case loop jitter is dominated by I2C OLED refresh (~5.8 ms for a full 64×32 frame), which only happens on parameter change — not every loop. At 300 BPM (50 ms/step), 5.8 ms jitter is 12% of a step: audible in theory, likely inaudible in practice for a generative arp. **Revisit with a hardware timer (Path B) if jitter is audible on the bench.**

### 10. Test strategy: host TDD for pure logic, bench for HAL

- **Host tests (`pio test -e native`):** `scales`, `arp`, `tempo` — no Arduino deps, pure logic
- **Bench-verified:** `pwm_dac` (PWM + 2-pole RC filter → MCP6002), `gate_out`, `oled_ui`, `encoder_input`, `clock_in`

Bench workflow: oscilloscope (Rigol DS1054Z) for timing, multimeter for V/Oct voltages, VCO + tuner for pitch tracking.

### 11. Platform version pinned to a tagged release

`platformio.ini` uses `maxgerhardt/platform-raspberrypi` (the PlatformIO wrapper) with the arduino-pico core pinned via `platform_packages`:

```ini
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
platform_packages = framework-arduinopico @ https://github.com/earlephilhower/arduino-pico.git#4.4.0
```

The bare `earlephilhower/arduino-pico` git URL cannot be used directly as `platform =` because PlatformIO expects a `platform.json` manifest that the core repo does not supply.

**Why pin:**
- CI and local builds stay reproducible across time. A platform bump should be a deliberate act, not a silent surprise on the next `pio run`.
- Bench work is expensive. A platform change can shift PWM slice assignments, ADC averaging defaults, or I2C timing — any of which invalidate Story 003/004 calibration. If we're going to revalidate the bench, we do it because we chose to, not because GitHub's `main` moved.

**PWM frequency note (Story 003 finding):** arduino-pico only exposes the global `analogWriteFreq(uint32_t freq)` — there is no per-pin overload in any released version. In practice this is acceptable for this design: D2 (GP28) is the only `analogWrite` pin; gate output uses digital GPIO; NeoPixel uses PIO (not PWM slices). Call `analogWriteFreq(36621)` once in `setup()` before `analogWrite()`. If a future story needs a second PWM slice at a different frequency, use the pico-sdk `pwm_set_clkdiv()` / `pwm_set_wrap()` APIs directly.

**Bump protocol:** update the pin → run host tests → rebuild firmware → re-run Story 003 PWM ramp bench checks → record in `bench-log.md`.

### 12. 64×32 OLED (0.49"), mounted vertically

The 64×32 SSD1306 at 0.49" fits within the 2HP panel width. Mounted vertically (rotated 90° in software), the active area is 32 px wide × 64 px tall in the panel window — enough for a parameter name and value on two lines with large-ish characters.

**Alternative:** 128×32 OLED (used in the RA4M1 design pivot doc) — wider than 2HP when mounted horizontally; would need vertical mount too. The 64×32 is a better physical fit.

### 13. Git workflow: feature branches + self-reviewed PRs

Same as RA4M1 project. CI gates PRs. Self-review catches real bugs.

### 14. User stories as the unit of work

See [`stories/README.md`](./stories/README.md) for format. Each story defines a verifiable outcome. Stories are satisfied by code and, for pure-logic modules, by tests first (TDD).

### 15. CHANGELOG.md updated on every commit

Keep a Changelog format. `## [Unreleased]` in flight; moves to a versioned heading on tag.

### 16. Versioning — SemVer for firmware, Rev N.M for hardware

**Firmware:** `arp/v0.1.0`, `arp/v0.2.0`, etc. First tag on completion of Story 005.

**Hardware:** `Rev 0.1` is first fabricated board. `Rev 1.0` is first public release.

### 17. Encoder library: `mathertel/RotaryEncoder`

`paulstoffregen/Encoder` was the original plan (interrupt-driven, fast) but it does not compile on RP2350 — it uses AVR/ARM-specific direct register-access macros (`DIRECT_PIN_READ`, `pin1_register`, `pin2_bitmask`, `Encoder::interruptArgs[]`) that have no RP2350 implementation in the library. Discovered during Story 002 platform fix; library removed from `lib_deps` at that time.

`mathertel/RotaryEncoder` chosen as replacement:
- Pure C++ — no platform-specific register access
- Polling-based by default (call `tick()` from loop), with optional interrupt mode
- LatchMode handles standard PEC11 detents (`TWO03` for one count per detent)
- Active maintenance, broadly used across PIO platforms

The polling approach is fine for our 24-detent PEC11 at human turn rates; we tick every loop iteration alongside other non-blocking work. If we ever observe missed detents under heavy loop load, switching to interrupt mode is a one-line change in `lib/encoder_input/`.

### 18. RP2350-E9 silicon errata: external pulldowns must be ≤ 8.2 kΩ

> **Note (post-platform-pivot):** §19+ moves all CV/trigger jacks behind op-amp buffers + MCP3208. The RP2350-E9 rule still applies to any leftover direct-to-GPIO input, but Rev 0.1 has none — encoder + SPI CS lines are all driven actively or pulled high to +3.3V. The rule is preserved here for future stories that might add a direct-to-GPIO trigger input on a spare pin.

The RP2350 has a documented silicon bug ([RP2350-E9](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf), also covered on Hackaday): when a GPIO is configured as `INPUT_PULLDOWN` *and* the input buffer is enabled, the pad latches at ~2.2 V instead of pulling to GND. The internal pulldown alone cannot defeat this latch.

**Workaround (per Pi Foundation):** drive the input with an external pulldown of **≤ 8.2 kΩ**. That impedance can overpower the latched 2.2 V and produce a clean LOW.

**Discovered during Story 010** (clock-in on J1). The original spec'd divider was 100 kΩ + 68 kΩ — far too high-impedance. With those values the pin sat at ~2 V "low," `digitalRead` always returned HIGH, and no edges were detected. Cost roughly 90 minutes of bench debugging including swapping a XIAO RP2350 (in case the pin had been damaged by the patched MOD2 output) before the user spotted the errata.

**Implications for Rev 0.1 PCB:**
- Any GPIO input that needs a pulldown must have an external resistor ≤ 8.2 kΩ tied to GND.
- For voltage dividers feeding GPIO (e.g. clock-in, trigger-in), bias the resistor values toward 10 k / 10 k (or lower) rather than the spec's 100 k / 68 k. Adjust the dividing ratio accordingly.
- The CV input on J2 is fine because it's buffered by op-amp B (the ADC sees a low-Z source already).
- Firmware: continue to set `INPUT_PULLDOWN` as belt + braces, but treat it as decorative — the external resistor is doing the actual work.

### 19. Platform-module reframing — gio is a platform, the arp is one app

`arp/v0.3.0` shipped a fully-functional generative arpeggiator. During post-v0.3.0 design discussion (2026-04-23) we reframed gio as a **platform module**: the same hardware should host multiple swappable apps (arp, clock-out, LFO, S&H, quantizer, etc.), with the user selecting an app at runtime via the encoder.

**Why pivot now, before the PCB:**
- The hardware doesn't change much for "platform" vs "single-purpose" — the changes are mostly in firmware structure + panel labelling.
- A platform module needs **symmetric I/O** (any jack can do any job per app); we were partway there already (J1+J2 dual-mode in spec) but committed to it incompletely.
- Forcing the architecture through "must support N apps" exposes what's actually generic vs. arp-specific in the firmware. Cleaner code, smaller per-app footprint.
- The chip (RP2350 dual Cortex-M33, 4 MB flash, 520 KB SRAM) has *vastly* more headroom than the arp uses. Wasted otherwise.

**What changes:**
- Panel silkscreen goes generic (`IN 1`, `IN 2`, `OUT 1`, `OUT 2`, `KNOB`).
- Firmware grows a `lib/platform/` layer with an `App` interface + app loader + flash-persisted "last loaded app."
- HAL is aggregated into a `Hardware` struct passed to apps; apps don't reach for global pins.

**What doesn't change:**
- The arp app's behaviour is identical to `arp/v0.3.0` — just refactored to live behind the App interface.
- 2 HP form factor.
- XIAO RP2350.

### 20. SPI bus replaces PWM-DAC + native ADC for signal I/O

**Out:** RP2350 12-bit PWM on D2 + 2-pole RC filter + MCP6002 op-amp scaling (the v0.3.0 V/oct chain). Native ADCs A1 (CV in) and A3-that-doesn't-exist (clock in).

**In:** TI **DAC8552** (16-bit dual SPI DAC) + Microchip **MCP3208** (8-channel SPI ADC), both on a shared SPI bus. Each gets a dedicated CS line.

**Why:**
- **Symmetric I/O.** Both outputs become full-fidelity CV outputs that can also do gates (Story 014 verifies edge speed). Both inputs become full-fidelity CV inputs that can also do trigger detection in firmware (Story 016). No hardware mode switches; apps decide per-jack.
- **Resolution win.** DAC8552 is 16-bit (76 µV/count at 5V VREF) vs the 12-bit PWM-DAC (805 µV/count) — at jack scale that's 305 µV/count vs 3.2 mV/count, ≈ 0.4 cents vs 4 cents at V/oct. PWM-DAC was already adequate; DAC8552 is comfortably overkill.
- **No PWM-RC filter.** Eliminates the 5 ms settling time; outputs respond at SPI write rate (~5 µs).
- **Frees native ADCs.** A0 (tempo pot) stays native; A1/A2 become spares. A3-on-GP29 is no longer a problem because it never existed (board variant doesn't expose GP29).
- **Already in stock.** DAC8552 ×4 (Cab-3/Bin-37); MCP3208 ordered tomorrow. Saves the hunt for a different DAC.

**Cost:**
- One SPI bus eats 3 + N CS pins (N = number of SPI devices). For us: SCK + MOSI + MISO + 2 CS = 5 pins. The encoder has to move off D8/D9/D10 to free the SPI0 default pin set; new home is A1/A2/D3 (GP27/GP28/GP5). Polled encoder doesn't care about pin choice.
- DAC8552 is VSSOP-8 (3×3 mm). For breadboard work, mounted on a hand-soldered protoboard or a VSSOP→DIP breakout. For PCB it's a hand-solderable QFN-class part.

### 21. TL072 op-amps on ±12V replaces MCP6002 single-supply 5V

**Out:** MCP6002 single-supply rail-to-rail op-amp (5V powered) used for V/oct gain stage (v0.3.0) and CV-in buffering (Story 009).

**In:** **TL072** dual JFET op-amp running directly off Eurorack ±12V. Two TL072s for the four op-amp channels needed (2 inputs + 2 outputs); a TL074 quad is a viable consolidation (in stock as backup).

**Why:**
- **Native bipolar swing.** Eurorack signals are ±10V; we want to handle them honestly without offset gymnastics. ±12V supply lets the TL072 swing the full Eurorack range with 1–2V headroom.
- **House style.** Mutable Instruments uses TL07x throughout; their reference circuits (which we're patterning input/output protection on, §22) assume dual-supply. Stays compatible.
- **No new rail.** Eurorack already supplies ±12V; the TL072 plugs in directly. The local LM7805 is still needed for the DAC + ADC + XIAO logic side.
- **In stock.** TL072CP × 25 (Cab-3/Bin-32 §B). DIP-8, breadboard-friendly.

**Cost:**
- Need the negative rail (was previously not strictly necessary). Eurorack provides it; trivial.
- TL072 input bias current is pA-range (JFET) — same as MCP6002 — so high-impedance input nodes are still safe.

### 22. Mutable Instruments–style input + output protection on every jack

Every jack gets the same protection topology, modelled on Mutable's published designs:

**Inputs:**
- 100 kΩ series resistor (current-limits clamp diodes during abuse)
- Dual Schottky (BAT43) clamps to ±12V rails (caps over-voltage at ±12.4V; current limited by the 100 kΩ to < 30 µA at ±15V abuse)
- TL072 inverting summing amp scales ±10V at jack to 0–5V at MCP3208 (with offset reference from +5V)

**Outputs:**
- TL072 inverting summing amp scales 0–5V at DAC to ±10V at jack (with offset reference from +5V; firmware compensates for inversion)
- 1 kΩ series resistor at output (limits short-circuit current to ~12 mA, well within TL072 protection)
- Dual Schottky (BAT43) clamps to ±12V at the jack (protects against rail-shorts patched in by mistake)

**Why:**
- **Patch-cable abuse is real.** A user plugging +12V from a power module into our input or shorting our output to ±12V should not destroy the module. MI-style protection survives arbitrary patch-cable mistakes.
- **In-stock parts.** BAT43 × 50+; TL072 × 25; standard SMT resistor reels. No exotic parts.
- **Convention compliance.** Anyone who's worked on Eurorack DIY recognises the topology immediately; review and trust come for free.

### 23. Generic jack labels + dual-purpose jack convention

Panel silkscreen labels jacks by direction only (IN/OUT), not by function (no "TEMPO," "V/OCT," "GATE," "CV IN" labels). The OLED tells the user what each jack is doing in the current app.

**Why:**
- **Direct consequence of §19.** A platform module can't lock jack semantics in silkscreen — different apps use them differently.
- **Both inputs are dual-purpose** (CV or trigger detected in firmware via Schmitt — see Story 016). Both outputs are dual-purpose (CV via `outputs.write(volts)` or gate via `outputs.gate(on)` — same hardware). Labels would mislead.
- **Visual distinction:** inputs vs outputs distinguished by jack body colour or position (TBD at panel-design time; Mutable convention is white = in, black = out).

---

## Bring-up sequence (first five PRs)

| PR | Story | Outcome |
|---|---|---|
| 1 | [001](./stories/001-repo-scaffolded.md) | Repo skeleton, licenses, CI, `.gitignore` |
| 2 | [002](./stories/002-xiao-blinks.md) | Firmware builds and blinks the XIAO RP2350 |
| 3 | [003](./stories/003-dac-and-quantiser.md) | PWM DAC + 2-pole RC filter emits clean ramp; scale quantiser TDD'd on host |
| 4 | [004](./stories/004-voct-tracks-octaves.md) | Op-amp breadboard + calibration; V/Oct ±5¢ across C3–C7 |
| 5 | [005](./stories/005-arp-plays-up-pattern.md) | Minimal arpeggiator — **v0.1 makes music** |

After PR 5, everything else (encoder menu, OLED display, six scales, CV in, chaos, NeoPixel, calibration mode, KiCad) is additive.

---

## Deferred decisions

### Resolved by the platform pivot (§19–§23)

- ~~**CV divider bench confirmation**~~ — superseded by §22 MI-style protection topology. Story 015 will validate the new circuit.
- ~~**External-clock divider feel**~~ — implemented in `arp/v0.3.0` with multiplier (×1/×2/×4) replacing divider. Now part of arp-app behaviour, not a platform-level concern.
- ~~**"gio as clock source" feature**~~ — implemented as a first-class app (`apps/clock_out/`) per Story 018. Resolves the deferred question.

### Still open

- **Calibration storage:** hardcoded `gain` / `offset` constants in Rev 0.1. Move to flash/EEPROM post-platform-skeleton (own story).
- **OLED rotation strategy:** software rotation (Adafruit GFX `setRotation(1)`) or hardware OLED orientation strap — confirm with the 0.49" 64×32 part in hand.
- **App-private settings persistence:** Story 018 persists "last loaded app" only. Per-app state (e.g. arp's last scale/order/root) needs a follow-up story with a flash-partition scheme.
- **Calibration-utility app:** Stories 013/015 use hand-rolled bench calibration via `main.cpp` sweeps. A dedicated calibration app (selectable via app switcher) would be friendlier and could persist constants per channel. Defer until there's a concrete pain point.
- **Chaos design (arp app feature):** deferred from RA4M1 project, still deferred. Independent of platform work.
- **RNG choice:** `random()` fine for MVP; XORShift or `std::minstd_rand` for reproducible host tests post-MVP.
- **PCB jumper sanity check (lesson from MOD2 build):** the MOD2 Rev A board has a JP2 (solder-bridge across C18 for AC/DC coupling) where the JP2 pads do not actually route to C18's pads — bridging JP2 had no electrical effect. Workaround: solder a wire directly across C18. **Apply to gio Rev 0.1:** during KiCad work, verify every solder-jumper / option-jumper symbol's connections survive the schematic-to-PCB transfer (ERC + DRC + manual eyeball each JP-named net). Cheap insurance.
- **Second pot for Rev 0.2:** considered and deferred. One-pot UI is workable for many apps via the encoder-menu / pot-modulation convention, but multi-knob apps (LFO with rate + shape, dual-VCA with two gains, etc.) would benefit. Add when the first one-pot-cramps-an-app moment shows up.
- **NeoPixel light pipe:** mechanical detail of the panel PCB (clear plastic insert vs. clear-resin-fill vs. open pinhole). Pick at panel-design time.
- **DAC8552 VREF source:** Stories 011–013 use +5V from LM7805 as VREF. Add a precision 4.096V reference IC (REF3040 or similar) only if bench reveals temperature drift > ±1 mV at V/oct. The 16-bit DAC is wasted resolution if VREF is noisy/drifty.
- **Trigger output edge speed:** §20 assumes DAC8552 + TL072 produces edges fast enough for downstream gates (Story 014 verifies). If bench reveals some module that double-fires, add a 74HC14 Schmitt buffer (in stock as `sn74hct14n-hex-schmitt-trigger.md`) on the gate-likely outputs.
