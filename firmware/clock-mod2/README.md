# firmware/clock-mod2

Minimal bench clock source for a HAGIWO MOD2 board populated with a XIAO RP2350. Exists primarily to feed a clock signal into `gio`'s J1 input during Story 010 bring-up and beyond — but it's a standalone useful module on its own.

## What it does

- **POT1 (A0)**: sets the clock rate. Exponential curve, 20–300 BPM (via the shared `tempo::potToBpm()`).
- **GP1**: drives the MOD2 output stage. 50 % duty square wave — scaled to ~0→10 V at the jack by the on-board op-amp.
- **GP5**: panel LED blinks in sync with each rising edge. Works on USB power alone, so you can verify the rate before putting the module back in the rack.
- POT2, POT3, CV in, button, IN1, IN2: unused in v1.

## Hardware requirements

- HAGIWO MOD2 PCB, populated with XIAO RP2350.
- **C18 (1 µF series cap on the OUT path) must be bypassed** for DC coupling. JP2 is *meant* to do this — see "JP2 board issue" below.
- ±12 V Eurorack power for the op-amp output stage. USB power alone runs the XIAO and LED but the output stays at 0 V.

## JP2 board issue (this build, Rev A)

The MOD2 schematic shows JP2 as a solder-bridge bypass across C18 (1 µF). When the bridge is closed, the OUT path is DC-coupled and produces clean 0→10 V gates. When open, the path is AC-coupled through C18 and an output square wave appears at the J6 jack as differentiated spikes (positive and negative, decaying with τ ≈ 100 ms via R20 = 100 kΩ).

**On this physical build, soldering JP2 had no effect.** Multimeter continuity confirmed JP2's pads were bridged after soldering, but the bypass didn't reach C18 — apparently the JP2 PCB traces don't route to C18's pads on this revision (board defect or layout error). Confirmed by directly clipping across C18's terminals: signal becomes a clean square wave immediately.

**Fix on this build:** solder a small wire (or 0 Ω resistor) directly across C18's pads. Permanent and reliable.

**Lesson for gio Rev 0.1 PCB:** before fab, verify that every solder-jumper symbol's connections in the schematic actually route to the correct nets in the layout (a quick ERC + DRC + visual on the JP nets). See `docs/decisions.md` "Deferred decisions" for the gio implication.

## Build

```bash
pio run -d firmware/clock-mod2
```

Shares toolchain with `firmware/arp` — the same pinned `framework-arduinopico@4.4.0` via `maxgerhardt/platform-raspberrypi`. Reuses `../arp/lib/tempo/` via `lib_extra_dirs` so BPM math stays in lock-step with gio.

## Upload

```bash
pio run -d firmware/clock-mod2 --target upload
```

Same workflow as the gio firmware — `picotool` soft-resets the board into BOOTSEL via USB, no button-press needed for normal re-flashes. Manual BOOTSEL only required for first-flash of a fresh XIAO or recovery. See `firmware/arp/README.md` for the manual procedure.

## Serial monitor

```bash
pio device monitor -d firmware/clock-mod2
```

Baud: 115200. Mostly quiet — just a startup banner.

## Wiring from MOD2 jack to gio J1

For Story 010 bench use:

- **MOD2 J6 (OUT)** tip → **gio J1** tip (through the 100 k + 100 k divider we're building for J1).
- **MOD2 GND** → **gio GND** (through the common Eurorack ground, or a jumper if both on the same bench).

MOD2 is an ~0→10 V gate source; gio's input divider brings that down to a safe ADC level.
