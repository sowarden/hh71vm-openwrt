#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
_lan.py - the shared part of the paths that need NO UART: entering the bootloader
with a button, and uploading over TFTP.

HOW WE KNOW UART IS NOT NEEDED
    The start-up path of the stock bootloader was disassembled (from a dump of the
    boot code, base 0x80000000):

      0x8000C080  "boot, or drop into the console"
                    -> 0x8000C030 "is there a reason not to boot"
                          jal 0x8000BB40 (a0=0x1B)  ESC in the UART receiver
                          jal 0x8000BB8C (a0=-1)    PEFGH_DAT (0xB8003528) bit 22 == 0
                          jal 0x8000BBD4            PEFGH_DAT bit 24 == 0
                    any of the three -> prints "---Escape booting by user---"
                                        and calls 0x8000BFFC

      0x8000BFFC  jal 0x8000BB40 (a0=0x6D 'm')   -- skip if there is an 'm' in the UART
                  jal 0x8000BF54 (a0=1, a1=0)    -- ETHERNET INITIALISATION
                  jal 0x8000CACC                 -- the console command loop

    So the bootloader brings the network up BY ITSELF as soon as it enters the
    console - the `ETH` command does not have to be typed. `ETH` unconditionally
    sets the address to 192.168.1.6, `AUTOBURN` defaults to 1 and `LOADADDR`
    defaults to 0xA0A00000. That means a single TFTP put of an image with a valid
    header already writes it to flash.

    The PEFGH bits: bit N is GPIO 32+N. Bit 22 is GPIO 54 (port G, bit 6) and bit 24
    is GPIO 56 (port H, bit 0). In the vendor BSP those exact pins are named
    `BSP_RESET_BTN_PIN = G6` and `BSP_WPS_BTN_PIN = H0`. Holding the button while
    power is applied gives the console; the check is active-low.

WHAT THIS PATH CANNOT DO
    Only WRITE. Flash cannot be read through the bootloader without a UART: reading
    uses the `FLR`/`DW` commands and they have to be typed into the console. That is
    why the stock backup is taken from the running system (see `tftp_dump_mtd.py`)
    rather than from here.

REQUIREMENTS ON THE COMPUTER
    - an address on the 192.168.1.0/24 network (after ETH the bootloader is always
      192.168.1.6);
    - a host firewall rule for inbound UDP for that very `python.exe`: TFTP replies
      come not from port 69 but from an increasing TID (2098, 2099, ...).
"""

import os
import re
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import rtk_tftp_put                                      # noqa: E402
import _common                                           # noqa: E402

BOOTLOADER_IP = "192.168.1.6"
STOCK_IP = "192.168.1.1"


class LanError(RuntimeError):
    pass


# --- where we are on the network ----------------------------------------

def local_ipv4_addresses():
    """Every IPv4 address of this computer. Through socket, without parsing the
    output of ipconfig: interface names are localised and differ between systems."""
    out = set()
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            out.add(info[4][0])
    except socket.gaierror:
        pass
    # the address that would be used towards the router: this also covers the case
    # where the hostname does not resolve
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((BOOTLOADER_IP, 9))
        out.add(s.getsockname()[0])
    except OSError:
        pass
    finally:
        s.close()
    return sorted(out)


def pc_ip_on_router_net(addresses=None):
    """The address of the computer on the router network, or None."""
    for a in (addresses or local_ipv4_addresses()):
        if a.startswith("192.168.1."):
            return a
    return None


def require_router_net(pc_ip=None):
    ip = pc_ip or pc_ip_on_router_net()
    if ip:
        return ip
    raise LanError(
        "this computer has no address on the 192.168.1.0/24 network, and after it "
        "brings the network up the bootloader console always sits on 192.168.1.6.\n"
        "Give the wired interface a static address such as 192.168.1.50/24 and run "
        "this again.\n"
        "Addresses found: %s" % (", ".join(local_ipv4_addresses()) or "none"))


# --- is the bootloader alive --------------------------------------------

_MAC_RE = re.compile(r"([0-9a-fA-F]{2}[-:]){5}[0-9a-fA-F]{2}")


def arp_lookup(ip):
    """The MAC for ip from the ARP cache of the operating system, or None.

    ARP rather than ping: the bootloader has no ICMP handler (confirmed) and stays
    silent on a ping, but it does answer an ARP request. The ping is sent anyway -
    it is what makes the system issue that ARP request."""
    try:
        subprocess.run(["ping", "-n" if os.name == "nt" else "-c", "1",
                        "-w" if os.name == "nt" else "-W",
                        "500" if os.name == "nt" else "1", ip],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=6)
    except (OSError, subprocess.SubprocessError):
        pass
    try:
        res = subprocess.run(["arp", "-a", ip] if os.name == "nt" else ["arp", "-n", ip],
                             capture_output=True, timeout=6)
    except (OSError, subprocess.SubprocessError):
        return None
    text = (res.stdout or b"").decode("latin1", "replace")
    for line in text.splitlines():
        if ip in line:
            m = _MAC_RE.search(line)
            if m and m.group(0).lower() not in ("00-00-00-00-00-00", "00:00:00:00:00:00"):
                return m.group(0)
    return None


def wait_for_bootloader(ip=BOOTLOADER_IP, timeout=120.0, quiet=False):
    """Wait until ip starts answering ARP. Returns the MAC."""
    deadline = time.time() + timeout
    last = 0
    while time.time() < deadline:
        mac = arp_lookup(ip)
        if mac:
            return mac
        if not quiet and time.time() - last > 5:
            last = time.time()
            print("   ... %s is not answering ARP yet, waiting (%d s left)"
                  % (ip, int(deadline - time.time())))
        time.sleep(1.0)
    raise LanError(
        "%s did not answer ARP within %.0f s.\n"
        "Check: the cable is in a LAN port of the router; the computer has a "
        "192.168.1.x address; the button was held down BEFORE power was applied and "
        "kept held for several seconds afterwards; the power really was removed "
        "rather than the device merely restarted."
        % (ip, timeout))


BUTTON_HELP = """\
How to put the router into flashing mode (no UART needed):

  1. Unplug power from the router.
  2. Connect a LAN port of the router to the computer with a cable.
     The computer address is %s.
  3. Press and hold the WPS button.
  4. While still holding it, plug the power back in.
  5. Keep holding the button for about 12 more seconds, then release it.

The bootloader then does not boot the system, brings the network up by itself and
listens for TFTP on 192.168.1.6. There may be no indication of this at all - we
check over ARP."""


def guide_enter_bootloader(pc_ip, auto_yes=False, timeout=180.0):
    print()
    print(BUTTON_HELP % pc_ip)
    print()
    if not auto_yes:
        try:
            input("Do that, then press Enter... ")
        except EOFError:
            pass
    print("Looking for the bootloader at %s..." % BOOTLOADER_IP)
    mac = wait_for_bootloader(BOOTLOADER_IP, timeout=timeout)
    print("The bootloader answers, MAC %s" % mac)
    return mac


# --- uploading the plan over TFTP ---------------------------------------

# The speed at which the bootloader writes flash, measured live: a 2 859 010 byte
# r6cr section took about 39 seconds, that is roughly 74 KB/s. The figure below is
# deliberately lower - better to wait too long than to arrive at a busy bootloader.
FLASH_WRITE_BPS = 60000.0

# How much longer to be patient beyond that estimate if the write drags on. The
# attempts are deliberately infrequent: the bootloader is single-threaded, and every
# WRQ that arrives during a write piles up in the buffer and resets its receive
# state once it gets to them.
WRQ_WAIT_TIMEOUT = 5.0
WRQ_WAIT_RETRIES = 24


def flash_write_seconds(length):
    """An estimate of how long writing a section to flash takes after it has been
    received over TFTP."""
    return length / FLASH_WRITE_BPS


def send_plan(plan, host=BOOTLOADER_IP, pause=1.5):
    """Upload every item of the plan with one TFTP put. The order of the plan has
    already been checked by build_bootloader_plan_blobs (a section with an automatic
    restart has to be last).

    AUTOBURN is ALWAYS 1 here: that is its default value, and changing it without a
    console (that is, without a UART) is impossible. So there is no dry run of the
    transfer itself on this path - either nothing is sent, or it is written.

    A pause for the flash write between sections is mandatory. The bootloader is
    single-threaded: once it has accepted a section it goes away to write, answers
    nothing at all meanwhile, and prints the WRQs that arrived in one burst after
    `Flash Write Successed!`. With a UART that is visible (`send_image_via_tftp`
    waits for the line to go quiet); here there is nothing to watch, so we wait by
    the estimate and make up the rest with patience on the first WRQ of the next
    section."""
    results = []
    waited = False
    for i, item in enumerate(plan, 1):
        name = os.path.basename(item["path"])
        print()
        print("[%d/%d] %s -> 0x%06X, %d bytes"
              % (i, len(plan), name, item["burn_addr"], item["length"]))
        # Patience on the WRQ is only needed for sections that follow a write to
        # flash: before the first one the bootloader is certainly free.
        kw = {}
        if waited:
            kw = {"wrq_timeout": WRQ_WAIT_TIMEOUT, "wrq_retries": WRQ_WAIT_RETRIES}
        stats = rtk_tftp_put.put(item["data"], host=host, remote_name=name, **kw)
        print("      sent %d B in %.2f s (%.0f KB/s), retransmits %d"
              % (stats["bytes"], stats["seconds"], stats["kbps"],
                 stats["retransmits"]))
        results.append(stats)
        if item["reboot"]:
            print("      section %s restarts the board right after the write - "
                  "this is the last item of the plan" % item["sig"])
        else:
            wait = max(pause, flash_write_seconds(item["length"]))
            print("      the bootloader is writing the section to flash, waiting "
                  "%.0f s (about %.0f KB/s, measured)" % (wait, FLASH_WRITE_BPS / 1000.0))
            time.sleep(wait)
            waited = True
    return results


# --- slicing a raw dump into r6cr images --------------------------------

SECTOR = 0x1000


def _all_ff(data):
    return data.count(0xFF) == len(data)


def first_erased_sector(image, base, start, end):
    """The first address in [start, end) whose sector in the dump is entirely 0xFF.
    Returns None if there is none."""
    off = start - base
    while off + SECTOR <= end - base:
        if _all_ff(image[off:off + SECTOR]):
            return base + off
        off += SECTOR
    return None


def last_used_address(image, base, start, end):
    """The address up to which restoring makes sense: the end of the last sector
    holding data in [start, end).

    Why: `flash_write` erases sectors by the formula
    nblocks = (dst+len)/erasesize - dst/erasesize + 1, so a write that ends exactly
    on a sector boundary erases one more sector BEYOND itself. The chain of "restore
    this, spoil the next, restore that too" has to be broken somewhere. We break it
    on a sector that is already erased in the dump itself: the extra erase leaves it
    in exactly the state it has in the original, and there is nothing there to
    restore."""
    off = end - base
    while off - SECTOR >= start - base:
        if not _all_ff(image[off - SECTOR:off]):
            return base + off
        off -= SECTOR
    return start


def chunks_for_range(image, base, start, end, chunk_size):
    """Slice [start, end) into pieces of chunk_size, aligned to a sector."""
    out = []
    addr = start
    while addr < end:
        size = min(chunk_size, end - addr)
        out.append((addr, image[addr - base:addr - base + size]))
        addr += size
    return out
