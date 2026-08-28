#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mkrtkimg.py - build flashable Realtek RTL8197F images in the OpenWrt tree.

The format was derived from the device boot code and the stock `/bin/fwupg`
utility, then confirmed against images accepted by both consumers:
the ROM loader and the stock upgrader.

    header 16 bytes, fields big-endian:
        sig[4] | startAddr(u32) | burnAddr(u32) | len(u32)
    body of length len; the final two bytes make the sum of its
    16-bit big-endian words equal zero.

Both consumers of the image do the same check:
  * the ROM loader (`AUTOBURN`, function burn_image);
  * stock `/bin/fwupg` on the running vendor system (function 0x2ef0).
    Its `check` command accepts images produced by this tool.

Relevant signatures:
    cr6c / cs6c - kernel; the header and body are both written to flash;
    r6cr        - root filesystem; `fwupg` writes only the body at offset zero
                  in the rootfs partition and ignores burnAddr.

Subcommands:
    build      --sig --start --burn --body --out
    pad        --file --align [--out]
    concat     --out file...
    checksize  --file --max
"""
import argparse
import struct
import sys

BODY16 = ("cr6c", "cs6c", "r6cr", "boot")
BODY8 = ("w6cg",)


def sum16_be(data):
    total = 0
    for i in range(0, len(data) - 1, 2):
        total = (total + struct.unpack_from(">H", data, i)[0]) & 0xFFFF
    if len(data) & 1:
        total = (total + (data[-1] << 8)) & 0xFFFF
    return total


# The loader takes the rootfs image length from squashfs rather than the section header.
# It reads the 32-bit word at offset 8, byte-swaps it, adds SQFS_SUPER + CKSUM,
# and checksums that many bytes. Squashfs 4.0 normally stores `mkfs_time` there,
# but the vendor replaces it with the byte-swapped image length.
# Without this stamp the loader derives a bogus length (about 2.3 GiB in one build),
# checksums beyond the stored image, and never starts the kernel.
# Stock-image proof: word@8 = 0x00725D80 for a body length of 0x726002.
SQFS_SUPER = 640


def stamp_rootfs_len(body):
    """Stamp squashfs with the length expected by the bootloader."""
    if body[:4] not in (b"hsqs", b"sqsh"):
        sys.exit("mkrtkimg: r6cr body is not squashfs - check the build")
    return body[:8] + struct.pack(">I", len(body) - SQFS_SUPER) + body[12:]


def append_checksum(body, sig):
    """Append checksum bytes that make the body sum equal zero."""
    if sig in BODY8:
        pad = (-sum(body)) & 0xFF
        return body + bytes([pad])
    if len(body) & 1:                      # summed as 16-bit words
        body += b"\x00"
    if sig == "r6cr":
        body = stamp_rootfs_len(body)
    need = (-sum16_be(body)) & 0xFFFF
    return body + struct.pack(">H", need)


def cmd_stamp(a):
    """Prepare a rootfs body with its length stamp and checksum bytes.

    The bootloader applies the same rootfs check regardless of how
    the partition was written, so both the vendor r6cr section and the
    sysupgrade image need the same prepared body.
    """
    body = append_checksum(open(a.file, "rb").read(), "r6cr")
    open(a.out, "wb").write(body)
    print("mkrtkimg: %s stamped, body=%d" % (a.out, len(body)))


def cmd_build(a):
    body = open(a.body, "rb").read()
    if not a.body_final:
        body = append_checksum(body, a.sig)
    header = a.sig.encode("ascii") + struct.pack(">III", a.start, a.burn, len(body))
    blob = header + body
    if sum16_be(body) != 0 and a.sig in BODY16:
        sys.exit("mkrtkimg: checksum mismatch - image construction failed")
    open(a.out, "wb").write(blob)
    print("mkrtkimg: %s sig=%s start=0x%08x burn=0x%08x body=%d total=%d"
          % (a.out, a.sig, a.start, a.burn, len(body), len(blob)))


def cmd_pad(a):
    data = open(a.file, "rb").read()
    if len(data) % a.align:
        data += b"\xff" * (a.align - len(data) % a.align)
    open(a.out or a.file, "wb").write(data)
    print("mkrtkimg: %s aligned to %d bytes" % (a.out or a.file, len(data)))


def cmd_concat(a):
    with open(a.out, "wb") as out:
        for name in a.files:
            out.write(open(name, "rb").read())
    print("mkrtkimg: %s assembled from %d parts" % (a.out, len(a.files)))


def cmd_checksize(a):
    size = len(open(a.file, "rb").read())
    if size > a.max:
        sys.exit("mkrtkimg: %s is %d bytes, but the partition holds %d"
                 % (a.file, size, a.max))
    print("mkrtkimg: %s = %d bytes, headroom %d" % (a.file, size, a.max - size))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("build")
    p.add_argument("--sig", required=True, choices=sorted(set(BODY16) | set(BODY8)))
    p.add_argument("--start", required=True, type=lambda s: int(s, 0))
    p.add_argument("--burn", required=True, type=lambda s: int(s, 0))
    p.add_argument("--body", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--body-final", action="store_true",
                   help="the body already contains its stamp and checksum bytes")
    p.set_defaults(func=cmd_build)

    p = sub.add_parser("stamp")
    p.add_argument("--file", required=True)
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_stamp)

    p = sub.add_parser("pad")
    p.add_argument("--file", required=True)
    p.add_argument("--align", type=lambda s: int(s, 0), default=4096)
    p.add_argument("--out")
    p.set_defaults(func=cmd_pad)

    p = sub.add_parser("concat")
    p.add_argument("--out", required=True)
    p.add_argument("files", nargs="+")
    p.set_defaults(func=cmd_concat)

    p = sub.add_parser("checksize")
    p.add_argument("--file", required=True)
    p.add_argument("--max", required=True, type=lambda s: int(s, 0))
    p.set_defaults(func=cmd_checksize)

    a = ap.parse_args()
    a.func(a)


if __name__ == "__main__":
    main()
