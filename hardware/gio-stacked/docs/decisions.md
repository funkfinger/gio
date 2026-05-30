# gio-stacked architecture decisions

Scoped to this subproject. The parent `/docs/decisions.md` covers
platform-level decisions; this file covers what's specific to the
three-board redesign.

---

## 1. Three stacked boards (power / signal / faceplate)

A single 4 HP PCB ran out of area for the expanded feature set (6 jacks
instead of 4, tap-tempo button, same OLED + pot + encoder). Splitting into
three depth-stacked boards trades vertical real estate for layout headroom.

**Why three (not two):**
- Faceplate must be a PCB to hold jacks + encoder + pot + button + OLED on its back side — that's one PCB by necessity
- Power circuitry (Eurorack header, bulk caps, ±12 V regulators, REF3040) on its own board isolates supply noise from the signal-domain analog stages and keeps the heavy/bulky components (caps, regulators) at the back of the module
- Signal board in the middle is pure SMD — easy to lay out densely without competing for area with through-hole jacks or bulk power components

**Why not two (signal + faceplate, with power on the signal board):**
- Tried mentally — power components compete with the dense SMD analog stages for area on signal. Splitting them out lets each board be optimized for its dominant component class.

## 2. 2.54 mm pin-header interconnect (vs board-to-board / castellated)

Standard 0.1" pin headers with female sockets across each board junction.

**Why:**
- Trivially sourceable, cheap (~$0.50 per pair).
- Hand-solderable on both sides.
- Probable at each pin for bench debugging — you can lift one board off the stack and clip a scope probe to any signal mid-development.
- Compatible with through-hole prototyping (perfboard adapter, breakout cable for early bring-up).

**Trade-off:** less compact than board-to-board connectors (Pico-Lock), no polarization. We accept the polarization risk by recommending shrouded headers on `J_PWR` where reversal is dangerous; `J_FACE` ships un-shrouded with clear silkscreen orientation marks.

## 3. Same MCU + analog stack as `hardware/gio/`

Keep XIAO RP2350, DAC8552, MCP3208, REF3040, TL072s. Don't change horses
mid-stream; the platform is bench-validated. Calibration constants from the
v1 design carry over directly (op-amp stages are channel-symmetric per
`bench-log.md` 2026-05-21).

The two new CV ins (J3, J4 in panel layout — channels 3 and 4 in firmware)
duplicate the existing input stage. MCP3208 has 5 unused channels (CH3–CH7);
we're claiming two of them, leaving 3 for future expansion.

## 4. Tap-tempo button on XIAO D0

D0 was freed when the tempo pot moved to MCP3208 CH0 (pre-pivot, the pot was
on D0 as a native analog input). It's the only free GPIO on the XIAO that
isn't part of SPI / I²C / encoder. Wire the button between D0 and GND, use
the RP2350 internal pull-up, debounce in firmware.

If a second button is ever wanted, options are: another MCP3208 channel
read as a digital threshold (works but uses analog bandwidth), an I²C
button expander (cheap, but adds a part), or sharing the OLED `OLED_RST`
pin if we hard-wire OLED reset and free that line.

## 5. Faceplate is a real PCB (FR4 with black soldermask)

Not milled aluminum. Decision driven by mechanical: the jacks, button, OLED
breakout headers, encoder, and pot all need to physically mount somewhere,
and a PCB is the cheapest sane mounting surface. Aluminum panels would
require a separate "jack PCB" behind them anyway, defeating the purpose.

Visual: black soldermask + matte finish + white silkscreen approximates a
"black panel" look adequately for a DIY/boutique aesthetic. Not as premium
as anodized aluminum but well within DIY norms.

## 6. Maximum depth ≈ 40 mm — at Eurorack standard limit

Three PCBs (1.6 mm each = 4.8 mm) + 14 mm + 13 mm standoffs + 9 mm Eurorack
header = 40.8 mm. Standard Doepfer A-100 cases allow 40 mm before the bus
board interferes. We have ~zero margin.

Trim path if needed at fab time: reduce the signal-to-power standoff from
13 mm to 10 mm (requires signal-board components to fit in the smaller gap)
or use SOT-223 instead of TO-220 for the LDO. Defer until parts placement
reveals which constraint is binding.

## 7. Inter-board pinouts locked before schematic work

`docs/interconnect.md` is the contract. With the pinouts fixed, the three
boards can be schematic-designed independently; the only thing they need to
agree on is the pin assignment + connector footprint. Avoids the failure
mode where you finish two boards and discover they have incompatible
expectations of which pin carries which signal.
