# Story 018: Platform Skeleton — App Loader, Settings, Switching

**As a** developer
**I want** a minimal "app loader" framework so that gio can host multiple apps selectable via the encoder
**So that** the module becomes a true platform (not a single-purpose arp), and adding new apps in future stories is a known, repeatable workflow

## Acceptance Criteria

### Architecture

- [ ] **App interface** in `firmware/arp/lib/platform/app.h`:
  ```cpp
  struct App {
    const char* name;                          // shown in app switcher menu
    void (*setup)(Hardware& hw);               // called once when app is loaded
    void (*loop)(Hardware& hw);                // called every main-loop iteration
    void (*onEncoderShortPress)(Hardware& hw); // optional; nullptr = ignored
    void (*onEncoderLongPress)(Hardware& hw);  // optional; nullptr = "open app switcher"
  };
  ```
- [ ] **Two apps shipped in this story:**
  - `apps/arp/` — the Story 017 arp behavior, refactored into the App interface
  - `apps/clock_out/` — the `clock-mod2` firmware ported to gio's hardware (one DAC channel as square-wave clock source, pot sets rate)
- [ ] **App registry** in `firmware/arp/src/main.cpp`: a `static const App* APPS[]` array. Adding an app = adding an entry.

### App switcher UI

- [ ] **Default long-press behavior** is now "open app switcher menu" (not "reset arp" — that becomes an arp-app-specific in-menu action).
- [ ] App switcher: OLED shows app name + selection cursor. Rotate encoder to scroll through apps. Click to load. Long-press to cancel and return to current app.
- [ ] Loading an app: calls current app's teardown (none for now), calls new app's `setup()`, then enters its `loop()`.

### Persistence

- [ ] **Last-loaded app saved to flash** on app switch. On boot, the saved app loads automatically.
- [ ] Storage: arduino-pico exposes the RP2350 flash via `EEPROM.h` (emulated EEPROM in flash; lives in the last 4 KB sector and survives normal `picotool` re-flash).
- [ ] **Reserve the full EEPROM layout from `decisions.md` §27**, not just the fields this story uses. Story 022 (calibration app) will fill the calibration block + CRC32 region without needing any storage migration.

  ```
  Offset  Size  Field                       Owner / written by
  ------  ----  -------------------------   ----------------------------
  0x00      4   Magic 'GIO!'                 Story 018 (this story)
  0x04      2   Layout version (uint16)      Story 018
  0x06      1   Last loaded app (uint8)      Story 018
  0x07      1   Reserved                     —
  0x08     32   Calibration block            Story 022 — zero-padded for now
  0x28      4   Calibration CRC32            Story 022 — zero for now
  0x2C    ...   Reserved for future use      —
  ```

  Boot logic for this story: validate magic + version, read last-app, default to app 0 if invalid. The calibration fields stay untouched (Story 022 owns them).

- [ ] App-private settings storage: out of scope for this story AND no follow-up story planned. Per the `decisions.md` §27 grilling outcome: app-private state is RAM-only, lost on app switch.

### Bench

- [ ] Boot → arp app loads (default).
- [ ] Long-press encoder → app switcher appears. Rotate to highlight "Clock Out". Click → clock-out app loads. Pot now sets clock rate; J3 outputs square wave.
- [ ] Power-cycle → clock-out app reloads automatically.
- [ ] Long-press encoder → switcher → arp → click → arp loads. Power-cycle → arp persists.
- [ ] Stress: switch apps 50 times in a session. No crash, no flash corruption.

### Host tests

- [ ] App registry compiles with both apps; `APPS[0]` and `APPS[1]` both have non-null `setup`/`loop`.
- [ ] Mock `Hardware` struct allows host-side execution of an app's `setup()` + a few `loop()` ticks (smoke test only — real behaviour stays bench-tested).

## Notes

- **Scope discipline:** this story explicitly does NOT include:
  - App-private settings (each app saving its own scales/orders/etc — separate story)
  - Boot-time app selection via held button (recovery / safe mode — useful, not now)
  - App live-reload via SD card or USB MSC (cool, way later)
  - Inter-app communication / shared state (probably never — apps are isolated by design)
- **Long-press hijacking:** Story 008 used long-press for "reset arp." That semantic moves to a soft-button or a held-double-press in the arp app — TBD when porting. The platform-level long-press always means "app switcher."
- **Apps as a discoverability story:** future apps to consider: LFO, S&H, slew limiter, quantizer, envelope generator, V/Oct calibrator, Turing-machine random sequencer. The hardware (2 ins, 2 outs, all symmetric) supports all of these; firmware is the only constraint.

## Depends on

- Story 017 (UI working on new platform)

## Status

not started
