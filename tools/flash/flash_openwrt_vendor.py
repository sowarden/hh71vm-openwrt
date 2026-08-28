#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
flash_openwrt_vendor.py - install our OpenWrt port the vendor way (`/bin/fwupg`)
from an ALREADY RUNNING stock system, using the `*-hh71vm-fwupg.bin` image.

This is the normal first-installation path over UART: it needs neither the
bootloader console nor TFTP, only the stock UART shell and the network.

STATUS OF THIS PATH
    The whole procedure was carried out on a live device: `fwupg check` ->
    `fwupg reboot` -> `fwd` writing `mtdblock0`/`mtdblock1` -> our OpenWrt booting
    from flash -> settings surviving a reboot.

    The first boot after installation normally takes about two minutes: the hook
    `79_wipe_stale_rootfs_data.sh` erases `rootfs_data`, where the middle of the
    stock rootfs is left behind by `fwupg`. That is not a hang.

    The image is delivered to `/tmp/fw.bin` through a temporary HTTP server on the
    computer and `curl` on the device. The result is checked by size and MD5 on the
    device itself.

    The firmware-update form in the stock web interface has never been tested.

WHAT IT DOES
    1. Parses the local image file as an fwupg container (several 16-byte sections
       back to back - the format is identical to rtk_mkimg.py, but `fwupg`/`fwd`
       uses it INDEPENDENTLY of the bootloader) and works out which absolute flash
       addresses each section will land on:
         cs6c/cr6c -> the "boot+cfg+linux" partition (physically 0x000000-0x2FFFFF),
                      offset = burnAddr from the header, header and body are written;
         r6cr      -> the "rootfs" partition (physically 0x300000-0xBFFFFF), offset
                      is ALWAYS 0 (the burnAddr in the header is ignored - that is a
                      quirk of fwd, not to be confused with the bootloader path),
                      only the body is written;
         w6cg      -> the target partition is unknown -> the script REFUSES rather
                      than guessing.
    2. Checks every section: the checksum, that it fits inside its own partition
       (fwd does not check those bounds itself - a short write goes into an endless
       reboot loop with no rollback) and - unconditionally - that nothing lands in
       0x000000-0x01FFFF (bootloader) or 0x020000-0x023FFF (hwsetting/MAC). In our
       real image the sections sit deep inside their partitions (kernel from
       0x30000, rootfs from 0x300000) and never come close, but the check does not
       rely on that - it is recomputed from the headers of the file.
    3. Prints the plan and demands a confirmation (unless --yes) BEFORE touching the
       device at all.
    4. Over the UART shell (reusing `uart_shell.py` as a module, so the marker
       protocol for reading output is not duplicated):
         a. gets past the login prompt: the stock console asks for one, the default
            login is `root` and the password comes from `--password`;
         b. checks that `/tmp/jrd-resource*` and `/tmp/ipq` are absent - their
            presence sends `fwupg` (in ANY mode, even `check`) into a branch with
            `killall -9` and `mtd erase mtd2` BEFORE it even looks at fw.bin. If any
            are found the script STOPS and does not remove them itself, leaving that
            decision to the operator;
         c. starts a temporary HTTP server on the computer and asks the device to
            `curl -f -o /tmp/fw.bin http://<pc-ip>:<port>/<file>`, then checks the
            size and MD5 of what arrived;
         d. `fwupg check file /tmp/fw.bin` - the answer has to contain "Firmware
            upgrade check OK!", otherwise it stops BEFORE `reboot`;
         e. (unless --dry-run, and after a second confirmation)
            `fwupg reboot file /tmp/fw.bin` - the irreversible write plus a watchdog
            restart, after which the script shows the boot and waits for the prompt
            of our system.

SAFETY CATCHES
    - 0x000000-0x01FFFF and 0x020000-0x023FFF are forbidden outright, with no way
      around it.
    - `check` before `reboot` is mandatory and never skipped.
    - The confirmation before `fwupg reboot` is separate from the one for the plan,
      because that is the last reversible moment (the file is on the device, the
      write has not happened yet).
    - `--dry-run` stops RIGHT AFTER a successful `fwupg check` and never sends
      `fwupg reboot`.
    - In `reboot` mode the vendor `fwupg` ITSELF changes two MIB fields
      (`flash set WLAN0_VAP0_WLAN_DISABLED 1`, and the same for WLAN1). That is a
      side effect of `fwupg`, not of this script, but it is printed in the plan in
      advance so it does not come as a surprise.

REQUIREMENTS
    pip install pyserial
    rtk_mkimg.py and uart_shell.py in the same directory (used as modules).
    The device has to be on a WORKING stock system (not in the bootloader), and the
    UART has to give a login or shell prompt. The computer and the device must be on
    the same network; the address of the computer is given with `--pc-ip` or worked
    out automatically.
    The host firewall has to allow inbound TCP on the HTTP server port for the
    `python.exe` the script runs under.
    Keep a way back into the bootloader console ready (WPS or ESC): if the system
    does not come up after the write, the rollback is restore_stock.py.

EXAMPLES
    python flash_openwrt_vendor.py --image openwrt-hh71vm-fwupg.bin --password ... --dry-run
    python flash_openwrt_vendor.py --image openwrt-hh71vm-fwupg.bin --password ... --yes
"""

import argparse
import hashlib
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rtk_mkimg  # noqa: E402
import _common  # noqa: E402

try:
    import serial
except ImportError:
    serial = None


# --- the partition layout -----------------------------------------------
MTD0_BASE, MTD0_SIZE = 0x000000, 0x300000   # "boot+cfg+linux"
MTD1_BASE, MTD1_SIZE = 0x300000, 0x900000   # "rootfs"
# mtd2 ("jffs2 file") = 0xC00000, 0x400000 is not touched by this path; fwupg/fwd
# only opens mtd2 in the separate branch that handles /tmp/jrd-resource-jffs2.img,
# which this script has to keep from happening (see the jrd-resource check below).

class VendorError(_common.SafetyError):
    pass


# Parsing the container lives in _common (flash_openwrt_tftp.py uses it too).
walk_fwupg_sections = _common.walk_fwupg_sections


def section_target(sec):
    """Return (device_label, abs_lo, abs_hi, header_written) for a section of an
    fwupg container. w6cg gives None (its partition is not documented)."""
    sig = sec["sig"]
    if sig in ("cs6c", "cr6c"):
        # the offset is burnAddr, and header plus body are written (len+16)
        lo = MTD0_BASE + sec["burn_addr"]
        hi = lo + 16 + sec["length"] - 1
        if hi > MTD0_BASE + MTD0_SIZE - 1:
            raise VendorError(
                "section %r: a write of 0x%X..0x%X runs past the end of the "
                "boot+cfg+linux partition (0x%X..0x%X) - fwd does not check the "
                "bounds itself, and a short write goes into an endless watchdog "
                "loop with no rollback"
                % (sig, lo, hi, MTD0_BASE, MTD0_BASE + MTD0_SIZE - 1))
        return "boot+cfg+linux (mtdblock0)", lo, hi, True
    if sig == "r6cr":
        # the offset is ALWAYS 0 and the burnAddr in the header is IGNORED - a quirk
        # of fwd, not of the bootloader
        lo = MTD1_BASE
        hi = lo + sec["length"] - 1
        if hi > MTD1_BASE + MTD1_SIZE - 1:
            raise VendorError(
                "section r6cr: a body of 0x%X bytes does not fit in the rootfs "
                "partition (0x%X..0x%X) - fwd does not check the bounds itself"
                % (sec["length"], MTD1_BASE, MTD1_BASE + MTD1_SIZE - 1))
        return "rootfs (mtdblock1)", lo, hi, False
    return None, None, None, None


def build_vendor_plan(image_path):
    with open(image_path, "rb") as f:
        data = f.read()
    sections = walk_fwupg_sections(data)
    plan = []
    for sec in sections:
        label, lo, hi, header_written = section_target(sec)
        if label is None:
            raise VendorError(
                "section %r: the target partition for this signature is not known - "
                "only cs6c, cr6c and r6cr have a documented destination, and the "
                "script refuses to guess." % sec["sig"])
        _common.check_not_forbidden(lo, hi, "%s (section %s): " % (image_path, sec["sig"]))
        plan.append(dict(sec, label=label, abs_lo=lo, abs_hi=hi,
                         header_written=header_written))
    return data, plan


def print_vendor_plan(image_path, data, plan, dry_run):
    _common.print_header("PLAN FOR INSTALLING OPENWRT THROUGH /bin/fwupg"
                         + (" (DRY RUN)" if dry_run else ""))
    print("image: %s (%d bytes)" % (image_path, len(data)))
    for i, item in enumerate(plan, 1):
        print("%d. section %s -> %s" % (i, item["sig"], item["label"]))
        print("   file: offset 0x%X, body=%d bytes" % (item["file_offset"], item["length"]))
        print("   absolute flash address: 0x%06X..0x%06X (%s)"
              % (item["abs_lo"], item["abs_hi"],
                 "header and body" if item["header_written"] else "body only"))
    print()
    print("The areas 0x000000-0x01FFFF (bootloader) and 0x020000-0x023FFF "
          "(hwsetting/MAC) are not part of the plan and cannot be added to it "
          "by any flag.")
    print("A side effect of fwupg itself in reboot mode: it sets "
          "WLAN0_VAP0_WLAN_DISABLED=1 and the same for WLAN1 in the MIB, BEFORE "
          "writing anything.")
    print("mtd2 (jffs2, the settings) is not touched by this path as long as "
          "/tmp/jrd-resource* and /tmp/ipq are absent - which is checked separately.")
    if dry_run:
        print("DRY RUN: the script will stop right after a successful "
              "'fwupg check' and will not send 'fwupg reboot'.")
    else:
        print("'fwupg reboot file /tmp/fw.bin' will be sent - the write is "
              "IRREVERSIBLE. The first boot after it normally takes about two "
              "minutes while rootfs_data is erased. Keep the rollback through "
              "restore_stock.py within reach.")


# --- commands in the live device shell ----------------------------------

def check_jrd_resource_absent(sh):
    checks = [
        ("/tmp/jrd-resource-jffs2.img", "erases mtd2"),
        ("/tmp/jrd-resource", "vendor resource handling"),
        ("/tmp/ipq", "vendor resource handling"),
    ]
    found = []
    for path, ref in checks:
        out = sh.run("test -e %s && echo LEFTOVER_FOUND || echo LEFTOVER_ABSENT" % path)
        if "LEFTOVER_FOUND" in out:
            found.append((path, ref))
        elif "LEFTOVER_ABSENT" not in out:
            raise VendorError(
                "the check for %s gave no clear answer: %r - not going on, silence "
                "here must not be read as 'the file is not there'" % (path, out))
    if found:
        raise VendorError(
            "leftovers were found on the device that fwupg acts on BEFORE it even "
            "looks at fw.bin (even in check mode): %s. Remove them by hand (for "
            "example 'rm -f <path>') and run the script again - this script does not "
            "delete them itself." % ", ".join("%s (%s)" % (p, r) for p, r in found))


def deliver_image(sh, image_path, data, base_url, remote_name):
    """Put the image into /tmp/fw.bin and prove that exactly the same thing
    arrived: the size and MD5 are computed on the device itself."""
    url = "%s/%s" % (base_url, remote_name)
    print("downloading onto the device: curl -f -o /tmp/fw.bin %s" % url)
    # -f is essential: without it curl treats an HTTP error page as success.
    sh.run_ok("curl -f -s -o /tmp/fw.bin '%s'" % url, timeout=180.0,
              what="delivering the image to the device")

    size_out = sh.run("wc -c < /tmp/fw.bin", timeout=20.0)
    try:
        remote_size = int(size_out.strip().split()[0])
    except (ValueError, IndexError):
        raise VendorError("could not read the size of /tmp/fw.bin: %r" % size_out)
    if remote_size != len(data):
        raise VendorError(
            "the size of /tmp/fw.bin on the device (%d) does not match the local "
            "file (%d) - the transfer is damaged, not going on"
            % (remote_size, len(data)))
    print("the size on the device matches: %d bytes" % remote_size)

    local_md5 = hashlib.md5(data).hexdigest()
    md5_out = sh.run("md5sum /tmp/fw.bin", timeout=120.0)
    if "md5sum" in md5_out and "not found" in md5_out:
        print("WARNING: this firmware has no md5sum - the delivery is confirmed "
              "by size only")
        return
    if local_md5 not in md5_out:
        raise VendorError(
            "the MD5 of /tmp/fw.bin on the device does not match the local %s:\n%s"
            % (local_md5, md5_out.strip()))
    print("the MD5 on the device matches: %s" % local_md5)


def watch_boot(ser, seconds, needles):
    """Show the boot and wait for any of the marker lines."""
    print("watching the boot (up to %.0f s); the first boot after fwupg normally "
          "takes about two minutes - rootfs_data is being erased" % seconds)
    deadline = time.time() + seconds
    seen = ""
    while time.time() < deadline:
        chunk = ser.read(4096)
        if not chunk:
            continue
        text = chunk.decode("latin-1")
        seen += text
        sys.stdout.write(text.encode("ascii", "replace").decode("ascii"))
        sys.stdout.flush()
        for needle in needles:
            if needle in seen:
                print()
                print("saw %r on the console - the system is up" % needle)
                return True
    print()
    print("no boot marker arrived within the time given - check the console")
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default=_common.uart_shell.PORT_DEFAULT,
                    help="the UART serial port (default %s)"
                         % _common.uart_shell.PORT_DEFAULT)
    ap.add_argument("--baud", type=int, default=_common.uart_shell.BAUD_DEFAULT,
                    help="UART speed (default %d)"
                         % _common.uart_shell.BAUD_DEFAULT)
    ap.add_argument("--image", required=True, metavar="FILE",
                    help="the *-hh71vm-fwupg.bin container for /bin/fwupg")
    ap.add_argument("--login", default=_common.STOCK_LOGIN,
                    help="login on the stock console (default %s)"
                         % _common.STOCK_LOGIN)
    ap.add_argument("--password", default=None,
                    help="password on the stock console (it asks for one)")
    ap.add_argument("--pc-ip", default=None, type=_common.ipv4_arg,
                    help="address of the computer as the device sees it (by default "
                         "worked out automatically and printed for checking)")
    ap.add_argument("--http-port", type=int, default=8000,
                    help="port of the temporary HTTP server on the computer (default 8000)")
    ap.add_argument("--dry-run", action="store_true",
                    help="stop right after a successful 'fwupg check' and do not "
                         "send 'fwupg reboot'")
    ap.add_argument("--yes", action="store_true",
                    help="do not ask for confirmations (both the plan AND the final reboot step)")
    ap.add_argument("--boot-seconds", type=float, default=240.0,
                    help="how many seconds to show the boot after the write")
    args = ap.parse_args()

    if serial is None:
        raise SystemExit("pyserial is required: pip install pyserial")

    try:
        data, plan = build_vendor_plan(args.image)
    except _common.SafetyError as e:
        print("REFUSED BEFORE CONNECTING TO THE PORT: %s" % e)
        sys.exit(1)

    print_vendor_plan(args.image, data, plan, args.dry_run)

    if not _common.confirm("Connect to %s and start?" % args.port, args.yes):
        print("cancelled by the user")
        sys.exit(1)

    pc_ip = args.pc_ip or _common.guess_pc_ip()
    if not pc_ip:
        raise SystemExit("could not work out the address of this computer - pass --pc-ip")
    print("address of this computer for HTTP: %s (check that the device really sees "
          "this address - it was worked out automatically unless --pc-ip was given)" % pc_ip)

    installed = False
    ser = serial.Serial(args.port, args.baud, timeout=0.3)
    try:
        sh = _common.Shell(ser, login=args.login, password=args.password)
        print("waking the device console...")
        sh.wake()

        banner = sh.run("cat /proc/version; cat /proc/mtd", timeout=20.0)
        print(banner)
        if "boot+cfg+linux" not in banner:
            raise VendorError(
                "/proc/mtd does not show the stock 'boot+cfg+linux' layout - the "
                "device is not running stock, and `fwupg` only exists on stock. This "
                "path only applies to installing FROM stock.")

        print("checking that /tmp/jrd-resource* and /tmp/ipq are absent...")
        check_jrd_resource_absent(sh)
        print("clean.")

        print("starting a temporary HTTP server on port %d..." % args.http_port)
        with _common.FileServer([args.image], args.http_port, bind=pc_ip) as srv:
            deliver_image(sh, args.image, data,
                          "http://%s:%d" % (pc_ip, srv.port), srv.names[0])

        print("fwupg check file /tmp/fw.bin ...")
        check_out = sh.run("fwupg check file /tmp/fw.bin", timeout=60.0)
        print(check_out)
        if "Firmware upgrade check OK!" not in check_out:
            raise VendorError(
                "'fwupg check' did not confirm the image (we expected 'Firmware "
                "upgrade check OK!') - 'fwupg reboot' is NOT being sent")

        if args.dry_run:
            print()
            print("DRY RUN: the check passed, stopping here. /tmp/fw.bin is still on "
                  "the device - remove it by hand if you like "
                  "('rm -f /tmp/fw.bin').")
            return

        if not _common.confirm(
                "The check passed. Send 'fwupg reboot file /tmp/fw.bin' - the "
                "IRREVERSIBLE write?", args.yes):
            print("cancelled by the user before the irreversible step")
            sys.exit(1)

        print("fwupg reboot file /tmp/fw.bin ...")
        ser.write(b"fwupg reboot file /tmp/fw.bin\n")
        ser.flush()
        installed = watch_boot(ser, args.boot_seconds,
                               ("procd: - init complete -", "please press enter",
                                "Please press Enter"))
    finally:
        ser.close()

    print()
    if installed:
        print("Installation done: `fwd` wrote the kernel and rootfs and the system "
              "came up. From here the device is updated with `sysupgrade`, and "
              "restore_stock.py puts stock back.")
    else:
        print("The write command was sent, but no boot marker was seen. Check the "
              "console; if the system does not come up, catch the bootloader "
              "(uart_ram_boot.py --listen-only --catch-esc 150) and restore stock "
              "with restore_stock.py.")


if __name__ == "__main__":
    main()
