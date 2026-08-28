#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
install_openwrt_lan.py - install our OpenWrt on the HH71VM with NO UART and no
disassembly.

What it does, step by step:

  1. Checks the image (the `*-fwupg.bin` container) before touching the router at
     all: section signatures, checksums, and whether anything lands in a forbidden
     area of flash.
  2. Takes a full backup of the stock firmware (16 MiB, three partitions) from the
     RUNNING stock system over telnet plus TFTP, the same way `tftp_dump_mtd.py`
     does. Without that copy there is nothing to put your own stock firmware back
     from.
  3. Asks you to put the router into flashing mode with the button (WPS held while
     power is applied) and confirms over ARP that the bootloader came up.
  4. Uploads the sections over TFTP: the root filesystem first, the kernel last (the
     kernel restarts the board as soon as it has been written).

Why no UART is needed: the start-up path of the bootloader is analysed in the header
of `_lan.py`.

The backup (step 2) is the only step that needs access to the stock system: telnet on
192.168.1.1, port 2323 by default (`--telnet-port`). If there is no access, the step can
be skipped (`--skip-backup`), but
then going back to YOUR OWN stock firmware becomes impossible, and the script asks
you to confirm that in words.

The first boot after installation takes about two minutes: a preinit hook erases
`rootfs_data`, where pieces of the stock root filesystem are left. That is not a
hang, and power must not be removed while it happens.

EXAMPLES
    python tools/flash/install_openwrt_lan.py --image firmware/...-hh71vm-fwupg.bin --backup-dir backup-stock
    python tools/flash/install_openwrt_lan.py --image firmware/... --backup-dir ... --dry-run
"""

import argparse
import hashlib
import json
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import _common                                           # noqa: E402
import _lan                                              # noqa: E402
import tftp_dump_mtd                                     # noqa: E402

# The stock partition layout: device name, size, file label.
# It matches tftp_dump_mtd.PARTITIONS and is repeated here so that the script checks
# what it actually saw on the device rather than what it imported.
STOCK_PARTITIONS = [
    ("mtd0", 0x300000, "boot_cfg_linux"),
    ("mtd1", 0x900000, "rootfs"),
    ("mtd2", 0x400000, "jffs2"),
]

BACKUP_MANIFEST = "backup-manifest.json"


def tcp_port(value):
    port = int(value)
    if not 1 <= port <= 65535:
        raise argparse.ArgumentTypeError("port must be in the range 1..65535")
    return port


def parse_args():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--image", required=True,
                    help="the *-hh71vm-fwupg.bin container of our build")
    ap.add_argument("--backup-dir", default=None,
                    help="where to put the stock backup (default: "
                         "backup-stock-YYYYMMDD-HHMMSS next to the script)")
    ap.add_argument("--skip-backup", action="store_true",
                    help="do not take a stock backup (going back to your own stock "
                         "firmware then becomes impossible)")
    ap.add_argument("--pc-ip", default=None, type=_common.ipv4_arg,
                    help="address of this computer on the router network; detected "
                         "automatically by default")
    ap.add_argument("--telnet-port", type=tcp_port, default=2323,
                    help="temporary stock Telnet port (default: %(default)s; use 23 "
                         "for the marker-only firmware variant)")
    ap.add_argument("--dry-run", action="store_true",
                    help="check the image and take the backup, but write nothing to flash")
    ap.add_argument("--yes", action="store_true", help="do not ask for confirmations")
    return ap.parse_args()


# --- the stock backup ---------------------------------------------------

def check_stock_layout(tc):
    """Make sure the far end really is STOCK and not OpenWrt already.

    Stock has three partitions (3+9+4 MiB), our system has seven. Mixing them up
    would be bad: a "stock" dump taken from OpenWrt looks like an ordinary file and
    silently makes the whole exercise worthless."""
    out = tc.cmd("cat /proc/mtd").decode("latin1", "replace")
    entries = []
    for line in out.splitlines():
        match = re.match(r'^(mtd\d+):\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+"([^"]+)"',
                         line.strip())
        if match:
            entries.append((match.group(1), int(match.group(2), 16), match.group(3)))
    expected = [(dev, size) for dev, size, _label in STOCK_PARTITIONS]
    actual = [(dev, size) for dev, size, _name in entries]
    if actual != expected:
        raise _lan.LanError(
            "the flash layout on 192.168.1.1 is not the expected stock 3+9+4 MiB "
            "layout: /proc/mtd reports %s. It looks like different firmware is "
            "installed there, and a stock backup cannot be taken from it."
            % (actual or "no parseable partitions",))
    names = [name for _dev, _size, name in entries]
    print("   /proc/mtd: %s - the stock layout" % ", ".join(names))
    return names


def make_backup(outdir, pc_ip, telnet_port):
    os.makedirs(outdir, exist_ok=True)
    print()
    _common.print_header("Step 2. Stock backup (16 MiB)")
    print("The router has to be running stock and answering telnet on %s:%d."
          % (_lan.STOCK_IP, telnet_port))
    print("The data travels over a separate TFTP connection, not through the telnet")
    print("session: binary data through a tty arrives corrupted (OPOST/XTABS).")
    print()

    tc = tftp_dump_mtd.TelnetControl(port=telnet_port)
    try:
        check_stock_layout(tc)
        manifest = {"created": time.strftime("%Y-%m-%d %H:%M:%S"),
                    "source": "stock via telnet+tftp", "partitions": []}
        for dev, size, label in STOCK_PARTITIONS:
            path, md5, unstable = tftp_dump_mtd.dump_partition(
                tc, dev, size, label, outdir, pc_ip)
            entry = {"dev": dev, "label": label, "size": size,
                     "file": os.path.basename(path), "md5": md5}
            # A mounted jffs2 rewrites itself, so its copy is a snapshot rather than
            # an exact image of the partition. That is recorded in the manifest: it
            # does not affect the rollback (restore_stock_lan.py never writes mtd2,
            # RESTORE_END = 0xC00000), but it is worth knowing the copy is inexact.
            if unstable:
                entry["unstable"] = True
            manifest["partitions"].append(entry)
    finally:
        tc.close()

    with open(os.path.join(outdir, BACKUP_MANIFEST), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1, ensure_ascii=False)
    print()
    print("The backup is ready: %s" % outdir)
    for p in manifest["partitions"]:
        mark = "  SNAPSHOT (the partition changed while being read)" if p.get("unstable") else ""
        print("   %-28s %8d B  md5 %s%s" % (p["file"], p["size"], p["md5"], mark))
    return manifest


def verify_backup(outdir):
    """Check a backup that has already been taken: sizes, md5, and that it really is
    stock."""
    mpath = os.path.join(outdir, BACKUP_MANIFEST)
    if not os.path.exists(mpath):
        raise _lan.LanError("%s has no %s - this is not a backup directory"
                            % (outdir, BACKUP_MANIFEST))
    with open(mpath, encoding="utf-8") as f:
        manifest = json.load(f)
    for p in manifest["partitions"]:
        path = os.path.join(outdir, p["file"])
        with open(path, "rb") as f:
            data = f.read()
        if len(data) != p["size"]:
            raise _lan.LanError("%s: size %d, the manifest says %d"
                                % (path, len(data), p["size"]))
        md5 = hashlib.md5(data).hexdigest()
        if md5 != p["md5"]:
            raise _lan.LanError("%s: md5 %s, the manifest says %s" % (path, md5, p["md5"]))
    # the stock bootloader keeps a decompressor stub at the start of mtd0 and the
    # gzip body at 0x7D60; that is what is checked, not merely "the file is not empty"
    with open(os.path.join(outdir, manifest["partitions"][0]["file"]), "rb") as f:
        mtd0 = f.read()
    if mtd0[0x7D60:0x7D62] != b"\x1f\x8b":
        raise _lan.LanError(
            "the start of the mtd0 dump has no gzip signature of the bootloader at "
            "0x7D60 - this is not an image of a stock Realtek side")
    print("The backup checks out: sizes and md5 match, the bootloader is in place.")
    return manifest


# --- the write plan -----------------------------------------------------

def build_plan(image_path):
    with open(image_path, "rb") as f:
        data = f.read()
    blobs = _common.split_container(data, os.path.splitext(os.path.basename(image_path))[0])
    blobs, reordered = _common.order_reboot_last(blobs)
    plan = _common.build_bootloader_plan_blobs(blobs)
    return plan, reordered


def main():
    args = parse_args()

    _common.print_header("Step 1. Checking the image")
    plan, reordered = build_plan(args.image)
    if reordered:
        print("The section order was changed: the section with the automatic restart "
              "was moved last.")
    _common.print_bootloader_plan(
        plan, args.dry_run, "Write plan",
        extra_notes=[
            "Over LAN, AUTOBURN cannot be changed: its default value is 1 and the "
            "AUTOBURN command is typed into the console, which needs a UART. So a "
            "dry run here sends NOTHING - it stops before the first transfer.",
        ], autoburn=False)

    pc_ip = args.pc_ip or _lan.require_router_net()
    print()
    print("Address of this computer on the router network: %s" % pc_ip)

    backup_dir = args.backup_dir or os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "backup-stock-%s" % time.strftime("%Y%m%d-%H%M%S"))

    if args.skip_backup:
        print()
        print("!!! No stock backup is being taken (--skip-backup).")
        print("!!! There will then be nothing to put YOUR OWN stock firmware back from:")
        print("!!! another dump carries other MAC addresses, region and carrier settings.")
        if not args.yes:
            got = input('Type exactly "i already have a backup" to go on: ')
            if got.strip() != "i already have a backup":
                print("Cancelled.")
                return 2
    elif os.path.exists(os.path.join(backup_dir, BACKUP_MANIFEST)):
        print()
        _common.print_header("Step 2. Stock backup - already present")
        verify_backup(backup_dir)
    else:
        make_backup(backup_dir, pc_ip, args.telnet_port)
        verify_backup(backup_dir)

    if args.dry_run:
        print()
        print("DRY RUN: nothing was written to flash. Drop --dry-run to install the "
              "firmware.")
        return 0

    if not args.yes:
        if not _common.confirm("Write OpenWrt to flash?", args.yes):
            print("Cancelled.")
            return 2

    print()
    _common.print_header("Step 3. Bootloader")
    _lan.guide_enter_bootloader(pc_ip, auto_yes=args.yes)

    print()
    _common.print_header("Step 4. Writing")
    _lan.send_plan(plan)

    print()
    _common.print_header("Done")
    print("The board restarted by itself after the kernel was written.")
    print()
    print("The first boot takes about two minutes: a preinit hook erases rootfs_data,")
    print("where pieces of the stock root filesystem are left. That is NOT a hang -")
    print("power must not be removed now, or the partition stays half erased.")
    print()
    print("Next: http://192.168.1.1 (LuCI) or ssh -o HostKeyAlgorithms=+ssh-rsa root@192.168.1.1.")
    if not args.skip_backup:
        print("The stock backup for going back: %s" % backup_dir)
        print("To go back: python tools/flash/restore_stock_lan.py --backup-dir %s"
              % backup_dir)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (_lan.LanError, _common.SafetyError, OSError, RuntimeError,
            ValueError, KeyError) as e:
        print()
        print("REFUSED: %s" % e)
        sys.exit(1)
