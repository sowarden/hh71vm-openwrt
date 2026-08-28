#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
_common.py - shared plumbing for the flash tools (restore_stock.py,
flash_openwrt_tftp.py, flash_openwrt_vendor.py). Not a tool of its own. What
lives here is whatever more than one script needs:

  - the forbidden flash areas and the write-range check;
  - building and printing the plan for the path through the `<RealTek>`
    bootloader console (ETH -> LOADADDR -> AUTOBURN -> TFTP), and running it
    with readback verification;
  - a device shell over UART that can get past a login prompt (stock asks for
    one, our OpenWrt does not);
  - a temporary HTTP server on the computer for delivering files to the device.

The network and UART protocols themselves are not implemented here -
`rtk_romloader.py` and `uart_shell.py` are used as modules.
"""

import functools
import http.server
import ipaddress
import os
import shutil
import socket
import socketserver
import sys
import tempfile
import threading
import time

# The helper modules sit next to this file, so make sure this directory is on the
# module search path however the script was invoked.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rtk_mkimg  # noqa: E402
import rtk_romloader  # noqa: E402
import uart_shell  # noqa: E402


class SafetyError(RuntimeError):
    pass


# --- the device shell over UART ----------------------------------------
#
# The stock system shows a `hh71.home login:` prompt on the console and wants a
# password; our OpenWrt lets you into the shell without one. Both branches are
# needed in one place: `flash_openwrt_vendor.py` works with stock, while
# `restore_stock.py` works with our system running from RAM.

STOCK_LOGIN = "root"


class Shell:
    """A thin wrapper around uart_shell.run(): it keeps the seq counter (markers
    of different commands must not collide) and can get past a login prompt."""

    def __init__(self, ser, login=None, password=None):
        self.ser = ser
        self.login = login
        self.password = password
        self.seq = 0

    def _read(self, seconds):
        buf = b""
        deadline = time.time() + seconds
        while time.time() < deadline:
            chunk = self.ser.read(4096)
            if chunk:
                buf += chunk
            else:
                time.sleep(0.05)
        return buf.decode("latin-1")

    def wake(self, timeout=6.0):
        """Bring the console into a state where there is a shell prompt. Returns
        the text that had to be parsed to get there. Raises SafetyError if the
        bootloader is on the line, or if the login did not go through."""
        self.ser.reset_input_buffer()
        self.ser.write(b"\n")
        self.ser.flush()
        text = self._read(2.0)

        if "<RealTek>" in text:
            raise SafetyError(
                "the console is at the <RealTek> bootloader prompt rather than a "
                "system shell - this mode only works with a booted system")

        if "login:" in text:
            if not self.password:
                raise SafetyError(
                    "the console is asking for a login but no password was given - "
                    "pass --login/--password (on stock the user is root)")
            self.ser.write(("%s\n" % (self.login or STOCK_LOGIN)).encode("latin-1"))
            self.ser.flush()
            text += self._read(2.0)
            if "assword" not in text:
                raise SafetyError("no password prompt arrived after the login: %r"
                                  % text[-200:])
            self.ser.write(("%s\n" % self.password).encode("latin-1"))
            self.ser.flush()
            text += self._read(3.0)
            if "ncorrect" in text or "login:" in text.split("assword")[-1]:
                raise SafetyError("the login was refused - check the password")

        if "#" not in text and "$" not in text:
            # The console may have been quiet because the prompt had already been
            # printed earlier and Enter added nothing. Try a direct command.
            probe = self.run("echo SHELL_OK", timeout=timeout)
            if "SHELL_OK" not in probe:
                raise SafetyError(
                    "the console does not answer like a shell (answer to Enter: %r; "
                    "answer to echo: %r)" % (text[-200:], probe[-200:]))
        return text

    def run(self, cmd, timeout=15.0):
        out = uart_shell.run(self.ser, cmd, timeout, self.seq)
        self.seq += 1
        return out

    def run_ok(self, cmd, timeout=15.0, what=None):
        """Run a command and require a zero exit status."""
        out = self.run("%s; echo RC=$?" % cmd, timeout=timeout)
        if "RC=0" not in out:
            raise SafetyError("%s: the command %r returned non-zero.\n%s"
                              % (what or "step", cmd, out.strip()))
        return out


# --- the forbidden flash areas -----------------------------------------
#
#   0x000000, 128 KB  - the Realtek bootloader
#   0x020000, 16 KB   - hwsetting H601 (the MAC addresses, signature H601)
#
# Losing the bootloader means needing an external programmer, and ERASECHIP wipes
# the whole chip.
#
# These ranges are HARD-CODED and cannot be widened by any command-line argument -
# unlike the allowed_write_ranges of rtk_romloader.py, which --allow can extend.
FORBIDDEN_RANGES = [
    (0x000000, 0x01FFFF, "the Realtek bootloader (boot) - losing it means needing "
                          "an external programmer"),
    (0x020000, 0x023FFF, "hwsetting H601 (the MAC addresses of the device)"),
]


FLASH_SIZE = 0x1000000          # 16 MiB - the physical size of the chip


def check_not_forbidden(lo, hi, context=""):
    """Raise SafetyError if [lo, hi] (inclusive) overlaps any forbidden area or
    runs past the end of the chip. Call this BEFORE connecting to the port.

    The upper bound matters as much as the forbidden areas: the UART path has one
    in rtk_romloader.check_range_allowed(), while the LAN path had nothing at all
    for a while - a plan with an address past the end of the chip was accepted and
    sent to the bootloader, which checks nothing itself."""
    if hi >= FLASH_SIZE:
        raise SafetyError(
            "%sthe range 0x%06X..0x%06X runs past the end of the chip (16 MiB, "
            "last address 0x%06X)" % (context, lo, hi, FLASH_SIZE - 1))
    for f_lo, f_hi, name in FORBIDDEN_RANGES:
        if lo <= f_hi and hi >= f_lo:
            raise SafetyError(
                "%sthe range 0x%06X..0x%06X overlaps the FORBIDDEN area "
                "0x%06X..0x%06X (%s). This script never writes there, under any "
                "flag - use only explicit rtk_mkimg.py build images with a safe "
                "burnAddr." % (context, lo, hi, f_lo, f_hi, name))


def effective_flash_range(sig, burn_addr, body_length):
    """The real flash range an image with this signature, burnAddr and BODY length
    (without the header) will occupy.

    For sections with header_to_flash=True (cs6c/cr6c) the 16-byte header is
    written to flash TOGETHER with the body, starting at burnAddr: the header and
    the body sit back to back from burnAddr on. For r6cr the header never reaches
    flash (rtk_mkimg.py SECTIONS, header_to_flash=False) and the body starts right
    at burnAddr.
    """
    if sig not in rtk_mkimg.SECTIONS:
        raise SafetyError("unknown signature %r - the known ones are: %s"
                          % (sig, list(rtk_mkimg.SECTIONS)))
    header_to_flash = rtk_mkimg.SECTIONS[sig]["header_to_flash"]
    if header_to_flash:
        return burn_addr, burn_addr + 16 + body_length - 1
    return burn_addr, burn_addr + body_length - 1


def body_flash_addr(sig, burn_addr):
    """The address where the BODY (without the header) actually sits in flash -
    needed for readback verification. For header_to_flash=True that is burnAddr+16
    (the header takes the first 16 bytes of the range); for header_to_flash=False
    it is burnAddr itself.

    IMPORTANT: the built-in verification in rtk_romloader.py (the CLI `tftp
    --verify`) does NOT apply this shift - it always reads from burnAddr. For r6cr
    (header_to_flash=False, which is also the signature recommended for tests and
    recovery) that happens to be correct by accident. For cs6c/cr6c the built-in
    CLI verification would read the header instead of the body. Here the shift is
    explicit."""
    header_to_flash = rtk_mkimg.SECTIONS[sig]["header_to_flash"]
    return burn_addr + 16 if header_to_flash else burn_addr


# --- the fwupg container: several sections back to back in one file ------
#
# The format is the same as the bootloader's, but the set of signatures is limited
# to the ones fwupg accepts. Parsing it is needed both by
# `flash_openwrt_vendor.py` (installing from that container) and by
# `flash_openwrt_tftp.py` (which slices single-section images out of it).
FWUPG_SIGS = ("cs6c", "cr6c", "r6cr", "w6cg")
MAX_SECTION_LEN = 0x1000000


# --- parsing the fwupg container ----------------------------------------

def walk_fwupg_sections(data):
    """Parse data as an fwupg container: loop while the section offset plus 16 is
    still inside the file. An unknown signature AFTER at least one recognised
    section is the end of the image, not an error; an unknown signature on the
    FIRST section is an error (it matches "Invalid file format!" on the device)."""
    sections = []
    offset = 0
    n = len(data)
    while offset + 16 <= n:
        hdr = rtk_mkimg.unpack_header(data[offset:offset + 16])
        sig = hdr["sig"]
        if sig not in FWUPG_SIGS:
            if not sections:
                raise SafetyError(
                    "unknown signature %r at the start of the file - this is what "
                    "makes the device say 'Invalid file format!'" % sig)
            break  # the end of the image, not an error
        length = hdr["length"]
        if length <= 0 or length > MAX_SECTION_LEN:
            raise SafetyError(
                "section %r: the length 0x%X is outside the permitted bounds (0 < "
                "len <= 0x%X)" % (sig, length, MAX_SECTION_LEN))
        body = data[offset + 16:offset + 16 + length]
        if len(body) != length:
            raise SafetyError(
                "section %r at offset 0x%X: the file is shorter than the declared "
                "body length" % (sig, offset))
        checksum_mode = rtk_mkimg.SECTIONS[sig]["checksum"]
        if not rtk_mkimg.verify_checksum(body, checksum_mode):
            raise SafetyError(
                "section %r at offset 0x%X: the body checksum does not come out "
                "zero - the image is damaged" % (sig, offset))
        sections.append(dict(sig=sig, file_offset=offset, burn_addr=hdr["burn_addr"],
                             start_addr=hdr["start_addr"], length=length, body=body))
        offset += 16 + length
    if not sections:
        raise SafetyError("the file is shorter than 16 bytes or holds no sections at all")
    return sections


# --- building and printing the ETH->LOADADDR->AUTOBURN->TFTP plan -------

def split_container(data, name_prefix):
    """Slice an fwupg container into single-section images (header plus body), the
    way the path through the bootloader expects them. Returns a list of
    (name, bytes).

    In TFTP+AUTOBURN mode the bootloader parses exactly one header at the start of
    the file it receives, so a concatenated container cannot be handed to it. The
    sections in a container sit back to back and each already carries its own valid
    header, so slicing is just a split - nothing is rebuilt and no checksum is
    recomputed."""
    out = []
    for sec in walk_fwupg_sections(data):
        blob = data[sec["file_offset"]:sec["file_offset"] + 16 + sec["length"]]
        out.append(("%s-%s.img" % (name_prefix, sec["sig"]), blob))
    return out


def order_reboot_last(blobs):
    """Move the sections the bootloader reboots after to the end.

    The bootloader restarts the board right after writing `cs6c`/`cr6c`/`boot`
    (rtk_mkimg.SECTIONS, the reboot field). If such a section is not the last one,
    everything after it in the plan will not be written this power cycle - and it
    will look like a successful flash. Returns (the ordered list, whether the order
    was changed)."""
    def reboots(blob):
        sig = rtk_mkimg.unpack_header(blob[1][:16])["sig"]
        return rtk_mkimg.SECTIONS.get(sig, {}).get("reboot", False)

    ordered = [b for b in blobs if not reboots(b)] + [b for b in blobs if reboots(b)]
    return ordered, [id(b) for b in ordered] != [id(b) for b in blobs]


def build_bootloader_plan(image_paths):
    """Parse every file in image_paths through rtk_mkimg.parse_image and return a
    list of descriptions. Raises SafetyError BEFORE connecting to the port if any
    image fails a check (unknown signature, wrong checksum, landing in a forbidden
    area)."""
    blobs = []
    for path in image_paths:
        with open(path, "rb") as f:
            blobs.append((path, f.read()))
    return build_bootloader_plan_blobs(blobs)


def build_bootloader_plan_blobs(blobs):
    """The same as build_bootloader_plan, but from images already in memory: a list
    of (name, bytes) pairs. The name is used both as the label in the plan and as
    the file name during the TFTP transfer."""
    plan = []
    for path, data in blobs:
        info = rtk_mkimg.parse_image(data, verify=True)
        if not info["known_section"]:
            raise SafetyError(
                "%s: unknown signature %r - these tools accept only images built by "
                "rtk_mkimg.py build (known sections: %s)"
                % (path, info["sig"], list(rtk_mkimg.SECTIONS)))
        if not info["checksum_ok"]:
            raise SafetyError(
                "%s: the body checksum does not come out zero - the image is damaged "
                "or was built wrongly, and must not be uploaded" % path)
        sig = info["sig"]
        section = rtk_mkimg.SECTIONS[sig]
        lo, hi = effective_flash_range(sig, info["burn_addr"], info["length"])
        check_not_forbidden(lo, hi, "%s: " % path)
        plan.append(dict(
            path=path, data=data, sig=sig, desc=section["desc"],
            burn_addr=info["burn_addr"], length=info["length"],
            header_to_flash=section["header_to_flash"], reboot=section["reboot"],
            flash_lo=lo, flash_hi=hi,
            body_addr=body_flash_addr(sig, info["burn_addr"]),
        ))
    for i, item in enumerate(plan[:-1]):
        if item["reboot"]:
            raise SafetyError(
                "%s (section %s) makes the bootloader restart immediately after "
                "writing, but it is number %d of %d in the plan - nothing after it "
                "will be written this power cycle. Put such images last "
                "(order_reboot_last)."
                % (item["path"], item["sig"], i + 1, len(plan)))
    return plan


def print_header(title):
    print("=" * len(title))
    print(title)
    print("=" * len(title))


def print_bootloader_plan(plan, dry_run, title, extra_notes=None, autoburn=True):
    """autoburn=False is the path where AUTOBURN cannot be set (no console, that is
    no UART): a dry run then sends nothing at all, and promising "the transfer will
    happen, the write will not" would be wrong."""
    print_header(title + (" (DRY RUN)" if dry_run else ""))
    for i, item in enumerate(plan, 1):
        print("%d. %s" % (i, item["path"]))
        print("   signature=%s (%s), burnAddr=0x%06X, body=%d bytes"
              % (item["sig"], item["desc"], item["burn_addr"], item["length"]))
        print("   what actually reaches flash: 0x%06X..0x%06X (%s)"
              % (item["flash_lo"], item["flash_hi"],
                 "header and body" if item["header_to_flash"] else "body only"))
        print("   the bootloader restarts after writing this section: %s"
              % item["reboot"])
        # the documented off-by-one when erasing sectors
        if (item["flash_hi"] + 1) % 0x1000 == 0:
            boundary = item["flash_hi"] + 1
            extra = ""
            if boundary == 0xC00000:
                extra = (" That is the mtd1(rootfs)/mtd2(jffs2) BOUNDARY - make sure "
                         "the image for mtd2 is part of this run as well, or the "
                         "first sector of jffs2 will be silently erased with nothing "
                         "to restore it from.")
            print("   !!! the write ends exactly on a 4K sector boundary - by the "
                  "documented off-by-one the neighbouring sector 0x%06X..0x%06X WILL "
                  "BE ERASED in full, even though nothing was written into it.%s"
                  % (boundary, boundary + 0xFFF, extra))
    print()
    print("The areas 0x000000-0x01FFFF (bootloader) and 0x020000-0x023FFF "
          "(hwsetting/MAC) are not part of the plan and cannot be added to it "
          "by any flag.")
    if dry_run and autoburn:
        print("DRY RUN: AUTOBURN will be 0 - the TFTP transfer really happens "
              "(which checks connectivity and that the data is accepted), the "
              "write to flash does not.")
    elif dry_run:
        print("DRY RUN: NOTHING at all will be sent.")
    else:
        print("AUTOBURN will be 1 - the write to flash is REAL and cannot be undone "
              "without a separate restore.")
    for note in (extra_notes or []):
        print(note)


# --- getting into the bootloader console --------------------------------

class ConsoleEcho:
    """A minimal output sink for uart_ram_boot.catch_prompt_esc(): that function
    wants a `write`/`note` interface, and a full Tee with a log file is not needed
    here."""

    def write(self, text):
        enc = sys.stdout.encoding or "ascii"
        sys.stdout.write(text.encode(enc, "replace").decode(enc, "replace"))
        sys.stdout.flush()

    def note(self, text):
        print("\n*** %s" % text)


def console_state(port, baud, serial_mod):
    """What is on the console right now: 'bootloader', 'shell' or 'unknown'."""
    with serial_mod.Serial(port, baud, timeout=0.3) as ser:
        ser.reset_input_buffer()
        ser.write(b"\n")
        ser.flush()
        time.sleep(1.5)
        text = ser.read(8192).decode("latin-1")
    if "<RealTek>" in text:
        return "bootloader", text
    if "#" in text or "$" in text or "login:" in text:
        return "shell", text
    return "unknown", text


# The MIPS reset vector. The bootloader has no "reboot" command, but `J` can jump
# anywhere, and jumping to the reset vector restarts the board completely: the
# console prints `reboot.......` and a normal boot follows. The address must be
# given WITHOUT a `0x` prefix - with one the bootloader answers `Invalid
# Address(HEX) value`.
RESET_VECTOR = "BFC00000"


def soft_reset(rl):
    """Restart the board from the bootloader console, without cycling the power."""
    text = rl.send_raw("J %s" % RESET_VECTOR)
    if "Jump to address" not in text:
        raise SafetyError("the bootloader did not accept the jump to the reset vector: %r"
                          % text[-200:])
    return text


def wait_for_shell(port, baud, serial_mod, seconds):
    """Wait until a system prompt appears on the console. Needed after an automatic
    restart: while the board is booting the console does not answer Enter, so there
    is nothing to catch at that moment."""
    print("waiting for the system to boot (up to %.0f s)" % seconds)
    deadline = time.time() + seconds
    while time.time() < deadline:
        state, _ = console_state(port, baud, serial_mod)
        if state == "shell":
            print("the system is up")
            return True
        if state == "bootloader":
            print("the bootloader is on the console")
            return True
    return False


def ensure_bootloader(port, baud, serial_mod, esc_seconds=120.0,
                      login=None, password=None):
    """Bring the device to the `<RealTek>` prompt.

    If a running system is on the console, restart it and catch the bootloader by
    spamming ESC (the WPS button is neither needed nor wanted: it also puts the
    Qualcomm side into 900E mode, see uart_ram_boot.catch_prompt_esc). If the
    console is silent, ask for a power cycle and catch ESC all the same."""
    import uart_ram_boot as ram_boot  # local: only needed here, and it pulls in rtk_tftp_put

    state, _ = console_state(port, baud, serial_mod)
    print("console state: %s" % state)
    if state == "bootloader":
        return

    if state == "shell":
        with serial_mod.Serial(port, baud, timeout=0.3) as ser:
            sh = Shell(ser, login=login, password=password)
            sh.wake()
            print("restarting the system so the bootloader can be caught with ESC")
            ser.write(b"reboot\n")
            ser.flush()
            time.sleep(1.0)
    else:
        print("the console is silent - cycle the power of the device, do NOT hold WPS")

    with rtk_romloader.RomLoader(port=port, baud=baud) as rl:
        if not ram_boot.catch_prompt_esc(rl, ConsoleEcho(), esc_seconds):
            raise SafetyError(
                "could not reach <RealTek> with ESC in %.0f s" % esc_seconds)
        # Catching stops at the first sight of the prompt, but a couple of extra
        # ESCs still make it into the port and stay in the receive buffer of the
        # device. They stick to the front of the next command: the bootloader sees
        # "\x1bETH" and answers "Unknown command !", while the echo shows a normal
        # "ETH". An empty command eats that leftover - uart_ram_boot.py does the
        # same before its own work.
        for _ in range(2):
            rl.send_raw("")


# --- delivering files to the device: a temporary HTTP server ------------
#
# An ordinary `python -m http.server` on the computer plus a download from the
# device. TFTP is not needed here - that belongs to the bootloader, while a booted
# system has wget or curl.

ROUTER_IP = "192.168.1.1"


def guess_pc_ip(router_ip=ROUTER_IP):
    """A UDP connect sends no packets, it merely picks the outgoing interface -
    which is how we learn what address the device sees the computer as."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((router_ip, 1))
        return s.getsockname()[0]
    except OSError:
        return None
    finally:
        s.close()


def ipv4_arg(value):
    """An argparse type that accepts only a valid IPv4 literal.

    Without this check a typo in `--pc-ip` travelled all the way to the device
    inside a `curl http://<address>:<port>/...` command line: the error showed up
    on the router, in the middle of a flash, and looked like a failed download
    rather than a bad argument."""
    ipaddress.IPv4Address(value)     # ValueError -> argparse explains what is wrong
    return value


class FileServer:
    """Serve the given files over HTTP out of a temporary directory. The files are
    copied so that the device cannot see anything else from the working
    directories of the computer.

    `bind` is the address to listen on. Pass the address of the computer on the
    router's network: the server hands out the contents of working directories and
    must not be visible from other interfaces (Wi-Fi, VPN, a guest network) while
    flashing. `None` keeps the older behaviour of listening everywhere."""

    def __init__(self, paths, port, extra_files=None, bind=None):
        self.tmpdir = tempfile.mkdtemp(prefix="hh71vm-flash-tools-")
        self.names = []
        for path in paths:
            name = os.path.basename(path)
            shutil.copyfile(path, os.path.join(self.tmpdir, name))
            self.names.append(name)
        for name, content in (extra_files or {}).items():
            with open(os.path.join(self.tmpdir, name), "w",
                      encoding="utf-8", newline="\n") as f:
                f.write(content)
            self.names.append(name)

        handler = functools.partial(http.server.SimpleHTTPRequestHandler,
                                    directory=self.tmpdir)
        # Without reuse, starting again right after the previous run fails with
        # "address already in use" while the socket sits in TIME_WAIT.
        socketserver.TCPServer.allow_reuse_address = True
        self.httpd = socketserver.TCPServer((bind or "0.0.0.0", port), handler)
        self.port = self.httpd.server_address[1]
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()

    def close(self):
        self.httpd.shutdown()
        self.httpd.server_close()
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def confirm(prompt, auto_yes):
    if auto_yes:
        print("%s -> --yes was given, confirmation skipped" % prompt)
        return True
    ans = input("%s (yes/no): " % prompt).strip().lower()
    return ans in ("yes", "y")


# --- readback verification after a write through the bootloader ---------
#
# Reading goes over the UART (FLR+DW, hex as text) at about 449 B/s. For a 2.6 MB
# body a full comparison is on the order of an hour and a half, so the default
# mode checks samples.

VERIFY_MODES = ("sample", "full", "none")
SAMPLE_CHUNK = 0x1000


def verify_samples(body_length, chunk=SAMPLE_CHUNK):
    """Offsets of the sample comparison inside the body: the start, the middle
    (aligned to a sector) and the end. That catches every realistic failure of this
    path - a missed address, shifted data, and an unwritten tail. For short bodies
    it returns a single offset of 0, that is a full comparison."""
    if body_length <= chunk:
        return [(0, body_length)]
    offsets = {0, ((body_length // 2) // chunk) * chunk, body_length - chunk}
    return [(off, min(chunk, body_length - off)) for off in sorted(offsets)]


def execute_bootloader_plan(rl, plan, dry_run, verify_mode, host=None):
    """Run the plan on an already open rtk_romloader.RomLoader: ETH once (before
    the first image), then LOADADDR+AUTOBURN+TFTP for each image through
    rl.send_image_via_tftp() (the protocol is not duplicated), and, unless dry_run,
    readback verification in the given verify_mode.

    Returns the list of images whose verification had to be postponed: after
    writing a section with reboot=True the board restarts, and checking it needs a
    separate trip into the bootloader (verify_plan_items on a new connection)."""
    if verify_mode not in VERIFY_MODES:
        raise SafetyError("unknown verification mode %r" % verify_mode)
    deferred = []
    for i, item in enumerate(plan):
        print()
        print("--- image %d/%d: %s ---" % (i + 1, len(plan), item["path"]))
        uart_text, stats = rl.send_image_via_tftp(
            item["data"], os.path.basename(item["path"]),
            autoburn=not dry_run, host=host, do_eth=(i == 0))
        print(uart_text)
        if stats:
            print("TFTP: %(bytes)d bytes, %(seconds).2fs, %(kbps).1f KB/s, "
                  "server TID=%(server_tid)s" % stats)

        if dry_run or verify_mode == "none":
            continue

        if item["reboot"]:
            # The bootloader is already restarting, so there is nobody left to read
            # flash through FLR. Catching the prompt again right here does not work:
            # send_image_via_tftp only returns once the line has been quiet for five
            # seconds, and it goes quiet only AFTER the system has fully booted, by
            # which time the ESC window has long closed. So such sections are checked
            # on a separate trip into the bootloader, from the booted system.
            print("this section restarts the board - its verification is postponed "
                  "to a separate trip into the bootloader")
            deferred.append(item)
            continue

        verify_plan_items(rl, [item], verify_mode)
    return deferred


def run_bootloader_flow(plan, port, baud, serial_mod, dry_run, verify_mode,
                        host=None, catch_esc=120.0, boot_seconds=200.0,
                        login=None, password=None):
    """The whole path through the bootloader console from end to end: getting into
    `<RealTek>`, writing the plan, the postponed verification of sections the board
    restarts after, and a soft restart at the end.

    Shared by restore_stock.py and flash_openwrt_tftp.py - the only difference
    between them is which images end up in the plan."""
    allowed = list(rtk_romloader.DEFAULT_ALLOWED_WRITE_RANGES)
    for item in plan:
        allowed.append((item["flash_lo"], item["flash_hi"]))

    if catch_esc:
        ensure_bootloader(port, baud, serial_mod, esc_seconds=catch_esc,
                          login=login, password=password)

    with rtk_romloader.RomLoader(port=port, baud=baud,
                                 allowed_write_ranges=allowed) as rl:
        print("rtk_romloader session log: %s" % rl.logfile)
        deferred = execute_bootloader_plan(rl, plan, dry_run, verify_mode,
                                           host=host)

    if not deferred:
        return False

    print()
    print("%d image(s) written before an automatic restart still need checking - "
          "bringing the board back into the bootloader" % len(deferred))
    wait_for_shell(port, baud, serial_mod, boot_seconds)
    ensure_bootloader(port, baud, serial_mod,
                      esc_seconds=max(catch_esc, 120.0),
                      login=login, password=password)
    with rtk_romloader.RomLoader(port=port, baud=baud) as rl:
        verify_plan_items(rl, deferred, verify_mode)
        print("verification finished, restarting the board from the bootloader")
        soft_reset(rl)
    return True


def verify_plan_items(rl, items, verify_mode):
    """Compare the bodies of already written images by reading flash back."""
    for item in items:
        body = item["data"][16:16 + item["length"]]
        addr = item["body_addr"]
        if verify_mode == "full":
            pieces = [(0, len(body))]
        else:
            pieces = verify_samples(len(body))
        total = sum(n for _, n in pieces)
        print("readback verification of %s from 0x%06X: %d piece(s), %d bytes "
              "(roughly %.0f s over the UART)"
              % (os.path.basename(item["path"]), addr, len(pieces), total,
                 total / 449.0))
        for off, n in pieces:
            expected = body[off:off + n]
            print("  0x%06X..0x%06X ..." % (addr + off, addr + off + n - 1), end="", flush=True)
            actual = rl.read_flash(addr + off, n)
            if actual != expected:
                ndiff = sum(1 for a, b in zip(actual, expected) if a != b)
                print(" MISMATCH")
                raise SafetyError(
                    "%s: readback verification FAILED on the piece at 0x%06X "
                    "(%d of %d bytes differ) - flash does not confirm what was "
                    "written" % (item["path"], addr + off, ndiff, n))
            print(" matches")
        if verify_mode == "sample":
            print("only sample pieces were compared (--verify full compares the "
                  "whole body)")
