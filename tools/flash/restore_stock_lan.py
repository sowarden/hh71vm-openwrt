#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
restore_stock_lan.py - put your own stock firmware back on the HH71VM with NO UART
and no disassembly.

It takes the backup made by `install_openwrt_lan.py` (three partition dumps plus a
manifest), slices it into `r6cr` images and uploads them over TFTP to the bootloader
console, which the router reaches when a button is held while power is applied.

It also works when the system does not boot at all: the bootloader needs nothing but
power and a cable.

WHAT IS RESTORED AND WHAT IS NOT

    0x000000  128 KB  boot          bootloader       - never touched
    0x020000   16 KB  hwsetting     MAC addresses    - never touched
    0x024000   48 KB  config        stock MIB        - not touched: OpenWrt never spoiled it
    0x030000 2880 KB  kernel        RESTORED
    0x300000 4096 KB  rootfs        RESTORED
    0x700000 5120 KB  rootfs_data   RESTORED (on stock this is the tail of the squashfs)
    0xC00000 4096 KB  vendor_jffs2  not touched: our OpenWrt never writes there

ABOUT THE EXTRA ERASED SECTOR
    `flash_write` in the bootloader erases sectors by the formula
    nblocks = (dst+len)/erasesize - dst/erasesize + 1, so a write that ends exactly on
    a sector boundary erases one more sector BEYOND itself. The pieces go in
    ascending address order, so the extra erased sector is overwritten by the next
    piece every time. There is nothing to break the chain with at the end, so the
    last piece stops where an already erased area begins in the dump itself: the
    extra erase leaves it in exactly the state it was already in.

    On the verified dump the tail of the stock rootfs (0xBE0000..0xBFFFFF, 128 KB) is
    solid 0xFF and the chain ends there. The script looks for that boundary in YOUR
    dump rather than trusting one found in somebody else's; if there is no erased
    tail, it refuses to run and explains why.

EXAMPLES
    python tools/flash/restore_stock_lan.py --backup-dir backup-stock --dry-run
    python tools/flash/restore_stock_lan.py --backup-dir backup-stock
"""

import argparse
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import rtk_mkimg                                         # noqa: E402
import _common                                           # noqa: E402
import _lan                                              # noqa: E402

# Where everything comes from. The addresses are in flash; (file, offset in the file)
# is the backup. The stock layout: mtd0 = 0x000000..0x2FFFFF,
# mtd1 = 0x300000..0xBFFFFF, mtd2 = 0xC00000..0xFFFFFF.
STOCK_MAP = [
    ("mtd0", 0x000000, 0x300000),
    ("mtd1", 0x300000, 0x900000),
    ("mtd2", 0xC00000, 0x400000),
]

RESTORE_START = 0x030000     # the start of the kernel: anything below is none of our business
RESTORE_END = 0xC00000       # the end of rootfs_data; vendor_jffs2 is left alone

# The upper bound of the write. The factory image leaves the last 128 KB of the
# partition (0xBE0000..0xBFFFFF) erased, and the whole scheme leans on that clean
# tail: it is where the chain of extra erases ends, see the header of this file. The
# stock squashfs ends much earlier, at 0xA256BE, so that area does not belong to
# stock at all.
#
# WHY THE LIMIT EXISTS. This script does NOT write the tail, so after a rollback
# whatever was there before stays - jffs2 markers left by a removed OpenWrt, for
# instance. A backup taken from such a stock system no longer has a clean tail, and
# without the limit the script refused to accept it: stock could be restored only
# once. Now the plan is capped at this boundary, and what lies above is not restored,
# because there are no stock data there.
TAIL_GUARD = 0xBE0000
CHUNK = 0x200000             # 2 MiB per image: the same order of magnitude already
                             # proven live (2.6 MB), and progress stays visible


def parse_args():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--backup-dir", required=True,
                    help="the backup directory (with backup-manifest.json inside)")
    ap.add_argument("--pc-ip", default=None, type=_common.ipv4_arg)
    ap.add_argument("--dry-run", action="store_true",
                    help="build and show the plan, send nothing")
    ap.add_argument("--yes", action="store_true")
    return ap.parse_args()


def load_backup(outdir):
    mpath = os.path.join(outdir, "backup-manifest.json")
    if not os.path.exists(mpath):
        raise _lan.LanError("there is no backup-manifest.json in %s" % outdir)
    with open(mpath, encoding="utf-8") as f:
        manifest = json.load(f)

    by_dev = {p["dev"]: p for p in manifest["partitions"]}
    flash = bytearray(b"\xff" * 0x1000000)
    for dev, base, size in STOCK_MAP:
        p = by_dev.get(dev)
        if not p:
            raise _lan.LanError("the backup has no partition %s" % dev)
        with open(os.path.join(outdir, p["file"]), "rb") as f:
            data = f.read()
        if len(data) != size:
            raise _lan.LanError("%s: size %d, expected %d" % (p["file"], len(data), size))
        md5 = hashlib.md5(data).hexdigest()
        if md5 != p["md5"]:
            raise _lan.LanError("%s: md5 %s, the manifest says %s" % (p["file"], md5, p["md5"]))
        flash[base:base + size] = data
    if flash[0x7D60:0x7D62] != b"\x1f\x8b":
        raise _lan.LanError(
            "there is no gzip signature of the bootloader at 0x7D60 - this backup was "
            "not taken from a stock Realtek side")
    return bytes(flash), manifest


def build_plan(flash):
    end = _lan.last_used_address(flash, 0, RESTORE_START, RESTORE_END)

    # The tail beyond TAIL_GUARD does not belong to stock: it holds either the clean
    # 0xFF of the factory image or leftovers of firmware installed earlier. It is not
    # restored.
    capped = end > TAIL_GUARD
    if capped:
        end = TAIL_GUARD

    if end >= RESTORE_END:
        raise _lan.LanError(
            "your dump has no erased tail at the end of rootfs_data: the last sector "
            "before 0x%06X holds data.\n"
            "The write would then end exactly on the partition boundary and silently "
            "erase the first sector of vendor_jffs2 (0xC00000), which cannot be "
            "restored this same way without erasing the next one, and so on.\n"
            "For such a dump, use the path through the UART: restore_stock.py."
            % RESTORE_END)

    blobs = []
    chunks = _lan.chunks_for_range(flash, 0, RESTORE_START, end, CHUNK)
    for addr, body in chunks:
        # build_image appends two checksum bytes to the body, and they go into flash
        # as well: burn_image writes exactly len bytes of body. So every piece
        # occupies [addr, addr+len(body)+2). For every piece but the last those two
        # bytes land in the first sector of the next piece, which erases that sector
        # and overwrites it with its own data anyway.
        img = rtk_mkimg.build_image("r6cr", addr, body)
        blobs.append(("restore-0x%06X.img" % addr, img))
    plan = _common.build_bootloader_plan_blobs(blobs)

    # For the last piece there is nothing to overwrite those two bytes with. Make sure
    # they land in an area that is erased in the dump too: then the only difference
    # from the original is two bytes in a dead zone past the end of the stock
    # squashfs. When the plan was capped this check is unnecessary: the two checksum
    # bytes of the last piece land right after TAIL_GUARD, that is in the area we had
    # already decided not to restore. From there it is 128 KB to vendor_jffs2
    # (0xC00000).
    tail_lo, tail_hi = plan[-1]["flash_lo"], plan[-1]["flash_hi"]
    if not capped and tail_hi >= end:
        stray = flash[end:tail_hi + 1]
        if stray.count(0xFF) != len(stray):
            raise _lan.LanError(
                "the two checksum bytes of the last piece would land on "
                "0x%06X..0x%06X, and the dump holds data there. This dump cannot be "
                "written back byte for byte through the bootloader - use "
                "restore_stock.py (over the UART)." % (end, tail_hi))
    return plan, end, capped


def main():
    args = parse_args()

    _common.print_header("Step 1. The backup")
    flash, manifest = load_backup(args.backup_dir)
    print("Backup from %s, three partitions, md5 all match." % manifest.get("created", "?"))

    plan, end, capped = build_plan(flash)
    skipped = RESTORE_END - end
    print()
    print("Restoring 0x%06X..0x%06X (%d B)."
          % (RESTORE_START, end - 1, end - RESTORE_START))
    if capped:
        print("The tail 0x%06X..0x%06X (%d B) is not written: in this backup it is NOT"
              % (end, RESTORE_END - 1, skipped))
        print("clean - it holds data from firmware installed before stock. The stock")
        print("squashfs ends at 0xA256BE, so that area does not belong to stock.")
        print("The two checksum bytes of the last piece go there as well, into what we")
        print("were not restoring anyway. That leaves 128 KB before vendor_jffs2 (0xC00000).")
    else:
        print("The tail 0x%06X..0x%06X (%d B) is already erased in the dump - it is not"
              % (end, RESTORE_END - 1, skipped))
        print("written, and it is exactly where the chain of extra erases ends.")
        print("The first two bytes of that tail (0x%06X..0x%06X) take the checksum"
              % (end, end + 1))
        print("of the last piece: the dump holds 0xFF there, past the end of the stock squashfs.")

    _common.print_bootloader_plan(
        plan, args.dry_run, "Restore plan",
        extra_notes=[
            "Every section is r6cr: the header is not written to flash and there is no "
            "automatic restart. After the last piece, power the board off and on "
            "yourself.",
            "Over LAN, AUTOBURN cannot be changed (it is a console command, which "
            "needs a UART), so a dry run sends nothing.",
        ], autoburn=False)

    pc_ip = args.pc_ip or _lan.require_router_net()
    print()
    print("Address of this computer on the router network: %s" % pc_ip)

    if args.dry_run:
        print()
        print("DRY RUN: nothing was sent.")
        return 0

    if not _common.confirm("Restore stock? The current firmware will be overwritten.", args.yes):
        print("Cancelled.")
        return 2

    print()
    _common.print_header("Step 2. Bootloader")
    _lan.guide_enter_bootloader(pc_ip, auto_yes=args.yes)

    print()
    _common.print_header("Step 3. Writing")
    _lan.send_plan(plan)

    print()
    _common.print_header("Done")
    print("The stock kernel, rootfs and rootfs_data have been written. The bootloader,")
    print("the hwsetting area with the MAC addresses, config and vendor_jffs2 are untouched.")
    print()
    print("Power the device off and on again (this time do NOT hold the button).")
    print("It will simply boot the restored stock firmware.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (_lan.LanError, _common.SafetyError, OSError, RuntimeError,
            ValueError, KeyError) as e:
        print()
        print("REFUSED: %s" % e)
        sys.exit(1)
