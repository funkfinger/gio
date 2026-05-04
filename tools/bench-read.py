#!/usr/bin/env python3
"""
Read the gio smoke-test firmware serial stream and summarize channels.

Usage:
    python3 tools/bench-read.py [--seconds N] [--port PATH] [--rows N]

Defaults:
    --seconds 4   Read window in seconds.
    --port    auto-detect /dev/cu.usbmodem*
    --rows    5   Sample rows to print at the end.

Auto-discovers column names by looking for the smoke test's '# t_ms ...'
header line. Falls back to "col0, col1, ..." if no header is seen.

Examples:
    # Quick 4s capture, all columns summarized:
    python3 tools/bench-read.py

    # Longer capture for a slow rotation test:
    python3 tools/bench-read.py --seconds 10

    # Specific port (e.g. multiple XIAOs):
    python3 tools/bench-read.py --port /dev/cu.usbmodem20402
"""

import argparse
import glob
import sys
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial not installed. Run: pip3 install pyserial", file=sys.stderr)
    sys.exit(1)


def find_port():
    """Return the first /dev/cu.usbmodem* device, or None."""
    candidates = sorted(glob.glob("/dev/cu.usbmodem*"))
    return candidates[0] if candidates else None


def read_window(port, baud, seconds):
    """Open the port, capture data for `seconds`, return (header_cols, rows)."""
    s = serial.Serial(port, baud, timeout=0.5)
    time.sleep(0.3)
    s.reset_input_buffer()

    header_cols = None
    rows = []
    end = time.time() + seconds
    while time.time() < end:
        line = s.readline()
        if not line:
            continue
        decoded = line.decode(errors="replace").rstrip()
        if not decoded:
            continue
        if decoded.startswith("#"):
            # Header line: "# t_ms  dac_a_v  ..." → ["t_ms", "dac_a_v", ...]
            cols = decoded.lstrip("#").split()
            if cols:
                header_cols = cols
            continue
        parts = decoded.split("\t")
        # Try to parse as numeric row
        try:
            row = [float(p) for p in parts]
            rows.append(row)
        except ValueError:
            # Non-numeric line (boot message, etc.) — skip silently
            pass

    return header_cols, rows


# Fallback column names for the gio smoke-test firmware. Used when the
# header line isn't seen in the capture window (the firmware prints it
# only once at boot). Update if main_smoketest.cpp's header changes.
SMOKETEST_COLS = [
    "t_ms", "dac_a_v", "dac_b_v",
    "pot_v", "pot_count",
    "j1_v",  "j1_count",
    "j2_v",  "j2_count",
    "enc_count", "click", "long",
]


def summarize(header_cols, rows):
    if not rows:
        print("No numeric rows captured.")
        return

    n_cols = len(rows[0])
    if header_cols is None or len(header_cols) != n_cols:
        if n_cols == len(SMOKETEST_COLS):
            header_cols = SMOKETEST_COLS
        else:
            header_cols = [f"col{i}" for i in range(n_cols)]

    print(f"rows: {len(rows)}")
    print()
    name_w = max(len(c) for c in header_cols)
    print(f"  {'column':<{name_w}}  {'min':>10}  {'max':>10}  {'range':>10}  {'last':>10}")
    print(f"  {'-' * name_w}  {'-' * 10}  {'-' * 10}  {'-' * 10}  {'-' * 10}")
    for i, name in enumerate(header_cols):
        col = [r[i] for r in rows if i < len(r)]
        if not col:
            continue
        lo, hi = min(col), max(col)
        last = col[-1]
        print(f"  {name:<{name_w}}  {lo:>10.4f}  {hi:>10.4f}  {hi - lo:>10.4f}  {last:>10.4f}")


def print_samples(header_cols, rows, n_rows):
    if not rows or n_rows <= 0:
        return
    print()
    print(f"sample rows (last {n_rows}):")
    n_cols = len(rows[0])
    if header_cols is None or len(header_cols) != n_cols:
        if n_cols == len(SMOKETEST_COLS):
            header_cols = SMOKETEST_COLS
        else:
            header_cols = [f"col{i}" for i in range(n_cols)]
    print("  " + "\t".join(header_cols))
    for row in rows[-n_rows:]:
        print("  " + "\t".join(f"{v:.4f}" for v in row))


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--seconds", type=float, default=4.0, help="capture window (default 4)")
    p.add_argument("--port", default=None, help="serial port (default: auto)")
    p.add_argument("--baud", type=int, default=115200, help="baud rate (default 115200)")
    p.add_argument("--rows", type=int, default=5, help="sample rows to print (default 5; 0 to skip)")
    args = p.parse_args()

    port = args.port or find_port()
    if not port:
        print("ERROR: no /dev/cu.usbmodem* found. Pass --port explicitly.", file=sys.stderr)
        sys.exit(1)
    print(f"port: {port}  baud: {args.baud}  window: {args.seconds}s")

    try:
        header_cols, rows = read_window(port, args.baud, args.seconds)
    except serial.SerialException as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    summarize(header_cols, rows)
    print_samples(header_cols, rows, args.rows)


if __name__ == "__main__":
    main()
