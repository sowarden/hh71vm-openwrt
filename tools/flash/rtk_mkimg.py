#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rtk_mkimg.py - build and parse flashable images for the Realtek RTL8197F bootloader
(the `<RealTek>` console on the HH71VM Realtek side).

FORMAT (recovered by disassembling the bootloader):

    header (16 bytes, big-endian fields):
        sig[4] | startAddr(u32 BE) | burnAddr(u32 BE) | len(u32 BE)
    body (len bytes), whose last 1-2 bytes are a checksum chosen so that the whole
    sum (see below) comes out zero.

The signature decides:
    - how the checksum is computed (body only / header plus body / 8-bit);
    - whether the 16-byte header is written to flash along with the body, or only
      the body (values from the section table at 0x80012DEC, hard-coded as SECTIONS);
    - whether the bootloader reboots after flashing that section successfully.

WHY
    To build an image for a TFTP upload (`AUTOBURN`) without guessing on a live
    device. The correctness gate is the self-test below: a rebuilt stock kernel
    image (`cr6c`) has to match the flash contents byte for byte.

    The self-test needs stock dumps of your own device, which are not part of this
    repository; the `build` and `parse` subcommands do not.

EXAMPLES
    python rtk_mkimg.py selftest --dump0 mtdblock0.bin --kernel-payload kernel-payload.bin
    python rtk_mkimg.py build --sig r6cr --burn 0xB01000 --body payload.bin --out img.bin
    python rtk_mkimg.py parse img.bin
"""

import argparse
import struct
import sys

# --- section table (from the bootloader, 0x80012DEC) -----------------------
# checksum: 'body16'  - 16-bit BE sum of the body (len bytes) == 0
#           'all16'   - 16-bit BE sum of header plus body (len+16 bytes) == 0
#           'body8'   - 8-bit sum of the body == 0
# header_to_flash: True  - the 16-byte header and the body are both written
#                  False - only the body is written (the header in the file only
#                          exists so the bootloader can identify the section)

SECTIONS = {
    "cs6c": dict(desc="Linux kernel",              checksum="body16", header_to_flash=True,  reboot=True),
    "cr6c": dict(desc="Linux kernel (root-fs)",     checksum="body16", header_to_flash=True,  reboot=True),
    "w6cg": dict(desc="Webpages",                   checksum="body8",  header_to_flash=True,  reboot=False),
    "r6cr": dict(desc="Root filesystem",             checksum="body16", header_to_flash=False, reboot=False),
    "boot": dict(desc="Boot code",                   checksum="body16", header_to_flash=False, reboot=True),
    "ALL1": dict(desc="Total Image",                 checksum="all16",  header_to_flash=False, reboot=True),
    "ALL2": dict(desc="Total Image (no check)",       checksum="all16",  header_to_flash=False, reboot=True),
}

DEFAULT_START_ADDR = {
    "cs6c": 0x80A00000,
    "cr6c": 0x80A00000,
}

# --- header ------------------------------------------------------------

def pack_header(sig, start_addr, burn_addr, length):
    if len(sig) != 4:
        raise ValueError("the signature must be exactly 4 bytes: %r" % sig)
    return sig.encode("ascii") + struct.pack(">III", start_addr, burn_addr, length)


def unpack_header(data):
    if len(data) < 16:
        raise ValueError("at least 16 header bytes are needed, got %d" % len(data))
    sig = data[:4].decode("ascii", errors="replace")
    start_addr, burn_addr, length = struct.unpack(">III", data[4:16])
    return dict(sig=sig, start_addr=start_addr, burn_addr=burn_addr, length=length)


# --- checksum -----------------------------------------------------------

def sum16_be(data):
    """16-bit BE sum over words; an odd trailing byte is ignored (the bootloader
    does the same - its loop steps by two up to a truncated len)."""
    total = 0
    n = len(data) - (len(data) % 2)
    for i in range(0, n, 2):
        total = (total + struct.unpack(">H", data[i:i + 2])[0]) & 0xFFFF
    return total


def sum8(data):
    return sum(data) & 0xFF


def append_checksum(body_core, mode, header_prefix=b""):
    """Append checksum bytes to body_core so that the checksum (in the given mode)
    becomes zero. header_prefix takes part in the sum only for mode='all16', and is
    used only when the final header is known in advance - see build_image."""
    if mode == "body16":
        if len(body_core) % 2 != 0:
            raise ValueError(
                "body16: the body before the checksum must have an even length "
                "(got %d bytes), otherwise the last byte falls out of the sum" % len(body_core))
        need = (0x10000 - sum16_be(body_core)) & 0xFFFF
        return body_core + struct.pack(">H", need)
    if mode == "body8":
        need = (0x100 - sum8(body_core)) & 0xFF
        return body_core + bytes([need])
    if mode == "all16":
        if len(body_core) % 2 != 0:
            raise ValueError("all16: the body before the checksum must have an even length")
        base = sum16_be(header_prefix) + sum16_be(body_core)
        need = (0x10000 - (base & 0xFFFF)) & 0xFFFF
        return body_core + struct.pack(">H", need)
    raise ValueError("unknown checksum mode: %r" % mode)


def verify_checksum(body, mode, header_prefix=b""):
    if mode == "body16":
        return sum16_be(body) == 0
    if mode == "body8":
        return sum8(body) == 0
    if mode == "all16":
        return (sum16_be(header_prefix) + sum16_be(body)) & 0xFFFF == 0
    raise ValueError("unknown checksum mode: %r" % mode)


# --- build / parse an image ----------------------------------------------

def build_image(sig, burn_addr, body_core, start_addr=None, checksum_already_present=False):
    """Return the bytes of a finished file (header plus body) for a TFTP upload.

    body_core       - the payload WITHOUT checksum bytes (the usual case), or WITH
                       checksum bytes already in place if
                       checksum_already_present=True (they are then left alone and
                       only verified).
    """
    if sig not in SECTIONS:
        raise ValueError("unknown signature %r, known ones: %s" % (sig, list(SECTIONS)))
    info = SECTIONS[sig]
    if start_addr is None:
        start_addr = DEFAULT_START_ADDR.get(sig, 0)

    if info["checksum"] == "all16":
        # the final length is known in advance (the checksum adds two bytes), so
        # the header can be computed once and included in the sum
        if checksum_already_present:
            body_final = body_core
        else:
            final_len = len(body_core) + 2
            header = pack_header(sig, start_addr, burn_addr, final_len)
            body_final = append_checksum(body_core, "all16", header_prefix=header)
        header = pack_header(sig, start_addr, burn_addr, len(body_final))
        return header + body_final

    if checksum_already_present:
        body_final = body_core
    else:
        body_final = append_checksum(body_core, info["checksum"])
    header = pack_header(sig, start_addr, burn_addr, len(body_final))
    return header + body_final


def parse_image(data, verify=True):
    hdr = unpack_header(data)
    sig = hdr["sig"]
    body = data[16:16 + hdr["length"]]
    if len(body) != hdr["length"]:
        raise ValueError("the file is shorter than the header says: %d body bytes needed, %d present"
                          % (hdr["length"], len(body)))
    result = dict(hdr)
    result["body"] = body
    result["known_section"] = sig in SECTIONS
    if sig in SECTIONS:
        info = SECTIONS[sig]
        result["desc"] = info["desc"]
        result["reboot_after"] = info["reboot"]
        result["header_written_to_flash"] = info["header_to_flash"]
        if verify:
            header_prefix = data[:16] if info["checksum"] == "all16" else b""
            result["checksum_ok"] = verify_checksum(body, info["checksum"], header_prefix)
    return result


# --- self-test: rebuild the stock kernel image ----------------------------

def selftest(dump0_path, payload_path):
    print("=== self-test: rebuilding the stock kernel image (cr6c) ===")
    with open(dump0_path, "rb") as f:
        flash = f.read()
    header_flash = flash[0x30000:0x30010]
    hdr = unpack_header(header_flash)
    print("header from flash: sig=%r start=0x%08X burn=0x%08X len=0x%X"
          % (hdr["sig"], hdr["start_addr"], hdr["burn_addr"], hdr["length"]))
    assert hdr["sig"] == "cr6c", "unexpected signature in the dump: %r" % hdr["sig"]

    body_flash = flash[0x30010:0x30010 + hdr["length"]]

    with open(payload_path, "rb") as f:
        payload = f.read()
    assert payload == body_flash, (
        "kernel-payload.bin (%d bytes) does not match the body from flash (%d bytes) - "
        "check the path and whether the extract is current" % (len(payload), len(body_flash)))

    # 1) check the checksum itself (independent of build_image)
    ok = verify_checksum(body_flash, "body16")
    print("the checksum of the body from flash comes out zero: %s" % ok)
    assert ok, "the stock body checksum does not come out zero - rethink the algorithm"

    # 2) the real test of build_image: cut off the last two bytes (the checksum),
    #    recompute them and compare with the original byte for byte
    body_core = body_flash[:-2]
    original_checksum_bytes = body_flash[-2:]
    rebuilt_body = append_checksum(body_core, "body16")
    rebuilt_checksum_bytes = rebuilt_body[-2:]
    print("checksum bytes in the original: %s, recomputed: %s"
          % (original_checksum_bytes.hex(), rebuilt_checksum_bytes.hex()))
    assert rebuilt_checksum_bytes == original_checksum_bytes, (
        "append_checksum produces different bytes than the stock image - the sum "
        "algorithm is wrong or ambiguous (different byte pairs may sum to zero)")

    # 3) a full build through build_image and a byte-for-byte comparison with flash
    full = build_image("cr6c", hdr["burn_addr"], body_core,
                        start_addr=hdr["start_addr"])
    expected = header_flash + body_flash
    assert full == expected, "the rebuilt image does not match flash byte for byte!"
    print("EXACT BYTE-FOR-BYTE MATCH: the rebuilt image (%d bytes) == flash 0x30000..0x%X"
          % (len(full), 0x30000 + len(full)))
    print("=== self-test PASSED ===")
    return True


# --- CLI ------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("selftest", help="rebuild the stock cr6c and compare it with flash")
    p.add_argument("--dump0", required=True, help="complete stock mtdblock0 dump")
    p.add_argument("--kernel-payload", required=True,
                   help="extracted stock kernel body including its checksum")

    p = sub.add_parser("build", help="build an image for upload")
    p.add_argument("--sig", required=True, choices=list(SECTIONS))
    p.add_argument("--burn", required=True, help="burnAddr, hex (for example 0xB01000)")
    p.add_argument("--start", default=None, help="startAddr, hex (default 0)")
    p.add_argument("--body", required=True, help="file holding the payload (without a checksum)")
    p.add_argument("--body-has-checksum", action="store_true",
                    help="the body already carries checksum bytes - leave them alone, only verify")
    p.add_argument("--out", required=True)

    p = sub.add_parser("parse", help="parse a finished image")
    p.add_argument("image")

    args = ap.parse_args()

    if args.cmd == "selftest":
        ok = selftest(args.dump0, args.kernel_payload)
        sys.exit(0 if ok else 1)

    elif args.cmd == "build":
        with open(args.body, "rb") as f:
            body_core = f.read()
        burn_addr = int(args.burn, 0)
        start_addr = int(args.start, 0) if args.start is not None else None
        img = build_image(args.sig, burn_addr, body_core, start_addr=start_addr,
                          checksum_already_present=args.body_has_checksum)
        with open(args.out, "wb") as f:
            f.write(img)
        info = SECTIONS[args.sig]
        print("built: %s (%s), burnAddr=0x%X, body=%d bytes, file=%d bytes"
              % (args.sig, info["desc"], burn_addr, len(body_core), len(img)))
        print("what reaches flash: %s, automatic reboot afterwards: %s"
              % ("header and body" if info["header_to_flash"] else "body only",
                 info["reboot"]))
        print("-> %s" % args.out)

    elif args.cmd == "parse":
        with open(args.image, "rb") as f:
            data = f.read()
        r = parse_image(data)
        print("sig=%r  known=%s" % (r["sig"], r["known_section"]))
        print("startAddr=0x%08X  burnAddr=0x%08X  len=0x%X (%d)"
              % (r["start_addr"], r["burn_addr"], r["length"], r["length"]))
        if r["known_section"]:
            print("description: %s" % r["desc"])
            print("what reaches flash: %s"
                  % ("header and body" if r["header_written_to_flash"] else "body only"))
            print("automatic reboot after flashing: %s" % r["reboot_after"])
            print("checksum comes out zero: %s" % r["checksum_ok"])


if __name__ == "__main__":
    main()
