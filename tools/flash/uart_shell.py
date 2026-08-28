#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
uart_shell.py - run a list of commands in the device UART shell and collect the output.

Used to read facts out of a system started by the RAM loader, which has neither
network nor SSH: the console is the only channel. Typing into a terminal by hand is
awkward, and `screen`/`putty` give no machine-readable result.

How it works: one line `echo BEGIN; <command>; echo END` is sent, and the result is
whatever sits between the marker lines IN THE OUTPUT. The line that was sent comes
back as well (console echo) and contains both markers too, so the LAST occurrence of
`BEGIN` is used, not the first.

Why not something simpler: the OpenWrt prompt changes during boot, and busybox ash
mixes line-editing escape sequences into the echo and wraps long commands at the
terminal width. Parsing "by prompt" or "everything up to the marker" breaks on that
and silently loses short answers.

Safety: this is an ordinary shell and the script imposes no restrictions - but by
default it writes nothing either, it only runs what it was given. Commands come from
the arguments or from a file (`--file`, one per line).

EXAMPLES
    python uart_shell.py "cat /proc/cpuinfo" "cat /proc/mtd"
    python uart_shell.py --file probe.txt --out facts.txt
    python uart_shell.py --wake        # only wake the console up (Enter)
"""

import argparse
import os
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

PORT_DEFAULT = "COM8"
BAUD_DEFAULT = 38400
BEGIN = "___RTK_B%d___"
END = "___RTK_E%d___"


def read_until(ser, needle, timeout):
    """Accumulate output until `needle` is seen TWICE (the first occurrence is the
    echo of the line we sent, the second is the real output) or time runs out."""
    buf = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = ser.read(4096)
        if chunk:
            buf += chunk
            if buf.count(needle.encode()) >= 2:
                break
        else:
            time.sleep(0.05)
    return buf.decode("latin-1")


def run(ser, cmd, timeout, seq):
    begin, end = BEGIN % seq, END % seq
    line = "echo %s; %s; echo %s\n" % (begin, cmd, end)
    try:
        payload = line.encode("latin-1")
    except UnicodeEncodeError:
        # The device console is eight-bit and cannot carry characters outside
        # latin-1. Saying so plainly beats a traceback in the middle of a run.
        bad = sorted({ch for ch in cmd if ord(ch) > 255})
        raise SystemExit(
            "the command contains characters that do not exist in latin-1: %s\n"
            "  %s\n"
            "The device console will not accept them - use ASCII." % ("".join(bad), cmd))
    ser.reset_input_buffer()
    ser.write(payload)
    ser.flush()
    out = read_until(ser, end, timeout).replace("\r", "")

    lines = out.split("\n")
    # The last line consisting of exactly the begin marker is already the output of
    # `echo`, not the echo of the command we sent (there the marker shares the line
    # with the command text).
    starts = [i for i, ln in enumerate(lines) if ln.strip() == begin]
    if not starts:
        return "[begin marker not found; raw output]\n" + out.strip("\n")
    body = lines[starts[-1] + 1:]
    ends = [i for i, ln in enumerate(body) if ln.strip() == end]
    if ends:
        body = body[:ends[0]]
    return "\n".join(body).strip("\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("commands", nargs="*")
    ap.add_argument("--port", default=PORT_DEFAULT)
    ap.add_argument("--baud", type=int, default=BAUD_DEFAULT)
    ap.add_argument("--file", help="file listing commands, one per line")
    ap.add_argument("--out", help="where to save the collected report")
    ap.add_argument("--timeout", type=float, default=10.0, help="seconds per command")
    ap.add_argument("--wake", action="store_true",
                    help="send Enter to activate the OpenWrt console")
    args = ap.parse_args()

    cmds = list(args.commands)
    if args.file:
        with open(args.file, encoding="utf-8") as f:
            cmds += [ln.strip() for ln in f if ln.strip() and not ln.startswith("#")]

    ser = serial.Serial(args.port, args.baud, timeout=0.3)
    report = []
    try:
        if args.wake or cmds:
            ser.write(b"\n")
            ser.flush()
            time.sleep(0.6)
            ser.reset_input_buffer()

        for i, cmd in enumerate(cmds):
            out = run(ser, cmd, args.timeout, i)
            block = "$ %s\n%s" % (cmd, out)
            report.append(block)
            print(block)
            print("-" * 70)
    finally:
        ser.close()

    if args.out:
        with open(args.out, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n\n".join(report) + "\n")
        print("report: %s" % os.path.abspath(args.out))


if __name__ == "__main__":
    main()
