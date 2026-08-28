#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
restore_stock.py - put the Realtek side of the HH71VM back on stock firmware, over
the UART.

If you have no UART adapter, use restore_stock_lan.py instead: it does the same job
through the button-and-network path.

TWO PATHS, ONE SCRIPT

    --via ramboot     (the default) Our OpenWrt is started from RAM, the stock dumps
                      are downloaded onto the device over HTTP and laid out into the
                      partitions with `mtd write`. This path has been used on a live
                      device end to end, by this very script: about 200 s of writing,
                      and the readback md5 matched on all three partitions.

    --via bootloader  The `<RealTek>` bootloader console (UART + TFTP + AUTOBURN),
                      with ready `rtk_mkimg.py build` images in `--image`. Needed
                      when the device does not boot at all - it is the only safety
                      net with no working system. The mechanism itself (transfer,
                      write, readback) has been proven live on full-size images of
                      several megabytes, but with images of our OpenWrt, through
                      `flash_openwrt_tftp.py`. Restoring the stock partitions by
                      exactly this path has not been done.

When nothing else helps, the hardware safety net is a full-chip image written with an
external SPI programmer.

WHAT IS RESTORED (ramboot mode)

The stock image is laid out over our partitions so that every byte lands on the
absolute flash address it originally came from:

    dump mtd0, 4K blocks 48..767      -> kernel partition       0x030000..0x2FFFFF
    dump mtd1, 4K blocks 0..1023      -> rootfs partition       0x300000..0x6FFFFF
    dump mtd1, 4K blocks 1024..2303   -> rootfs_data partition  0x700000..0xBFFFFF

Nothing else is touched: the bootloader (0x000000), `hwsetting` with the MAC
addresses (0x020000), the factory and current config (0x024000) and `vendor_jffs2`
(0xC00000) - so the stock settings survive the rollback.

SAFETY CATCHES
    - The areas 0x000000-0x01FFFF (bootloader) and 0x020000-0x023FFF (hwsetting,
      MAC) are forbidden outright; the check happens BEFORE connecting to the port
      and no flag turns it off.
    - The ramboot mode refuses to write if any flash partition is mounted on the
      device: that would mean the system runs from flash rather than from RAM, and
      writing into its own partitions would bring it down on the spot.
    - The md5 of every dump is checked on the device BEFORE the first write, and the
      md5 of every partition by reading it back AFTER the write, both against values
      computed on the computer from the same files.
    - `--dry-run` goes as far as the md5 check and stops without writing a byte.
    - The plan is printed before connecting to the port, and a real write asks for a
      confirmation (unless `--yes`).

REQUIREMENTS
    pip install pyserial; run the interpreter the host firewall rules were made for
    (inbound UDP for TFTP and inbound TCP for the temporary HTTP server). Any other
    program using the same serial port has to be closed.
    The sibling modules rtk_mkimg.py, rtk_romloader.py, rtk_tftp_put.py,
    uart_shell.py and uart_ram_boot.py have to be in this directory.

EXAMPLES
    python restore_stock.py --dry-run
    python restore_stock.py --yes
    python restore_stock.py --via bootloader --image stock-rootfs-r6cr.img --dry-run
"""

import argparse
import hashlib
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uart_ram_boot as ram_boot  # noqa: E402
import rtk_romloader  # noqa: E402
import _common  # noqa: E402

try:
    import serial
except ImportError:
    serial = None

HERE = os.path.dirname(os.path.abspath(__file__))
DUMPS = os.path.normpath(os.path.join(
    HERE, "backup-stock"))
DEFAULT_DUMP0 = os.path.join(DUMPS,
                             "mtd0-boot_cfg_linux.bin")
DEFAULT_DUMP1 = os.path.join(DUMPS,
                             "mtd1-rootfs.bin")
DEFAULT_RAMBOOT_IMAGE = os.path.normpath(os.path.join(
    HERE, "..", "..", "firmware",
    "openwrt-rtkmipsel-rtl8197f-hh71vm-nfjrom.bin"))

BLOCK = 4096

# Partition -> (which dump, offset in blocks, length in blocks, absolute address).
# The absolute addresses come from our partition layout, and they are also where
# these data originally sat in stock flash, so laying them out byte for byte gives
# stock back.
RESTORE_MAP = [
    dict(part="kernel",      dump="dump0", skip=48,   count=720,  addr=0x030000),
    dict(part="rootfs",      dump="dump1", skip=0,    count=1024, addr=0x300000),
    dict(part="rootfs_data", dump="dump1", skip=1024, count=1280, addr=0x700000),
]

# The script for the device is deliberately kept in ASCII: its output comes back over
# an eight-bit console and is parsed as latin-1. The first run backgrounds itself -
# appending `&` on the computer side is not possible, because uart_shell adds
# `; echo <marker>` and `& ; ` is a syntax error for ash.
DEVICE_SCRIPT = r"""#!/bin/sh
# Generated by restore_stock.py. Runs on a device booted from RAM.
BASE='@BASE@'
MTDLOG=/tmp/restore.mtd.log

if [ "$1" != "child" ]; then
    rm -f /tmp/restore.done /tmp/restore.log $MTDLOG
    sh "$0" child > /tmp/restore.log 2>&1 &
    echo "restore started in background"
    exit 0
fi

# While mtd write is running the console hits input overrun and loses characters of
# anything typed into it, so the device must not be polled at that time. The device
# therefore reports the end of the work itself, with one line to the console.
signal() {
    echo "@@RESTORE_EXIT=$1@@" > /dev/console
}

fail() {
    echo "FAIL: $*"
    echo 1 > /tmp/restore.done
    signal 1
    exit 1
}

part_num() {
    grep "\"$1\"" /proc/mtd | sed -n 's/^mtd\([0-9][0-9]*\):.*/\1/p'
}

part_size() {
    grep "\"$1\"" /proc/mtd | sed -n 's/^mtd[0-9][0-9]*: *\([0-9a-f][0-9a-f]*\).*/\1/p'
}

echo "=== guard: must run from RAM, not from flash ==="
if grep -q '^/dev/mtdblock' /proc/mounts; then
    fail "a flash partition is mounted: system runs from flash, not from RAM"
fi
if grep -q '^/dev/root' /proc/mounts; then
    fail "/dev/root is mounted: system runs from flash, not from RAM"
fi
echo "no flash partition mounted, continuing"

echo "=== guard: partition table ==="
@GUARDS@

echo "=== download ==="
@DOWNLOADS@
ls -l @LOCALFILES@

echo "=== verify md5 of sources ==="
@SRCSUMS@
echo "sources ok"

if [ "@DRYRUN@" = "1" ]; then
    echo "DRY RUN: stopping before any write"
    echo 0 > /tmp/restore.done
    signal 0
    exit 0
fi

@WRITES@

echo "=== readback md5 of partitions ==="
@READBACKS@

echo "RESTORE_OK"
echo 0 > /tmp/restore.done
signal 0
"""


def md5_file_slice(path, skip_blocks, count_blocks):
    h = hashlib.md5()
    with open(path, "rb") as f:
        f.seek(skip_blocks * BLOCK)
        left = count_blocks * BLOCK
        while left:
            chunk = f.read(min(1 << 20, left))
            if not chunk:
                raise _common.SafetyError(
                    "%s: the file is shorter than the slice skip=%d count=%d "
                    "blocks of %d bytes needs" % (path, skip_blocks, count_blocks, BLOCK))
            h.update(chunk)
            left -= len(chunk)
    return h.hexdigest()


def md5_file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def build_restore_plan(dump0, dump1):
    """Compute everything that can be computed on the computer: the slices, their
    md5 and the absolute flash ranges. Every check that can be done without the
    device is done here, before connecting to the port."""
    files = {"dump0": dump0, "dump1": dump1}
    for key, path in files.items():
        if not os.path.isfile(path):
            raise _common.SafetyError("no dump file %s: %s" % (key, path))
    plan = []
    for item in RESTORE_MAP:
        path = files[item["dump"]]
        length = item["count"] * BLOCK
        lo = item["addr"]
        hi = lo + length - 1
        _common.check_not_forbidden(lo, hi, "partition %s: " % item["part"])
        plan.append(dict(item,
                         path=path,
                         name=os.path.basename(path),
                         length=length,
                         lo=lo, hi=hi,
                         md5=md5_file_slice(path, item["skip"], item["count"])))
    return files, plan


def print_restore_plan(files, plan, dry_run):
    _common.print_header("PLAN FOR ROLLING BACK TO STOCK THROUGH A SYSTEM IN RAM"
                         + (" (DRY RUN)" if dry_run else ""))
    for key in sorted(files):
        print("%s: %s" % (key, files[key]))
        print("   %d bytes, MD5 %s" % (os.path.getsize(files[key]), md5_file(files[key])))
    print()
    for i, item in enumerate(plan, 1):
        print("%d. %s -> partition %s" % (i, item["name"], item["part"]))
        print("   slice: 4K blocks %d..%d (%d bytes)"
              % (item["skip"], item["skip"] + item["count"] - 1, item["length"]))
        print("   absolute flash address: 0x%06X..0x%06X" % (item["lo"], item["hi"]))
        print("   expected MD5 after the write: %s" % item["md5"])
    print()
    print("Not touched: boot (0x000000), hwsetting with the MAC addresses "
          "(0x020000), config (0x024000) and vendor_jffs2 (0xC00000) - the stock "
          "settings stay where they are.")
    if dry_run:
        print("DRY RUN: this goes as far as checking the md5 of the downloaded "
              "dumps and stops without writing a byte.")
    else:
        print("The write is REAL: three partitions will be erased and rewritten, "
              "which takes about 200 s.")


def render_device_script(base_url, plan, dry_run):
    guards, downloads, srcsums, writes, readbacks = [], [], [], [], []
    local = {}
    for item in plan:
        local[item["path"]] = "/tmp/%s" % item["name"]

    for path, tmp in sorted(local.items()):
        name = os.path.basename(path)
        downloads.append(
            'wget -q -O %s "$BASE/%s" || fail "download failed: %s"' % (tmp, name, name))
        srcsums.append('echo "%s  %s" | md5sum -c - || fail "md5 mismatch: %s"'
                       % (md5_file(path), tmp, name))

    for item in plan:
        part = item["part"]
        tmp = local[item["path"]]
        guards.append(
            'test "$(part_size %s)" = "%08x" || fail "partition %s is not %d bytes"'
            % (part, item["length"], part, item["length"]))
        writes.append('echo "=== write %s ==="' % part)
        writes.append(
            'dd if=%s bs=%d skip=%d count=%d 2>/dev/null | mtd write - %s '
            '>>$MTDLOG 2>&1 || fail "write failed: %s"'
            % (tmp, BLOCK, item["skip"], item["count"], part, part))
        writes.append('echo "%s written"' % part)
        readbacks.append(
            'GOT=$(dd if=/dev/mtd$(part_num %s) bs=%d count=%d 2>/dev/null | '
            'md5sum | cut -d" " -f1)' % (part, BLOCK, item["count"]))
        readbacks.append('echo "%s: $GOT"' % part)
        readbacks.append(
            '[ "$GOT" = "%s" ] || fail "readback mismatch on %s, expected %s"'
            % (item["md5"], part, item["md5"]))

    text = DEVICE_SCRIPT
    text = text.replace("@BASE@", base_url)
    text = text.replace("@GUARDS@", "\n".join(guards))
    text = text.replace("@DOWNLOADS@", "\n".join(downloads))
    text = text.replace("@LOCALFILES@", " ".join(sorted(local.values())))
    text = text.replace("@SRCSUMS@", "\n".join(srcsums))
    text = text.replace("@WRITES@", "\n".join(writes))
    text = text.replace("@READBACKS@", "\n".join(readbacks))
    text = text.replace("@DRYRUN@", "1" if dry_run else "0")
    return text


# --- bringing the device into a system started from RAM ------------------

def reboot_from_shell(port, baud):
    with serial.Serial(port, baud, timeout=0.3) as ser:
        sh = _common.Shell(ser)
        sh.wake()
        print("sending reboot and releasing the port for the ESC catch")
        ser.write(b"reboot\n")
        ser.flush()
        time.sleep(1.0)


def ram_boot_device(image, port, baud, esc_seconds, uart_seconds):
    """Run uart_ram_boot.py as a separate process, so as not to duplicate the
    already proven ETH/LOADADDR/AUTOBURN 0/TFTP sequence and its safety catches (the
    file name has to contain nfjrom)."""
    cmd = [sys.executable, os.path.join(HERE, "uart_ram_boot.py"),
           image, "--port", port, "--baud", str(baud),
           "--uart-seconds", str(uart_seconds)]
    if esc_seconds:
        cmd += ["--catch-esc", str(esc_seconds)]
    print("running: %s" % " ".join(cmd))
    rc = subprocess.call(cmd)
    if rc != 0:
        raise _common.SafetyError("uart_ram_boot.py returned %d - no system was started from RAM" % rc)


def ensure_ramboot_system(args):
    """Bring the device to the shell of a system started from RAM."""
    state, text = _common.console_state(args.port, args.baud, serial)
    print("console state: %s" % state)

    if state == "shell":
        with serial.Serial(args.port, args.baud, timeout=0.3) as ser:
            sh = _common.Shell(ser, login=args.login, password=args.password)
            sh.wake()
            mounts = sh.run("grep -c '^/dev/mtdblock\\|^/dev/root' /proc/mounts")
            if mounts.strip() == "0":
                print("the console already shows a system with no flash partitions "
                      "mounted - taking it to be running from RAM")
                return
        print("the system is running from flash - restarting it into the bootloader")
        reboot_from_shell(args.port, args.baud)
        ram_boot_device(args.ramboot_image, args.port, args.baud,
                        args.esc_seconds, args.boot_seconds)
    elif state == "bootloader":
        print("the device is already at the bootloader console - loading the image into RAM")
        ram_boot_device(args.ramboot_image, args.port, args.baud,
                        0, args.boot_seconds)
    else:
        print("the console is silent. Cycle the power of the device (do NOT hold "
              "WPS) - the ESC catch is already waiting.")
        ram_boot_device(args.ramboot_image, args.port, args.baud,
                        max(args.esc_seconds, 180), args.boot_seconds)


def run_ramboot_restore(args, plan):
    pc_ip = args.pc_ip or _common.guess_pc_ip()
    if not pc_ip:
        raise _common.SafetyError("could not work out the address of this computer - pass --pc-ip")

    ensure_ramboot_system(args)

    paths = sorted({item["path"] for item in plan})
    with _common.FileServer(paths, args.http_port, bind=pc_ip) as srv:
        base = "http://%s:%d" % (pc_ip, srv.port)
        script = render_device_script(base, plan, args.dry_run)
        script_path = os.path.join(srv.tmpdir, "restore.sh")
        with open(script_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(script)
        print("HTTP server: %s (%s)" % (base, ", ".join(srv.names + ["restore.sh"])))

        with serial.Serial(args.port, args.baud, timeout=0.3) as ser:
            sh = _common.Shell(ser, login=args.login, password=args.password)
            sh.wake()
            print(sh.run("cat /proc/mtd"))

            sh.run_ok('wget -q -O /tmp/restore.sh "%s/restore.sh"' % base,
                      timeout=30.0, what="delivering restore.sh")
            sh.run_ok("sh -n /tmp/restore.sh", timeout=20.0,
                      what="syntax check of restore.sh on the device")

            print("starting the restore on the device (the log is /tmp/restore.log)")
            print(sh.run_ok("sh /tmp/restore.sh", timeout=20.0,
                            what="starting restore.sh"))

            rc = wait_for_signal(ser, args.restore_timeout)

            print()
            print(sh.run("cat /tmp/restore.log", timeout=90.0))
            if rc != 0:
                raise _common.SafetyError(
                    "the script on the device finished with code %r - the write is "
                    "NOT confirmed" % rc)

            if args.dry_run:
                print()
                print("the dry run is finished: the dumps were delivered and "
                      "checked, flash was not touched.")
                return

            if args.no_reboot:
                print("--no-reboot: the device was left in the system running from RAM")
                return
            print("restarting the device into stock")
            ser.write(b"reboot\n")
            ser.flush()

    if not args.dry_run and not args.no_reboot:
        watch_stock_boot(args)


SIGNAL_RE = re.compile(r"@@RESTORE_EXIT=(\d+)@@")


def wait_for_signal(ser, timeout):
    """Wait for the signal line from the device, sending nothing into the port.

    Polling the device with commands while `mtd write` is running does not work: the
    console loses characters at that time (`ttyS0: input overrun`), the command
    arrives chewed up and the answer to it is junk. Confirmed live: polling every
    10 s fell apart on the very first write, even though the write itself went
    through and the md5 matched. So this only reads."""
    print("waiting for the signal from the device (up to %.0f s). Nothing is sent "
          "into the port: during a write the console loses input." % timeout)
    started = time.time()
    buf = ""
    next_tick = 30.0
    while time.time() - started < timeout:
        chunk = ser.read(4096)
        if chunk:
            buf += chunk.decode("latin-1")
            m = SIGNAL_RE.search(buf)
            if m:
                print("[%4.0f s] the device reported: code %s"
                      % (time.time() - started, m.group(1)))
                return int(m.group(1))
        elapsed = time.time() - started
        if elapsed > next_tick:
            print("[%4.0f s] the write is running..." % elapsed)
            next_tick += 30.0
    raise _common.SafetyError(
        "the device did not report within %.0f s. The write may well have gone "
        "through - look at /tmp/restore.done and /tmp/restore.log on the device "
        "before doing anything" % timeout)


def watch_stock_boot(args):
    """Show the stock boot and wait for its login prompt."""
    print("waiting for stock to boot (up to %.0f s)" % args.boot_seconds)
    deadline = time.time() + args.boot_seconds
    seen = ""
    with serial.Serial(args.port, args.baud, timeout=0.5) as ser:
        while time.time() < deadline:
            chunk = ser.read(4096)
            if not chunk:
                continue
            text = chunk.decode("latin-1")
            seen += text
            sys.stdout.write(text.encode("ascii", "replace").decode("ascii"))
            sys.stdout.flush()
            if "login:" in seen:
                print()
                print("stock has booted: the console shows its login prompt")
                return True
    print()
    print("no login prompt arrived within the time given - look at the console "
          "yourself")
    return False


# --- the path through the bootloader console -----------------------------

def run_bootloader_restore(args):
    if not args.image:
        raise _common.SafetyError(
            "--via bootloader needs at least one --image (an image built by "
            "rtk_mkimg.py build)")
    plan = _common.build_bootloader_plan(args.image)
    _common.print_bootloader_plan(plan, args.dry_run, "PLAN FOR RESTORING STOCK "
                                                      "THROUGH THE BOOTLOADER")

    if not _common.confirm("Connect to %s and run the plan?" % args.port, args.yes):
        print("cancelled by the user")
        sys.exit(1)

    _common.run_bootloader_flow(
        plan, args.port, args.baud, serial, args.dry_run, args.verify,
        host=args.host, catch_esc=args.catch_esc,
        boot_seconds=args.boot_seconds, login=args.login,
        password=args.password)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--via", choices=("ramboot", "bootloader"), default="ramboot",
                    help="which path to restore stock by (default ramboot - the one "
                         "that has been used on a live device)")
    ap.add_argument("--port", default=rtk_romloader.PORT_DEFAULT,
                    help="the UART serial port (default %s)" % rtk_romloader.PORT_DEFAULT)
    ap.add_argument("--baud", type=int, default=rtk_romloader.BAUD_DEFAULT,
                    help="UART speed (default %d)" % rtk_romloader.BAUD_DEFAULT)
    ap.add_argument("--dry-run", action="store_true",
                    help="ramboot: go as far as the md5 check and stop; "
                         "bootloader: AUTOBURN 0, the transfer happens, the write does not")
    ap.add_argument("--yes", action="store_true",
                    help="do not ask for confirmation before the real write")

    g = ap.add_argument_group("ramboot mode")
    g.add_argument("--dump0", default=DEFAULT_DUMP0,
                   help="the stock dump of mtd0 (boot+cfg+linux)")
    g.add_argument("--dump1", default=DEFAULT_DUMP1,
                   help="the stock dump of mtd1 (rootfs)")
    g.add_argument("--ramboot-image", default=DEFAULT_RAMBOOT_IMAGE,
                   help="our image to start from RAM (the name has to contain nfjrom)")
    g.add_argument("--pc-ip", default=None, type=_common.ipv4_arg,
                   help="address of the computer as the device sees it (worked out "
                        "automatically by default)")
    g.add_argument("--http-port", type=int, default=8000,
                   help="port of the temporary HTTP server on the computer (default 8000)")
    g.add_argument("--login", default=None, help="login on the console, if it asks for one")
    g.add_argument("--password", default=None, help="password on the console")
    g.add_argument("--esc-seconds", type=float, default=120.0,
                   help="how many seconds to catch the bootloader by spamming ESC")
    g.add_argument("--boot-seconds", type=float, default=120.0,
                   help="how many seconds to wait for the system to boot")
    g.add_argument("--restore-timeout", type=float, default=600.0,
                   help="how long to wait for the restore script on the device")
    g.add_argument("--no-reboot", action="store_true",
                   help="do not restart the device into stock after the write")

    b = ap.add_argument_group("bootloader mode")
    b.add_argument("--image", action="append", metavar="FILE",
                   help="an image built by rtk_mkimg.py build; may be given several times")
    b.add_argument("--host", default=None,
                   help="address of the bootloader console (default 192.168.1.6, "
                        "which ETH sets unconditionally)")
    b.add_argument("--catch-esc", type=float, default=120.0, metavar="SEC",
                   help="if the device is not at the bootloader console, restart it "
                        "and catch `<RealTek>` by spamming ESC for this many seconds "
                        "(0 means assume it is already in the bootloader)")
    b.add_argument("--verify", choices=_common.VERIFY_MODES, default="sample",
                   help="readback after the write: sample (the default, three 4 KB "
                        "pieces), full (the whole body, about 449 B/s over the UART) "
                        "or none")

    args = ap.parse_args()

    if serial is None:
        raise SystemExit("pyserial is required: pip install pyserial")

    try:
        if args.via == "bootloader":
            run_bootloader_restore(args)
        else:
            files, plan = build_restore_plan(args.dump0, args.dump1)
            ram_boot.check_name_executes(os.path.basename(args.ramboot_image))
            if not os.path.isfile(args.ramboot_image):
                raise _common.SafetyError("no image to start from RAM: %s"
                                          % args.ramboot_image)
            print_restore_plan(files, plan, args.dry_run)
            if not _common.confirm("Connect to %s and run the plan?"
                                   % args.port, args.yes):
                print("cancelled by the user")
                sys.exit(1)
            run_ramboot_restore(args, plan)
    except _common.SafetyError as e:
        print()
        print("STOPPED: %s" % e)
        sys.exit(1)

    print()
    print("the dry run is finished." if args.dry_run else "done.")


if __name__ == "__main__":
    main()
