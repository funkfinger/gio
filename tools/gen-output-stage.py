#!/usr/bin/env python3
"""
v2: Generate the output op-amp stage by COPYING the input op-amp section
text-wise, translating coordinates by +100 in Y, and re-writing references,
UUIDs, values, and net labels.

Output topology vs input topology:
  - Op-amp + bias dividers + power decoupling: IDENTICAL → translate as-is.
  - Input had R_series 100k + BAT54S clamps at jack tip → drop.
  - R_in changes from 22k to 10k.
  - R_fb changes from 22k to 47k.
  - R_iso (1k) repurposed as R_jack_series 1k.
  - New output clamps (BAT54S) placed AFTER R_jack_series, BEFORE jack.
  - New output jacks (J3, J4) placed to the right of clamps.
  - DAC output labels (dac_a, dac_b) replace the input clamp_a/clamp_b nets.
"""

import re
import uuid
import sys

UUID = lambda: str(uuid.uuid4())

SCH = "/Users/jayw/Git/gio/hardware/gio-opamp/gio-opamp.kicad_sch"
PROJECT_UUID = "f6852cf7-9b4a-49f8-8f93-0a6918ef02b7"
PROJECT_NAME = "gio-opamp"

# Y-translation for the common (op-amp + bias + decoupling) section
Y_OFFSET = 100.0

# Mapping of input-stage references to output-stage references
# Output stage uses U5 (input op-amp is U1, U2-U4 are ADC/DAC/MCU)
REF_MAP = {
    "U1": "U5",         # op-amp chip (3 unit instances)
    "R1": "R13",        # ch A R_fb (was 22k → now 47k)
    "R2": "R14",        # ch B R_fb (was 22k → now 47k)
    "R3": "R15",        # ch A R_iso (1k → repurposed as R_jack_series 1k)
    "R4": "R16",        # ch B R_iso (1k → R_jack_series 1k)
    "R5": "R17",        # ch A R_off_top (22k)
    "R6": "R18",        # ch B R_off_top (22k)
    "R7": "R19",        # ch B R_off_bot (14.7k)
    "R8": "R20",        # ch A R_off_bot (14.7k)
    "R10": "R21",       # ch A R_in (was 22k → now 10k)
    "R11": "R22",       # ch B R_in (was 22k → now 10k)
    "C1": "C6",         # bulk -12V decoupling
    "C2": "C7",         # bulk +12V decoupling
    "C3": "C8",         # ch A bias-node filter cap
    "C4": "C9",         # ch B bias-node filter cap
    "C5": "C10",        # local V+/V- decoupling
}

# Power-flag renumbering (input uses #PWR01..#PWR021, we start at #PWR022).
# Empty mapping built at runtime by visiting each power flag we encounter.

# Component value overrides (output topology differs from input)
VALUE_OVERRIDES = {
    "R13": "47k",   # was R1 22k → R_fb output is 47k
    "R14": "47k",   # was R2
    "R21": "10k",   # was R10 22k → R_in output is 10k
    "R22": "10k",   # was R11
    # R15, R16 stay at 1k (output isolation became R_jack_series, same value)
    # R17, R18 stay at 22k
    # R19, R20 stay at 14.7k
    # All caps stay at 100nF
}

# Net labels to rewrite when translating
LABEL_RENAMES = {
    # The input-stage label POSITIONS map to different semantics in the output
    # topology because the signal flow reverses direction:
    #   - In input: jack -> R_series -> [clamp_a node] -> R_in -> op-amp -> R_iso -> [adc_ch1]
    #   - In output: [dac_a] -> R_in -> op-amp -> R_jack_series -> [out_a] -> jack
    # The label that was at R_in's "outer" pin (clamp_a) is now where DAC enters.
    # The label that was at R_iso's "outer" pin (adc_ch1) is now where signal exits to jack.
    "clamp_a": "dac_a",   # at R21 (R_in ch A) right pin = DAC signal input
    "clamp_b": "dac_b",
    "adc_ch1": "out_a",   # at R15 (R_jack_series ch A) right pin = jack output
    "adc_ch2": "out_b",
}

# Components to SKIP entirely (input-protection chain not needed in output stage)
SKIP_REFS = {"R9", "R12", "D1", "D2", "J1", "J2"}


def read_file():
    with open(SCH) as f:
        return f.read()


def parse_top_level_blocks(text):
    """
    Walk through text and yield (kind, start, end, refs) for each top-level
    block (symbol/wire/junction/label/global_label) — where refs is a list
    of reference designators in symbols (or empty for wires).

    A top-level block starts at "\t(symbol|wire|junction|label|global_label" and
    ends at the matching closing paren at the same indentation level.
    """
    i = 0
    while i < len(text):
        # Find next \t( at indentation 1
        m = re.search(r'\n\t\((symbol|wire|junction|label|global_label|sheet_instances|embedded_fonts|paper)\b', text[i:])
        if not m:
            return
        kind = m.group(1)
        start = i + m.start() + 1   # skip the leading \n
        # Walk parens to find end
        depth = 0
        j = start
        in_str = False
        while j < len(text):
            c = text[j]
            if c == '"' and (j == 0 or text[j-1] != '\\'):
                in_str = not in_str
            elif not in_str:
                if c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                    if depth == 0:
                        # End of block
                        end = j + 1
                        block = text[start:end]
                        ref = None
                        ref_match = re.search(r'reference "([^"]+)"', block)
                        if ref_match:
                            ref = ref_match.group(1)
                        yield (kind, start, end, ref, block)
                        i = end
                        break
            j += 1
        else:
            return


def is_in_input_zone(block, kind):
    """
    True if the block's coordinates fall in the input op-amp's "common" zone
    (y in [20, 65]) — meaning we want to translate it for the output stage.
    Excludes the bottom row (y >= 75 = jacks/clamps/R_series).
    """
    if kind == "symbol":
        m = re.search(r'\(at ([-\d.]+) ([-\d.]+) [-\d]+\)', block)
        if not m:
            return False
        x, y = float(m.group(1)), float(m.group(2))
        return 18 <= x <= 110 and 20 <= y <= 65
    if kind == "wire":
        coords = re.findall(r'\(xy ([-\d.]+) ([-\d.]+)\)', block)
        if not coords:
            return False
        # Both endpoints must be in zone
        for x_s, y_s in coords:
            x, y = float(x_s), float(y_s)
            if not (18 <= x <= 115 and 20 <= y <= 70):
                return False
        return True
    if kind == "junction":
        m = re.search(r'\(at ([-\d.]+) ([-\d.]+)\)', block)
        if not m: return False
        x, y = float(m.group(1)), float(m.group(2))
        return 18 <= x <= 115 and 20 <= y <= 70
    if kind == "label":
        m = re.search(r'\(at ([-\d.]+) ([-\d.]+) [-\d]+\)', block)
        if not m: return False
        x, y = float(m.group(1)), float(m.group(2))
        return 18 <= x <= 115 and 20 <= y <= 70
    return False


def translate_block(block, kind, y_offset):
    """Translate a block's coordinates by y_offset and regenerate UUIDs."""
    # Translate Y coordinates of (at X Y ...) and (xy X Y) tokens
    def shift_at(m):
        prefix = m.group(1)
        x = float(m.group(2))
        y = float(m.group(3))
        rest = m.group(4)
        return f"{prefix}{m.group(2)} {y + y_offset:.4f}{rest}"
    block = re.sub(r'(\(at )([-\d.]+) ([-\d.]+)( [-\d]+\))', shift_at, block)

    def shift_at2(m):  # for (at X Y) without rotation (junction)
        x = float(m.group(2))
        y = float(m.group(3))
        return f"{m.group(1)}{m.group(2)} {y + y_offset:.4f})"
    block = re.sub(r'(\(at )([-\d.]+) ([-\d.]+)\)', shift_at2, block)

    def shift_xy(m):
        x = float(m.group(1))
        y = float(m.group(2))
        return f"(xy {m.group(1)} {y + y_offset:.4f})"
    block = re.sub(r'\(xy ([-\d.]+) ([-\d.]+)\)', shift_xy, block)

    # Regenerate ALL UUIDs
    block = re.sub(r'"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"',
                   lambda _: f'"{UUID()}"', block)
    return block


def remap_reference(block, old_ref, new_ref):
    """Replace reference designator references."""
    return block.replace(f'"Reference" "{old_ref}"', f'"Reference" "{new_ref}"') \
                .replace(f'(reference "{old_ref}")', f'(reference "{new_ref}")')


def remap_value(block, new_value):
    """Replace the Value property."""
    return re.sub(r'"Value" "[^"]+"', f'"Value" "{new_value}"', block, count=1)


def rename_label(block, old, new):
    """Rename a label string and any wire-attached label net name."""
    return re.sub(r'\(label "' + re.escape(old) + r'"', f'(label "{new}"', block)


# ----------------------------------------------------------------------
# Build the new schematic content
# ----------------------------------------------------------------------
text = read_file()

new_blocks = []   # list of (kind, block_text)
power_renumber = {}    # old #PWRxx -> new #PWRyy
next_pwr = 22          # existing schematic uses 01..21

# Walk all top-level blocks; for each in input zone, translate and remap
for kind, start, end, ref, block in parse_top_level_blocks(text):
    if not is_in_input_zone(block, kind):
        continue
    # Skip components that don't apply to output stage
    if ref in SKIP_REFS:
        continue
    # Translate coordinates and regenerate UUIDs
    new_block = translate_block(block, kind, Y_OFFSET)

    # Remap reference designator
    if ref in REF_MAP:
        new_ref = REF_MAP[ref]
        new_block = remap_reference(new_block, ref, new_ref)
        # Apply value override if present
        if new_ref in VALUE_OVERRIDES:
            new_block = remap_value(new_block, VALUE_OVERRIDES[new_ref])
    elif ref and ref.startswith("#PWR"):
        # Renumber power flag
        if ref not in power_renumber:
            power_renumber[ref] = f"#PWR0{next_pwr}"
            next_pwr += 1
        new_block = remap_reference(new_block, ref, power_renumber[ref])

    # Rename labels (clamp_a → out_a, etc.)
    if kind == "label":
        for old, new in LABEL_RENAMES.items():
            new_block = rename_label(new_block, old, new)

    new_blocks.append((kind, new_block))

# ----------------------------------------------------------------------
# Add new components for output topology: clamps (D3, D4) and jacks (J3, J4)
# Plus wires to connect op-amp outputs through R_jack_series → clamp → jack.
# Plus #PWR flags for D3/D4 V+/V-.
# ----------------------------------------------------------------------

EXTRAS = []   # raw s-expression strings

# --- D3 BAT54S clamp on ch A output, anchor (115.57, 127.94) rotation 90 ---
# Pin geometry (rotation 90 from earlier verification):
#   pin 3 (COM) at (X+5.08, Y) = (120.65, 127.94)
#   pin 1 (A)   at (X, Y+7.62) = (115.57, 135.56)
#   pin 2 (K)   at (X, Y-7.62) = (115.57, 120.32)

def bat54s_block(ref, x, y, rotation=90):
    su = UUID()
    p1, p2, p3 = UUID(), UUID(), UUID()
    return f"""\t(symbol
\t\t(lib_id "Diode:BAT54S")
\t\t(at {x} {y} {rotation})
\t\t(unit 1)
\t\t(body_style 1)
\t\t(exclude_from_sim no)
\t\t(in_bom yes)
\t\t(on_board yes)
\t\t(in_pos_files yes)
\t\t(dnp no)
\t\t(fields_autoplaced yes)
\t\t(uuid "{su}")
\t\t(property "Reference" "{ref}"
\t\t\t(at {x - 5.08} {y - 1.27} 0)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Value" "BAT54S"
\t\t\t(at {x - 5.08} {y + 1.27} 0)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Footprint" ""
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Datasheet" ""
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Description" "200mA dual Schottky series, SOT-23"
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(pin "1" (uuid "{p1}"))
\t\t(pin "2" (uuid "{p2}"))
\t\t(pin "3" (uuid "{p3}"))
\t\t(instances
\t\t\t(project "{PROJECT_NAME}"
\t\t\t\t(path "/{PROJECT_UUID}"
\t\t\t\t\t(reference "{ref}")
\t\t\t\t\t(unit 1)
\t\t\t\t)
\t\t\t)
\t\t)
\t)"""


def jack_block(ref, x, y, rotation=180):
    """PJ3001F at rotation 180: T at (X-5.08, Y), S at (X-5.08, Y+2.54), TN at (X-5.08, Y-2.54)."""
    su = UUID()
    pT, pS, pTN = UUID(), UUID(), UUID()
    return f"""\t(symbol
\t\t(lib_id "benjiaomodular:PJ3001F")
\t\t(at {x} {y} {rotation})
\t\t(unit 1)
\t\t(body_style 1)
\t\t(exclude_from_sim no)
\t\t(in_bom yes)
\t\t(on_board yes)
\t\t(in_pos_files yes)
\t\t(dnp no)
\t\t(fields_autoplaced yes)
\t\t(uuid "{su}")
\t\t(property "Reference" "{ref}"
\t\t\t(at {x + 5.08} {y - 2.54} 0)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Value" "PJ3001F"
\t\t\t(at {x + 5.08} {y} 0)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Footprint" ""
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Datasheet" ""
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Description" ""
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(pin "S" (uuid "{pS}"))
\t\t(pin "T" (uuid "{pT}"))
\t\t(pin "TN" (uuid "{pTN}"))
\t\t(instances
\t\t\t(project "{PROJECT_NAME}"
\t\t\t\t(path "/{PROJECT_UUID}"
\t\t\t\t\t(reference "{ref}")
\t\t\t\t\t(unit 1)
\t\t\t\t)
\t\t\t)
\t\t)
\t)"""


def power_block(ref, lib_id, x, y, rotation=0):
    su = UUID()
    pn = UUID()
    name = lib_id.split(":")[-1]
    return f"""\t(symbol
\t\t(lib_id "{lib_id}")
\t\t(at {x} {y} {rotation})
\t\t(unit 1)
\t\t(body_style 1)
\t\t(exclude_from_sim no)
\t\t(in_bom yes)
\t\t(on_board yes)
\t\t(in_pos_files yes)
\t\t(dnp no)
\t\t(fields_autoplaced yes)
\t\t(uuid "{su}")
\t\t(property "Reference" "{ref}"
\t\t\t(at {x} {y - 3.81} 0)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(hide yes)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Value" "{name}"
\t\t\t(at {x} {y - 3.81} 0)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Footprint" ""
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Datasheet" ""
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Description" "Power symbol"
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(show_name no)
\t\t\t(do_not_autoplace no)
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(pin "1" (uuid "{pn}"))
\t\t(instances
\t\t\t(project "{PROJECT_NAME}"
\t\t\t\t(path "/{PROJECT_UUID}"
\t\t\t\t\t(reference "{ref}")
\t\t\t\t\t(unit 1)
\t\t\t\t)
\t\t\t)
\t\t)
\t)"""


def wire_block(x1, y1, x2, y2):
    return f"""\t(wire
\t\t(pts
\t\t\t(xy {x1} {y1}) (xy {x2} {y2})
\t\t)
\t\t(stroke
\t\t\t(width 0)
\t\t\t(type default)
\t\t)
\t\t(uuid "{UUID()}")
\t)"""


def label_block(text, x, y, rotation=0, justify="left bottom"):
    return f"""\t(label "{text}"
\t\t(at {x} {y} {rotation})
\t\t(effects
\t\t\t(font
\t\t\t\t(size 1.27 1.27)
\t\t\t)
\t\t\t(justify {justify})
\t\t)
\t\t(uuid "{UUID()}")
\t)"""


# Output clamps
EXTRAS.append(bat54s_block("D3", 115.57, 127.94, 90))
EXTRAS.append(bat54s_block("D4", 115.57, 148.26, 90))

# Output jacks
EXTRAS.append(jack_block("J3", 130.81, 127.94, 180))
EXTRAS.append(jack_block("J4", 130.81, 148.26, 180))

# Power flags for clamps & jacks
EXTRAS.append(power_block(f"#PWR0{next_pwr}", "power:+12V", 115.57, 116.51, 0))
next_pwr += 1
EXTRAS.append(power_block(f"#PWR0{next_pwr}", "power:-12V", 115.57, 139.37, 180))
next_pwr += 1
EXTRAS.append(power_block(f"#PWR0{next_pwr}", "power:+12V", 115.57, 136.83, 0))
next_pwr += 1
EXTRAS.append(power_block(f"#PWR0{next_pwr}", "power:-12V", 115.57, 159.69, 180))
next_pwr += 1
EXTRAS.append(power_block(f"#PWR0{next_pwr}", "power:GND", 125.73, 132.08, 0))   # J3.S sleeve
next_pwr += 1
EXTRAS.append(power_block(f"#PWR0{next_pwr}", "power:GND", 125.73, 152.40, 0))   # J4.S sleeve
next_pwr += 1

# Wires for the output topology bottom row
# Channel A:
# R15 (R_jack_series) right pin (106.68, 127.94) → D3 pin 3 COM (120.65, 127.94)
EXTRAS.append(wire_block(106.68, 127.94, 120.65, 127.94))
# D3 pin 3 COM (120.65, 127.94) → J3.T (125.73, 127.94)
EXTRAS.append(wire_block(120.65, 127.94, 125.73, 127.94))
# D3 pin 2 K (115.57, 120.32) → +12V flag (115.57, 116.51)
EXTRAS.append(wire_block(115.57, 120.32, 115.57, 116.51))
# D3 pin 1 A (115.57, 135.56) → -12V flag (115.57, 139.37)
EXTRAS.append(wire_block(115.57, 135.56, 115.57, 139.37))
# J3.S (125.73, 130.48) → GND flag (125.73, 132.08)
EXTRAS.append(wire_block(125.73, 130.48, 125.73, 132.08))

# Channel B (mirror):
EXTRAS.append(wire_block(106.68, 148.26, 120.65, 148.26))
EXTRAS.append(wire_block(120.65, 148.26, 125.73, 148.26))
EXTRAS.append(wire_block(115.57, 140.64, 115.57, 136.83))
EXTRAS.append(wire_block(115.57, 155.88, 115.57, 159.69))
EXTRAS.append(wire_block(125.73, 150.80, 125.73, 152.40))

# Add EXTRAS to new_blocks
for blk in EXTRAS:
    new_blocks.append(("extra", blk))

# ----------------------------------------------------------------------
# Insert all new content before the (sheet_instances marker
# ----------------------------------------------------------------------
marker = "\n\t(sheet_instances"
idx = text.find(marker)
if idx == -1:
    print("ERROR: could not find sheet_instances marker", file=sys.stderr)
    sys.exit(1)

new_content = "\n".join(b for _, b in new_blocks)
output = text[:idx] + "\n" + new_content + text[idx:]

with open(SCH, "w") as f:
    f.write(output)

# Sanity stats
op = output.count("(")
cp = output.count(")")
print(f"Translated {len(new_blocks) - len(EXTRAS)} input-stage blocks; added {len(EXTRAS)} new blocks")
print(f"Power flag renumbering: {len(power_renumber)} flags renamed (next ref = #PWR0{next_pwr})")
print(f"Paren balance: {op} open, {cp} close, diff = {op - cp}")
