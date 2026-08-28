#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
uart_ram_boot.py - run an image from RAM on the HH71VM Realtek side, writing nothing
to flash.

This is the module the rollback tool uses to bring our own system up in RAM. For the
stand-alone RAM boot procedure described in the documentation, use tools/ram_boot.py
instead.

WHAT IT DOES

The `<RealTek>` ROM bootloader checks the name of the file it receives over TFTP
BEFORE deciding what to do with it: if the name contains the substring `nfjrom` (or
is exactly `boot.img`), it does NOT call `burn_image` but prints `Jump to 0x...` and
hands control to the received data as code (`jalr $t9`, 0x8000216C). The name check
happens BEFORE the `AUTOBURN` check.

Hence the whole sequence: `ETH` -> `LOADADDR` -> `AUTOBURN 0` -> a TFTP upload of a
file with `nfjrom` in its name -> reading the console while the kernel boots. Flash
is not touched at all; removing power returns the device to what is installed.

SAFETY CATCHES (inverted with respect to rtk_romloader.py)

In the normal mode (`rtk_romloader.py tftp`) the dangerous names are the ones with
`nfjrom`: they mean execution instead of writing, and the uploader refuses them. Here
it is the other way round - a name WITHOUT `nfjrom` is dangerous, because then the
bootloader treats the file as an image to be flashed. So this script:
  - refuses to run when the name has no `nfjrom` in it;
  - sets `AUTOBURN 0` anyway, as a second independent catch in case the name is
    somehow not recognised;
  - cannot send a single flash-write command (`FLW`/`ERASECHIP` are refused by
    `RomLoader` itself).

ABOUT THE LOAD ADDRESS

The default `0x84000000` is exactly the address our lzma-loader is linked at
(`LZMA_TEXT_START` in target/linux/rtkmipsel/image/Makefile). With a different
LOADADDR the jump lands in the wrong place. The address is cacheable (KSEG0) and that
is safe: before `jalr` the bootloader flushes the caches completely - `cache 1` (Index
Writeback Inv D) over all 32 KB of the D-cache and an I-cache invalidate
(0x80009D80/0x80009D50, confirmed by disassembly).

FIREWALL REQUIREMENT

As with the other network scripts here: inbound UDP from an arbitrary TID is only
covered by a host firewall rule for one specific interpreter path.

The device must already be sitting at the `<RealTek>` prompt (power applied with WPS
held).

EXAMPLES
    python uart_ram_boot.py openwrt-...-nfjrom.bin
    python uart_ram_boot.py image.bin --uart-seconds 120 --loadaddr 0x84000000
    python uart_ram_boot.py --listen-only --uart-seconds 60      # just watch the console
"""

import argparse
import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rtk_romloader
import rtk_tftp_put

DEFAULT_LOADADDR = 0x84000000
REQUIRED_NAME_SUBSTR = "nfjrom"

LOGDIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ram-boot-logs")


def check_name_executes(name):
    """The inverted safety catch: without `nfjrom` in the name the bootloader will
    treat the file as an image for flash rather than as code. For this script such a
    name is an error."""
    if REQUIRED_NAME_SUBSTR not in name.lower():
        raise SystemExit(
            "REFUSED: the name %r does not contain %r.\n"
            "The bootloader recognises the 'run from RAM' mode BY THE FILE NAME. "
            "Without it the file is taken as an image to be written to flash - and "
            "this script is meant for exactly the opposite. Rename the file."
            % (name, REQUIRED_NAME_SUBSTR))


class Tee:
    """Write both to stdout (to watch the boot live) and to a log file."""

    def __init__(self, path):
        self.fh = open(path, "a", encoding="utf-8", newline="")
        self.path = path

    def write(self, text):
        # An early-boot console happily emits junk bytes (wrong bit rate, fragments).
        # Dying with UnicodeEncodeError in the middle of a capture is not acceptable -
        # that output is exactly what we came to collect.
        enc = sys.stdout.encoding or "ascii"
        sys.stdout.write(text.encode(enc, "replace").decode(enc, "replace"))
        sys.stdout.flush()
        self.fh.write(text)
        self.fh.flush()

    def note(self, text):
        self.write("\n*** %s\n" % text)

    def close(self):
        self.fh.close()


def catch_prompt_esc(rl, tee, seconds):
    """Catch `<RealTek>` on a cold start by flooding the UART with ESC (0x1B).

    WHY THIS EXISTS. The only known way into the bootloader used to be holding WPS
    while applying power. But on the HH71VM that same button puts the SECOND chip,
    the Qualcomm MDM, into the `900E` diagnostic mode - its normal firmware does not
    start and the Realtek-to-modem USB link does not exist. For anything involving
    the modem, WPS must be left alone.

    Disassembling the bootloader (function 0x8000C030) showed the intended
    alternative: during the autoboot window it waits for the character **0x1B (ESC)**
    and, on receiving it, prints `---Escape booting by user---` and drops into
    command mode. No button is needed at all.

    The window is short, so ESC is sent continuously: the script is started BEFORE
    power is applied and keeps spamming until it sees the prompt.
    """
    tee.note("catching the bootloader with ESC - apply power, do NOT hold WPS (%.0f s)" % seconds)
    deadline = time.time() + seconds
    seen = ""
    # The normal port timeout is one second; with it read() would block the loop and
    # ESC would go out once a second, while the autoboot window is shorter. Use a
    # non-blocking read while catching, then restore.
    saved_timeout = rl.ser.timeout
    rl.ser.timeout = 0
    try:
        while time.time() < deadline:
            rl.ser.write(b"\x1b")
            chunk = rl.ser.read(4096)
            if chunk:
                text = chunk.decode("latin-1")
                tee.write(text)
                seen += text
                if "<RealTek>" in seen:
                    tee.note("prompt caught without WPS")
                    rl.ser.timeout = saved_timeout
                    # collect the rest of the banner and let the console settle
                    rl.read_quiet(quiet_ms=300, max_wait_s=3.0)
                    return True
            time.sleep(0.005)
    finally:
        rl.ser.timeout = saved_timeout
    tee.note("ESC did not work within the time given - no prompt")
    return False


def stream_uart(rl, tee, seconds, stop_when_quiet=None):
    """Read the console for `seconds` seconds, printing everything as it arrives.
    With `stop_when_quiet` set, return early after that many seconds of silence."""
    deadline = time.time() + seconds
    last = time.time()
    while time.time() < deadline:
        chunk = rl.read_quiet(quiet_ms=200, max_wait_s=1.0)
        if chunk:
            tee.write(chunk.decode("latin-1"))
            last = time.time()
        elif stop_when_quiet and (time.time() - last) >= stop_when_quiet:
            tee.note("the line has been quiet for %.0f s - stopping" % stop_when_quiet)
            return


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", nargs="?", help="image file (the name must contain nfjrom)")
    ap.add_argument("--port", default=rtk_romloader.PORT_DEFAULT)
    ap.add_argument("--baud", type=int, default=rtk_romloader.BAUD_DEFAULT)
    ap.add_argument("--host", default=rtk_tftp_put.DEFAULT_HOST,
                    help="address of the bootloader console (ETH sets it unconditionally)")
    ap.add_argument("--name", default=None,
                    help="file name for TFTP (default: the basename of the image)")
    ap.add_argument("--loadaddr", default=hex(DEFAULT_LOADADDR),
                    help="receive address and entry point, hex (default 0x84000000)")
    ap.add_argument("--skip-eth", action="store_true",
                    help="do not bring the network up again (if ETH already ran this power cycle)")
    ap.add_argument("--uart-seconds", type=float, default=90.0,
                    help="how many seconds to read the console after the transfer")
    ap.add_argument("--listen-only", action="store_true",
                    help="send nothing, only watch the console")
    ap.add_argument("--catch-esc", type=float, default=0.0, metavar="SEC",
                    help="before anything else, catch <RealTek> by spamming ESC for "
                         "this many seconds; apply power WITHOUT holding WPS (needed "
                         "when the Qualcomm side has to stay in its normal mode)")
    args = ap.parse_args()

    os.makedirs(LOGDIR, exist_ok=True)
    tee = Tee(os.path.join(LOGDIR, "ramboot-%s.log" % time.strftime("%Y%m%d-%H%M%S")))
    tee.note("log: %s" % tee.path)

    if args.listen_only:
        with rtk_romloader.RomLoader(port=args.port, baud=args.baud) as rl:
            if args.catch_esc:
                catch_prompt_esc(rl, tee, args.catch_esc)
            tee.note("listen-only mode, %.0f s" % args.uart_seconds)
            stream_uart(rl, tee, args.uart_seconds)
        tee.close()
        return

    if not args.image:
        raise SystemExit("a path to the image is required (or --listen-only)")

    remote_name = args.name or os.path.basename(args.image)
    check_name_executes(remote_name)

    with open(args.image, "rb") as f:
        data = f.read()

    loadaddr = int(args.loadaddr, 16)
    tee.note("image %s - %d bytes" % (args.image, len(data)))
    tee.note("LOADADDR 0x%08X, TFTP name %r, host %s" % (loadaddr, remote_name, args.host))
    tee.note("flash is NOT touched: an nfjrom name disables burn_image, plus AUTOBURN 0")

    with rtk_romloader.RomLoader(port=args.port, baud=args.baud) as rl:
        if args.catch_esc and not catch_prompt_esc(rl, tee, args.catch_esc):
            tee.close()
            raise SystemExit("could not reach <RealTek> with ESC - try again")

        tee.write(rl.send_raw(""))          # prompt, and a link check at the same time

        if not args.skip_eth:
            tee.note("ETH - bringing the bootloader network up")
            tee.write(rl.cmd_eth())

        tee.note("LOADADDR")
        tee.write(rl.cmd_loadaddr(loadaddr))
        tee.note("AUTOBURN 0 (safety catch: writing to flash is off)")
        tee.write(rl.cmd_autoburn(0))

        result = {}

        def _run_tftp():
            try:
                result["stats"] = rtk_tftp_put.put(
                    data, host=args.host, remote_name=remote_name, name_override=True)
            except Exception as exc:            # noqa: BLE001 - any upload failure counts
                result["error"] = exc

        tee.note("TFTP upload starting; the console should then show 'Jump to 0x%X'" % loadaddr)
        th = threading.Thread(target=_run_tftp, name="tftp-put", daemon=True)
        th.start()

        while th.is_alive():
            chunk = rl.read_quiet(quiet_ms=200, max_wait_s=1.0)
            if chunk:
                tee.write(chunk.decode("latin-1"))
        th.join(timeout=5.0)
        if th.is_alive():
            tee.note("the TFTP thread did not finish 5 s after join() - the transfer is still running")
            tee.close()
            raise SystemExit(1)

        if "error" in result:
            tee.note("TFTP FAILED: %s" % result["error"])
            stream_uart(rl, tee, 10.0)
            tee.close()
            raise SystemExit(1)

        st = result["stats"]
        tee.note("TFTP: %d bytes in %.1f s (%.0f KB/s), server TID %s, retransmits %s"
                 % (st["bytes"], st["seconds"], st["kbps"], st["server_tid"], st["retransmits"]))
        tee.note("reading the console for %.0f s" % args.uart_seconds)
        stream_uart(rl, tee, args.uart_seconds)

    tee.note("log saved: %s" % tee.path)
    tee.close()


if __name__ == "__main__":
    main()
