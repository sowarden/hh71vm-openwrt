#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
reset_openwrt.py - erase the HH71VM settings partition over TFTP, with NO UART.

WHY THIS EXISTS

    A bad configuration can lock you out of the router completely.  Put the LAN
    port into the WAN zone, or break /etc/config/network in any other way, and
    SSH, LuCI and Ethernet all go at once.

    Reinstalling the firmware does NOT get you out of that, and it surprises
    everyone the first time.  install_openwrt_lan.py writes the kernel and the
    root filesystem, both of which end below 0x600000.  The settings live in
    rootfs_data at 0x600000 and are never touched, and the first-boot hook
    (79_wipe_stale_rootfs_data.sh) deliberately preserves a partition that still
    starts with the JFFS2 magic 85 19.  So the same broken /etc/config/network
    comes back from the overlay and shadows the fresh copy in /rom.

    Until now the only way out was to restore stock firmware - which writes
    across 0x600000 and destroys the overlay as a side effect - and then install
    OpenWrt again.  That is a long way round for "forget my settings".

WHAT IS WRITTEN

    0x000000  128 KB  boot          bootloader     - never touched, cannot be
    0x020000   16 KB  hwsetting     MAC addresses  - never touched, cannot be
    0x024000   48 KB  config        stock MIB      - not touched
    0x030000 2880 KB  kernel        NOT touched - the firmware stays as it is
    0x300000 3072 KB  rootfs        NOT touched - the firmware stays as it is
    0x600000 6144 KB  rootfs_data   ERASED - this is the whole point
    0xC00000 4096 KB  vendor_jffs2  NOT touched

    Only the settings go.  The installed firmware, its version and its packages
    stay exactly where they were, which is what makes this different from a
    reinstall.

ABOUT THE SECTOR THAT IS ERASED BEYOND THE WRITE

    flash_write in the bootloader erases

        nblocks = (dst + len) / erasesize - dst / erasesize + 1

    sectors, so a write ending exactly on a sector boundary erases one sector
    BEYOND itself.  rootfs_data is 0x600000..0xBFFFFF, so a full 6 MiB write
    would end at 0xC00000 and take the first sector of vendor_jffs2 with it.

    The payload is therefore 6 MiB minus one sector (0x5FF000).  It ends at
    0xBFF000, the extra erased sector is the last sector of rootfs_data itself,
    and the erase covers 0x600000..0xBFFFFF exactly - the partition, and not one
    byte more.

THE TWO CHECKSUM BYTES

    An r6cr body carries a two-byte checksum at its end, and those two bytes are
    not 0xFF.  After a full erase the partition is blank apart from them, which
    the first-boot hook reads as "someone else's data" and erases once more,
    taking about a minute.  That is harmless, and it is why the first boot after
    this is slow.  It also means the result is clean even on a firmware whose
    hook never runs, because the bootloader has already erased every sector.

AFTER IT RUNS

    The r6cr section does not restart the board.  Power it off and on yourself,
    without holding any button, and it boots the firmware it already had with
    the settings of a fresh install: 192.168.1.1, no password.

EXAMPLES
    python tools/flash/reset_openwrt.py --dry-run
    python tools/flash/reset_openwrt.py
    python tools/flash/reset_openwrt.py --quick
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import rtk_mkimg                                         # noqa: E402
import _common                                           # noqa: E402
import _lan                                              # noqa: E402

SECTOR = 0x1000
ROOTFS_DATA_ADDR = 0x600000
ROOTFS_DATA_SIZE = 0x600000

# One sector short of the partition, so the bootloader's extra erased sector is
# the last sector of rootfs_data instead of the first sector of vendor_jffs2.
FULL_BODY_LEN = ROOTFS_DATA_SIZE - SECTOR

# Enough to destroy the JFFS2 magic at the start of the partition and to leave
# bytes that are not 0xFF, which is what the first-boot hook looks for.
QUICK_BODY_LEN = SECTOR


def build_payload(quick):
    """The image to upload, and a sentence describing what it does."""
    if quick:
        # 0x00 and not 0xFF: the hook decides a partition holding nothing but
        # 0xFF is empty, and an empty partition is left alone.
        core = b"\x00" * (QUICK_BODY_LEN - 2)
        what = ("the first %d bytes of rootfs_data are overwritten, which destroys "
                "the JFFS2 magic, and the first boot erases the rest"
                % QUICK_BODY_LEN)
    else:
        core = b"\xff" * (FULL_BODY_LEN - 2)
        what = ("the whole of rootfs_data (%d KiB) is erased by the bootloader"
                % (ROOTFS_DATA_SIZE // 1024))
    data = rtk_mkimg.build_image("r6cr", ROOTFS_DATA_ADDR, core)
    name = "reset-rootfs-data-quick.img" if quick else "reset-rootfs-data.img"
    return name, data, what


def erase_range(body_len):
    """The sectors flash_write really erases for a write of body_len at the start
    of the partition - the bootloader's own formula, not an assumption."""
    dst = ROOTFS_DATA_ADDR
    nblocks = (dst + body_len) // SECTOR - dst // SECTOR + 1
    return dst, dst + nblocks * SECTOR - 1


def parse_args():
    p = argparse.ArgumentParser(
        description="Erase the HH71VM settings partition (rootfs_data) over TFTP.")
    p.add_argument("--quick", action="store_true",
                   help="write one sector instead of the whole partition and let the "
                        "first-boot hook finish the erase. Seconds instead of about "
                        "two minutes, but it needs a firmware that has that hook.")
    p.add_argument("--dry-run", action="store_true",
                   help="show the plan and send nothing.")
    p.add_argument("--yes", action="store_true", help="do not ask for confirmation.")
    p.add_argument("--pc-ip", help="the address of this computer on 192.168.1.0/24.")
    return p.parse_args()


def main():
    args = parse_args()

    _common.print_header("Step 1. What will be erased")
    name, data, what = build_payload(args.quick)
    plan = _common.build_bootloader_plan_blobs([(name, data)])
    lo, hi = erase_range(plan[0]["length"])

    if hi > ROOTFS_DATA_ADDR + ROOTFS_DATA_SIZE - 1:
        raise _common.SafetyError(
            "the erase would reach 0x%06X, past the end of rootfs_data at 0x%06X - "
            "refusing, because what follows is vendor_jffs2"
            % (hi, ROOTFS_DATA_ADDR + ROOTFS_DATA_SIZE - 1))

    _common.print_bootloader_plan(
        plan, args.dry_run, "Write plan",
        extra_notes=[
            "The bootloader erases 0x%06X..0x%06X - that is rootfs_data and nothing "
            "else. The kernel and the root filesystem are not in this plan, so the "
            "installed firmware is left exactly as it is." % (lo, hi),
            "What this does: %s." % what,
            "Over LAN, AUTOBURN cannot be changed: its default value is 1 and the "
            "AUTOBURN command is typed into the console, which needs a UART. So a "
            "dry run here sends NOTHING - it stops before the first transfer.",
        ], autoburn=False)

    pc_ip = args.pc_ip or _lan.require_router_net()
    print()
    print("Address of this computer on the router network: %s" % pc_ip)

    print()
    print("Every setting goes: the root password, the network configuration, Wi-Fi")
    print("names and keys, and any package installed into the overlay. The firmware")
    print("itself stays, and the router comes back on 192.168.1.1 with no password.")

    if args.dry_run:
        print()
        print("DRY RUN: nothing was written to flash. Drop --dry-run to erase the settings.")
        return 0

    if not _common.confirm("Erase all settings?", args.yes):
        print("Cancelled.")
        return 2

    print()
    _common.print_header("Step 2. Bootloader")
    _lan.guide_enter_bootloader(pc_ip, auto_yes=args.yes)

    print()
    _common.print_header("Step 3. Erasing")
    _lan.send_plan(plan)

    print()
    _common.print_header("Done")
    print("The settings partition has been erased. This section does not restart the")
    print("board: power it off and on yourself, this time WITHOUT holding any button.")
    print()
    print("The first boot takes about a minute longer than usual - the hook finishes")
    print("clearing rootfs_data. That is NOT a hang, and pulling the power during it")
    print("leaves the partition half erased.")
    print()
    print("Then: http://192.168.1.1 (LuCI) or ssh -o HostKeyAlgorithms=+ssh-rsa "
          "root@192.168.1.1, with no password set.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (_lan.LanError, _common.SafetyError, OSError, RuntimeError,
            ValueError, KeyError) as e:
        print()
        print("REFUSED: %s" % e)
        sys.exit(1)
