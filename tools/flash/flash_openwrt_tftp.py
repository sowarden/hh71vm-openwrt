#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
flash_openwrt_tftp.py - install our OpenWrt port through the `<RealTek>` bootloader
console (UART + TFTP + AUTOBURN), the same path `restore_stock.py --via bootloader`
uses to put stock back, only with OpenWrt images.

This is the fallback path: the normal first installation goes through the vendor
`fwupg` (`flash_openwrt_vendor.py`), and updating an already installed system is
`sysupgrade`. This script is for when there is no working system on the device.

NOTES ON THE PATH
    1. The ETH -> LOADADDR -> AUTOBURN -> TFTP path has been used on a live device:
       the mechanism was proven on test images up to 510 KB, and full-size kernel
       and rootfs images were flashed with this script.
    2. `*-fwupg.bin` is a CONTAINER for /bin/fwupg (several sections back to back:
       `cr6c` plus `r6cr`), and splitting one file into several sections is the
       documented logic of `fwupg` itself, not of the bootloader. Whether the
       `<RealTek>` bootloader can split several concatenated sections out of one
       received file in TFTP+AUTOBURN mode is NOT documented, and rtk_romloader.py
       does not do it (send_image_via_tftp() handles exactly one header at the start
       of the file). So:

       TWO SEPARATE single-section images are needed - the kernel (`cr6c`, burnAddr
       0x030000) and the rootfs (`r6cr`, burnAddr 0x300000). They do not have to be
       prepared by hand: `--from-container <file>-fwupg.bin` slices the container
       into sections in memory (they sit back to back, each already carrying its own
       header) and puts them into a safe order at the same time - see point 4.
       `--image` accepts single-section files only.
    4. The order of the sections matters. After writing `cr6c` the bootloader
       restarts the board BY ITSELF, and anything that came after it in the plan
       will not be written this power cycle. Such sections therefore go last, and
       their readback is done on a separate trip into the bootloader from the booted
       system.
    3. The rootfs section has to be built from a squashfs with its length stamped
       inside (`stamp_rootfs_len` in image/mkrtkimg.py), or the bootloader computes
       a checksum over some 2.3 GB and never reaches the boot. In the published
       builds that stamp is already applied as part of the normal build.

WHAT IT DOES
    The same as restore_stock.py: it takes one or more `rtk_mkimg.py build` images,
    builds and prints the plan, checks the forbidden areas, uploads them one after
    another through ETH -> LOADADDR -> AUTOBURN ->
    `rtk_romloader.RomLoader.send_image_via_tftp()`, and on a real write does the
    readback verification. All the shared logic lives in _common.py and the protocol
    in rtk_romloader.py/rtk_tftp_put.py; none of it is duplicated here.

SAFETY CATCHES
    The same as restore_stock.py: a hard ban on writing to 0x000000-0x01FFFF and
    0x020000-0x023FFF, a warning about the off-by-one sector erase, the plan plus a
    confirmation before a real write (unless --yes), --dry-run meaning AUTOBURN 0,
    and readback verification after a real write.

REQUIREMENTS
    The same as restore_stock.py - pip install pyserial, the sibling modules in this
    directory, a host firewall rule for python.exe, and the device at the
    `<RealTek>` prompt.

EXAMPLES
    python flash_openwrt_tftp.py --from-container openwrt-...-hh71vm-fwupg.bin --dry-run
    python flash_openwrt_tftp.py --from-container openwrt-...-hh71vm-fwupg.bin --yes
    python flash_openwrt_tftp.py --image openwrt-rootfs-r6cr.img --image openwrt-kernel-cr6c.img --yes
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rtk_romloader  # noqa: E402
import _common  # noqa: E402

try:
    import serial
except ImportError:
    serial = None


OPENWRT_NOTES = [
    "",
    "REMINDER:",
    "  - if the plan contains an r6cr section, it has to be built from a squashfs "
    "with its length already stamped in (stamp_rootfs_len), or the bootloader will "
    "hang computing a checksum on the next ordinary boot; the published builds "
    "already carry that stamp;",
    "  - the kernel and the rootfs are uploaded as TWO separate single-section "
    "images: the bootloader parses only one header at the start of the file it "
    "receives. A ready *-fwupg.bin is passed through --from-container instead;",
    "  - this path does not touch the rootfs_data partition (0x600000): after an "
    "installation over stock it is cleaned by a preinit hook on the first boot.",
]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default=rtk_romloader.PORT_DEFAULT,
                    help="the UART serial port (default %s)" % rtk_romloader.PORT_DEFAULT)
    ap.add_argument("--baud", type=int, default=rtk_romloader.BAUD_DEFAULT,
                    help="UART speed (default %d)" % rtk_romloader.BAUD_DEFAULT)
    ap.add_argument("--image", action="append", metavar="FILE",
                    help="a SINGLE-SECTION rtk_mkimg.py build image (the kernel or "
                         "the rootfs separately); may be given several times")
    ap.add_argument("--from-container", metavar="FILE",
                    help="take the sections from a ready *-fwupg.bin container and "
                         "slice them into single-section images in memory - easier "
                         "than preparing the files by hand")
    ap.add_argument("--host", default=None,
                    help="address of the bootloader console (default 192.168.1.6)")
    ap.add_argument("--dry-run", action="store_true",
                    help="AUTOBURN 0 - the transfer happens, the write to flash does not")
    ap.add_argument("--yes", action="store_true",
                    help="do not ask for confirmation before the real write")
    ap.add_argument("--catch-esc", type=float, default=120.0, metavar="SEC",
                    help="if the device is not at the bootloader console, restart it "
                         "and catch `<RealTek>` by spamming ESC for this many seconds "
                         "(0 means assume it is already in the bootloader)")
    ap.add_argument("--boot-seconds", type=float, default=200.0,
                    help="how long to wait for the system to boot before going back "
                         "into the bootloader for the postponed verification")
    ap.add_argument("--login", default=None,
                    help="login on the system console, if it asks for one")
    ap.add_argument("--password", default=None, help="password on the system console")
    ap.add_argument("--verify", choices=_common.VERIFY_MODES, default="sample",
                    help="readback after the write: sample (the default, three 4 KB "
                         "pieces), full (the whole body, about 449 B/s over the UART "
                         "- hours for an image of several megabytes) or none")
    args = ap.parse_args()

    if bool(args.image) == bool(args.from_container):
        print("REFUSED: either one or more --image, or a single --from-container is "
              "needed, but not both and not neither")
        sys.exit(1)

    for path in args.image or []:
        base = os.path.basename(path).lower()
        if "fwupg" in base:
            print("REFUSED: the name %r looks like a combined *-fwupg.bin container - "
                  "the bootloader parses only one header at the start of the file. "
                  "Pass that file as --from-container and the script will slice it "
                  "itself; to install from a running stock system there is "
                  "flash_openwrt_vendor.py." % path)
            sys.exit(1)

    try:
        if args.from_container:
            with open(args.from_container, "rb") as f:
                container = f.read()
            prefix = os.path.basename(args.from_container)
            for suffix in (".bin", ".img"):
                if prefix.lower().endswith(suffix):
                    prefix = prefix[:-len(suffix)]
            # `nfjrom` in a name switches the bootloader to "run from RAM", and
            # `fwupg` was just refused as an input file name - both are cut out of
            # the names the sections will travel under over TFTP.
            prefix = prefix.replace("nfjrom", "x").replace("fwupg", "section")
            blobs = _common.split_container(container, prefix)
            print("container %s: %d sections -> %s"
                  % (args.from_container, len(blobs),
                     ", ".join(name for name, _ in blobs)))
            blobs, reordered = _common.order_reboot_last(blobs)
            if reordered:
                print("the order was changed: the kernel section restarts the board "
                      "right after it is written, so it goes last - otherwise the "
                      "rootfs would never get written. New order: %s"
                      % ", ".join(name for name, _ in blobs))
            plan = _common.build_bootloader_plan_blobs(blobs)
        else:
            plan = _common.build_bootloader_plan(args.image)
    except _common.SafetyError as e:
        print("REFUSED BEFORE CONNECTING TO THE PORT: %s" % e)
        sys.exit(1)

    _common.print_bootloader_plan(plan, args.dry_run, "PLAN FOR INSTALLING OPENWRT (TFTP/AUTOBURN)",
                                  extra_notes=OPENWRT_NOTES)

    if not _common.confirm("Connect to %s and run the plan?" % args.port, args.yes):
        print("cancelled by the user")
        sys.exit(1)

    try:
        if serial is None:
            raise SystemExit("pyserial is required: pip install pyserial")
        _common.run_bootloader_flow(
            plan, args.port, args.baud, serial, args.dry_run, args.verify,
            host=args.host, catch_esc=args.catch_esc,
            boot_seconds=args.boot_seconds, login=args.login,
            password=args.password)
    except _common.SafetyError as e:
        print("STOPPED: %s" % e)
        sys.exit(1)

    print()
    if args.dry_run:
        print("the dry run is finished, flash was not touched.")
    else:
        print("the write is finished and confirmed by readback, and the board has "
              "been restarted - an ordinary boot follows.")
        print("Keep access to the bootloader console within reach "
              "(uart_ram_boot.py --listen-only --catch-esc) in case you need to roll "
              "back with restore_stock.py.")


if __name__ == "__main__":
    main()
