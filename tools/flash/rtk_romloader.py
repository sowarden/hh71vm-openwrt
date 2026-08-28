#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rtk_romloader.py - a driver for the Realtek `<RealTek>` bootloader console (UART,
HH71VM Realtek side) that reads and writes flash through `FLR`/`FLW`.

COMMAND SEMANTICS come from disassembling the bootloader. In short, what matters
for this driver:

    - Commands are sent with a bare `\r` (no `\n`) - an extra `\n` left in the
      receive queue is eaten by the bootloader as the answer to a (Y/N) prompt and
      produces `Abort!`.
    - `FLW <flash_off> <ram_src> <len>` - three hex arguments, with NO argc check.
      It erases every 4 KB sector of the range itself before writing.
    - `ERASECHIP <0|1>` really does erase all 16 MB - this script physically cannot
      send it (`_check_forbidden()`, checked on every send).
    - `ERASESECTOR` is an empty stub and is not implemented here (pointless).
    - The device address defaults to 192.168.1.6 and the TFTP receive address to
      0xA0A00000.
    - `ETH` IS MANDATORY before any TFTP (the network-ready flag is only raised
      inside its handler) and unconditionally overwrites the IP and MAC, so it has
      to be called strictly before `IPCONFIG`. The console argument separator is a
      SPACE, not a colon (see `cmd_ipconfig`/`cmd_loadaddr`).

SAFETY CATCHES (in the code, not left to attentiveness):
    - `write_flash()`/`cmd_flw()` reject an address BEFORE sending anything to the
      port if it is not in the `DEFAULT_ALLOWED_WRITE_RANGES` allow-list (by
      default only the tail of mtd1, `0x00A27000`-`0x00BFFFFF`, which is all
      `0xFF` and unused by the system).
    - The `tftp` subcommand (upload through AUTOBURN) rejects an image BEFORE
      sending it if its `burnAddr..burnAddr+len` is not in the same allow-list -
      that is the only protection on the TFTP+AUTOBURN path, because the
      bootloader itself does not check the write area at all.
    - Any command containing `ERASECHIP` (in any case) is rejected at the
      low-level send, and getting past that means editing this script rather than
      passing a different argument.
    - The TFTP file names `boot.img` and `*nfjrom*` are rejected by
      `rtk_tftp_put.py` (they switch the bootloader to running the code instead of
      writing it to flash).
    - Before a real write: a printed "what, where, how much" and a demand for an
      explicit `--yes` or an interactive confirmation.
    - The whole exchange is written to a log file (the path is printed at start).

REQUIREMENTS
    pip install pyserial
    rtk_mkimg.py and rtk_tftp_put.py in the same directory (used as modules)

EXAMPLES
    python rtk_romloader.py info                                    # FLI, the prompt
    python rtk_romloader.py eth                                     # bring the bootloader network up
    python rtk_romloader.py ipconfig                                # current address (after eth)
    python rtk_romloader.py read 0x00B00000 256 --out dump.bin       # FLR+DW
    python rtk_romloader.py write 0x00B00000 --pattern 0011223344556677 --yes
    python rtk_romloader.py write 0x00B00000 --file payload.bin --yes
    python rtk_romloader.py verify 0x00B00000 --file payload.bin     # read back and compare
    python rtk_romloader.py tftp test-a.img --autoburn 0             # dry run, no writing
    python rtk_romloader.py tftp test-a.img --autoburn 1 --verify    # upload + write + readback
"""

import argparse
import os
import re
import sys
import threading
import time

try:
    import serial
except ImportError:
    serial = None

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rtk_mkimg      # image build/parse - parse_image/unpack_header are reused
import rtk_tftp_put    # TFTP WRQ client that latches the server TID

PORT_DEFAULT = "COM8"
BAUD_DEFAULT = 38400

# The tail of mtd1 (0x300000-0xBFFFFF in flash): the squashfs ends at 0xA256BE and
# everything beyond that up to 0xBFFFFF is erased (0xFF) and unused. This is the
# only range writable without widening the allow-list in the code.
DEFAULT_ALLOWED_WRITE_RANGES = [(0x00A27000, 0x00BFFFFF)]

FLASH_SIZE = 16 * 1024 * 1024

LOGDIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "romloader-logs")


class SafetyError(RuntimeError):
    pass


def check_range_allowed(addr, length, allowed_ranges):
    """Check that [addr, addr+length) lies entirely inside one of allowed_ranges (a
    list of inclusive (lo, hi) pairs) and does not run past the physical flash size.
    Returns nothing on success, raises SafetyError otherwise.

    Kept as a free function (same idea as parse_dw_output/parse_db_output below):
    it is used both by `_check_write_range()` (the FLW path) and by
    `send_image_via_tftp()` (the TFTP+AUTOBURN path, where the bootloader checks
    nothing at all and this is the only protection), and it can be unit-tested
    without a real COM port."""
    end = addr + length
    if end > FLASH_SIZE:
        raise SafetyError("a write of 0x%X..0x%X runs past the end of flash (16 MB)" % (addr, end))
    for lo, hi in allowed_ranges:
        if addr >= lo and end - 1 <= hi:
            return
    raise SafetyError(
        "a write of 0x%X..0x%X is OUTSIDE the permitted range (%s) - refused "
        "BEFORE anything was sent to the port." %
        (addr, end - 1, ["0x%X-0x%X" % r for r in allowed_ranges]))


# --- parsing DW/DB output - kept as free functions so they can be checked with
# synthetic data and no hardware (the format is confirmed by disassembly:
# DW -> "%08X:\t%08X\t%08X\t%08X\t%08X\n", DB -> "%08X: %02x %02x ..." up to 16 per line)

_DW_LINE = re.compile(
    r"([0-9A-Fa-f]{8}):\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+"
    r"([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})")
# THIS USED TO BE r"^(...)" with re.MULTILINE, and it broke on real hardware: every
# address line in genuine DB output is preceded by an EXTRA `\r`
# (`...00\n\r\rA0A00010: ...`, not just `\n`), so the `^` anchor after `\n` pointed
# at that `\r` rather than at the first hex digit and the regex never matched at all
# - DB silently returned b"" (not an exception), which could make the verification
# in write_flash() FALSELY succeed (b"" == data[:0] passes). Fixed after the pattern
# of the DW regex above: look for the pattern anywhere in the text, with no anchor to
# the start of a line - eight hex digits immediately before a `:` occur only in real
# address lines, and no false matches were seen or are expected in the rest of the
# console output.
_DB_LINE = re.compile(r"([0-9A-Fa-f]{8}):\s+((?:[0-9A-Fa-f]{2}\s+){1,16})")


def parse_dw_output(text, nwords):
    out = bytearray()
    for m in _DW_LINE.finditer(text):
        for g in m.groups()[1:]:
            out += bytes.fromhex(g)[::-1]  # a word prints as BE text but sits in memory as LE
    return bytes(out[:nwords * 4])


def parse_dw_output_strict(text, ram_addr, nwords):
    """Like `parse_dw_output`, but additionally checks that the addresses of the
    recognised lines form a STRICT sequence `ram_addr, ram_addr+16, ram_addr+32, ...`
    with no gaps, repeats or reordering.

    WHY (found during a real read of `mtd2`): corruption on the UART can replace a
    line with a VERBATIM REPEAT of a neighbouring one - observed: the same 62-byte
    sequence repeated seven times in a row - and the resulting line count and total
    length both stay correct, so neither the chunk length check (`read_flash`, added
    after that same incident) nor `parse_dw_output` itself (which simply
    concatenates words in the order they appear, without looking at the printed
    address) catches it: the resulting bytes are wrong rather than missing. Here the
    address of each line is part of its own text (`AXXXXXXX:`), so comparing the
    actual address sequence against the geometry of the request is enough, with no
    extra parity or CRC on the device side."""
    nlines = (nwords + 3) // 4
    matches = list(_DW_LINE.finditer(text))
    addrs = [int(m.group(1), 16) for m in matches]
    expected = [ram_addr + 16 * i for i in range(nlines)]
    if addrs != expected:
        for i, (a, e) in enumerate(zip(addrs, expected)):
            if a != e:
                raise ValueError(
                    "line %d: address 0x%X != the expected 0x%X (lines recognised: %d, "
                    "expected: %d)" % (i, a, e, len(addrs), nlines))
        raise ValueError("the number of recognised lines %d != the expected %d"
                          % (len(addrs), nlines))
    out = bytearray()
    for m in matches:
        for g in m.groups()[1:]:
            out += bytes.fromhex(g)[::-1]
    return bytes(out[:nwords * 4])


def parse_db_output(text, nbytes):
    out = bytearray()
    for m in _DB_LINE.finditer(text):
        out += bytes.fromhex(m.group(2).replace(" ", ""))
    return bytes(out[:nbytes])


class RomLoader:
    def __init__(self, port=PORT_DEFAULT, baud=BAUD_DEFAULT, timeout=1.0,
                 allowed_write_ranges=None, logfile=None):
        if serial is None:
            raise SystemExit("pyserial is required: pip install pyserial")
        self.allowed_write_ranges = allowed_write_ranges or list(DEFAULT_ALLOWED_WRITE_RANGES)
        os.makedirs(LOGDIR, exist_ok=True)
        if logfile is None:
            logfile = os.path.join(LOGDIR, "session-%s.log" % time.strftime("%Y%m%d-%H%M%S"))
        self.logfile = logfile
        self._log = open(logfile, "a", encoding="utf-8")
        self._log_line("=== session started, port=%s baud=%d ===" % (port, baud))
        self._log_line("permitted write ranges: %s"
                       % ["0x%X-0x%X" % r for r in self.allowed_write_ranges])
        self.ser = serial.Serial(port, baud, timeout=timeout)

    def close(self):
        self._log_line("=== session closed ===")
        self._log.close()
        self.ser.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # --- low-level exchange ----------------------------------------------

    def _log_line(self, msg):
        line = "[%s] %s" % (time.strftime("%H:%M:%S"), msg)
        self._log.write(line + "\n")
        self._log.flush()

    def _check_forbidden(self, cmd):
        if re.search(r"erasechip", cmd, re.IGNORECASE):
            raise SafetyError(
                "the command %r contains ERASECHIP - sending it is blocked in the "
                "driver (it erases the whole chip). If it is REALLY needed, edit "
                "rtk_romloader.py itself rather than passing it as an argument." % cmd)

    def read_quiet(self, quiet_ms=250, max_wait_s=8.0):
        """Read from the port until there is a pause of quiet_ms with no new bytes
        (or max_wait_s runs out). Returns the accumulated bytes."""
        buf = b""
        deadline = time.time() + max_wait_s
        last_data = time.time()
        while time.time() < deadline:
            chunk = self.ser.read(4096)
            if chunk:
                buf += chunk
                last_data = time.time()
            else:
                if buf and (time.time() - last_data) * 1000 >= quiet_ms:
                    break
                time.sleep(0.02)
        return buf

    def send_raw(self, cmd, quiet_ms=250, max_wait_s=8.0):
        """Send a command with a bare \\r, wait for the line to go quiet, and return
        the decoded answer (latin-1, so junk bytes cannot raise)."""
        self._check_forbidden(cmd)
        self._log_line(">> %s" % cmd)
        self.ser.reset_input_buffer()
        self.ser.write(cmd.encode("ascii") + b"\r")
        raw = self.read_quiet(quiet_ms=quiet_ms, max_wait_s=max_wait_s)
        text = raw.decode("latin-1")
        self._log_line("<< %r" % text)
        return text

    def confirm_yes(self):
        """Send 'Y' in answer to a (Y)es/(N)o prompt."""
        self._log_line(">> Y (confirm)")
        self.ser.write(b"Y\r")
        raw = self.read_quiet(quiet_ms=250, max_wait_s=15.0)
        text = raw.decode("latin-1")
        self._log_line("<< %r" % text)
        return text

    def confirm_no(self):
        """Send 'N' to decline a (Y)es/(N)o prompt. A safe way to clear ANY stuck
        confirmation (FLR/FLW) left behind when a previous command was interrupted
        (the script crashed, say) before answering Y/N - the command is declined and
        nothing is read or written."""
        self._log_line(">> N (decline)")
        self.ser.write(b"N\r")
        raw = self.read_quiet(quiet_ms=250, max_wait_s=15.0)
        text = raw.decode("latin-1")
        self._log_line("<< %r" % text)
        return text

    # --- commands with no risk --------------------------------------------

    def wait_prompt(self, max_wait_s=15.0):
        """Wait for the banner and prompt after power-on with WPS held. Returns
        everything that was collected."""
        self._log_line("(waiting for the <RealTek> prompt after a manual restart with WPS)")
        raw = self.read_quiet(quiet_ms=500, max_wait_s=max_wait_s)
        text = raw.decode("latin-1")
        self._log_line("<< %r" % text)
        return text

    def cmd_fli(self):
        """FLI - initialise and identify the flash. The expected answer contains
        'w25q128, size=16MB'."""
        return self.send_raw("FLI")

    def cmd_ipconfig(self, ip=None):
        """IPCONFIG [a.b.c.d] - with no argument, print the current device address.

        IMPORTANT (found by reading the tokeniser): the console argument separator
        is a SPACE (0x8000A754), not a colon. The first version of this method sent
        "IPCONFIG:<ip>", which is a single unrecognised token and makes the console
        answer "Unknown command !"."""
        return self.send_raw("IPCONFIG %s" % ip if ip else "IPCONFIG")

    def cmd_loadaddr(self, addr=None):
        """LOADADDR [hex] - with no argument, print the current TFTP receive address
        (0xA0A00000 by default). Same separator bug as IPCONFIG - fixed to a
        space."""
        return self.send_raw("LOADADDR 0x%X" % addr if addr is not None else "LOADADDR")

    def cmd_autoburn(self, enabled):
        return self.send_raw("AUTOBURN %d" % (1 if enabled else 0))

    def cmd_eth(self, wait_s=5.0):
        """ETH - bring up Ethernet and the bootloader network stack. MANDATORY
        before any TFTP: the network-ready flag (0x800148E4) is raised ONLY inside
        this command's handler, and without it both TFTP packet handlers (WRQ/DATA)
        silently do nothing. ETH also unconditionally rewrites the IP and MAC to
        their defaults (192.168.1.6), so it must be called STRICTLY before IPCONFIG,
        never after. The expected answer contains '---Ethernet init Okay V003---'."""
        text = self.send_raw("ETH", quiet_ms=250, max_wait_s=wait_s)
        if "Ethernet init Okay" not in text:
            raise RuntimeError("ETH did not confirm initialisation, answer: %r" % text)
        return text

    # --- DW/DB - parsing the output ----------------------------------------

    def cmd_dw(self, addr, nwords):
        """DW <addr> <len> - read nwords 32-bit words from RAM and return bytes
        (little-endian, as they sit in memory on MIPS LE).

        IMPORTANT: the bootloader parses the address as hex but the length AS
        DECIMAL (strtoul(..., 10)), unlike every other command (FLR/FLW/EB are all
        hex). Confirmed by disassembling the DW handler (0x8000CFD8) and by a live
        test: `DW addr 0x40` returned empty output (strtoul("0x40",10) stops at the
        'x' and yields 0) while `DW addr 64` works. The length is in words (4 bytes),
        and one output line is 4 words, i.e. 16 bytes."""
        text = self.send_raw("DW 0x%X %d" % (addr, nwords))
        return parse_dw_output(text, nwords)

    def cmd_db(self, addr, nbytes):
        """DB <addr> <len> - read nbytes bytes from RAM line by line (up to 16 per
        line). Same quirk as DW: the address is hex, the length is decimal."""
        text = self.send_raw("DB 0x%X %d" % (addr, nbytes))
        return parse_db_output(text, nbytes)

    def cmd_eb(self, addr, data, chunk=16):
        """EB <addr> <v1> <v2> ... - write data into RAM in small pieces (16 bytes
        per call by default, a safe margin against the unknown argument limit of the
        interpreter)."""
        for off in range(0, len(data), chunk):
            piece = data[off:off + chunk]
            args = " ".join("0x%02X" % b for b in piece)
            self.send_raw("EB 0x%X %s" % (addr + off, args))

    # --- reading flash (FLR + DW) -----------------------------------------

    def read_flash(self, flash_addr, length, ram_scratch=0xA0400000, chunk=0x1000,
                    max_retries=3):
        """Read length bytes from flash through FLR (flash to RAM) plus DW (RAM to
        text). The default RAM address is chosen far from the TFTP load area
        (0xA0A00000) and from anything meaningful in the bootloader.

        IMPORTANT (found during a live read of mtd2, 4 MB - two independent bugs):
        1. `parse_dw_output` silently returns FEWER bytes than asked for when even a
           single line of the text hex dump is lost on the UART, WITHOUT raising.
           Without a length check on every chunk the resulting file came out short
           and, worse, everything AFTER the gap was shifted with respect to the real
           flash addresses - useless for choosing a write address.
        2. Checking the length alone is NOT ENOUGH: corruption can replace a line
           with a verbatim repeat of its neighbour (a real case - the same 62-byte
           sequence repeated seven times in a row), which preserves both the line
           count and the total length but puts wrong bytes in the wrong place. Hence
           `parse_dw_output_strict` here - it also checks the sequence of addresses
           the device printed on each line.
        Both cases re-read the chunk (up to max_retries times) before moving on."""
        out = bytearray()
        remaining = length
        addr = flash_addr
        while remaining > 0:
            n = min(chunk, remaining)
            nwords = (n + 3) // 4
            data = None
            last_err = None
            for attempt in range(1, max_retries + 1):
                text = self.send_raw("FLR 0x%X 0x%X 0x%X" % (ram_scratch, addr, n))
                if "(Y)es" not in text and "(Y)" not in text:
                    raise RuntimeError("FLR did not ask for confirmation, answer: %r" % text)
                conf = self.confirm_yes()
                if "Successed" not in conf and "Success" not in conf:
                    raise RuntimeError("FLR did not report success, answer: %r" % conf)
                dw_text = self.send_raw("DW 0x%X %d" % (ram_scratch, nwords))
                try:
                    chunk_data = parse_dw_output_strict(dw_text, ram_scratch, nwords)
                except ValueError as exc:
                    last_err = str(exc)
                    print("!!! chunk 0x%X: broken DW address sequence (%s) "
                          "(attempt %d/%d), re-reading..." % (addr, last_err, attempt, max_retries))
                    continue
                if len(chunk_data) == n:
                    data = chunk_data
                    break
                last_err = "got %d/%d bytes" % (len(chunk_data), n)
                print("!!! chunk 0x%X: %s (attempt %d/%d), re-reading..."
                      % (addr, last_err, attempt, max_retries))
            if data is None:
                raise RuntimeError(
                    "chunk at address 0x%X: could not get the full %d bytes in %d attempts "
                    "(last error: %s)" % (addr, n, max_retries, last_err))
            out += data
            addr += n
            remaining -= n
        assert len(out) == length, "internal error: final length %d != %d" % (len(out), length)
        return bytes(out)

    # --- writing flash (EB + FLW) - with the safety catches ------------------

    def _check_write_range(self, addr, length):
        check_range_allowed(addr, length, self.allowed_write_ranges)

    def cmd_flw(self, flash_addr, ram_addr, length, spi_cnt=1):
        """FLW <flash_off> <ram_src> <len> <spi_cnt> - RAM to flash.
        `spi_cnt` is in fact never read by the bootloader, but it is passed for the
        sake of a readable log and of matching the command's own help text.

        IMPORTANT (found in a live test): the interactive `FLW` command does NOT
        print "Flash Write Successed!" or "Failed" - those strings belong ONLY to
        the automatic `burn_image` function (TFTP+AUTOBURN), not to the FLW handler
        (confirmed by disassembly: after the call to `flash_write` at `0x8000D930`
        there is no printf with either string). The only reliable way to know it
        worked is to read the data back (see `write_flash()`), not to parse the text
        of the confirmation reply."""
        self._check_write_range(flash_addr, length)
        text = self.send_raw("FLW 0x%X 0x%X 0x%X %d" % (flash_addr, ram_addr, length, spi_cnt))
        if "(Y)es" not in text:
            raise RuntimeError("FLW did not ask for confirmation, answer: %r" % text)
        conf = self.confirm_yes()
        if "Abort" in conf:
            raise RuntimeError("FLW was aborted by the bootloader (Abort!): %r" % conf)
        return conf

    def write_flash(self, flash_addr, data, ram_scratch=0xA0400000, yes=False,
                    confirm_cb=None):
        """The full cycle: EB (into RAM) -> FLW (RAM to flash) -> FLR+DB (read back
        to verify). The range is checked BEFORE the first byte reaches the port. The
        only source of truth about success is that the bytes read back match what
        was written (see cmd_flw)."""
        self._check_write_range(flash_addr, len(data))
        summary = ("WRITE: %d bytes to flash at 0x%08X..0x%08X (through RAM 0x%08X)"
                  % (len(data), flash_addr, flash_addr + len(data) - 1, ram_scratch))
        print(summary)
        self._log_line(summary)
        if not yes:
            if confirm_cb is not None:
                if not confirm_cb(summary):
                    print("cancelled by the user")
                    return False
            else:
                ans = input("Confirm the write? (yes/no): ").strip().lower()
                if ans not in ("yes", "y"):
                    print("cancelled by the user")
                    return False
        self.cmd_eb(ram_scratch, data)
        readback = self.cmd_db(ram_scratch, min(len(data), 256))
        if readback != data[:len(readback)]:
            raise RuntimeError(
                "the data in RAM after EB is not what was expected - not going on to FLW. "
                "wanted=%s got=%s" % (data[:16].hex(), readback[:16].hex()))
        self.cmd_flw(flash_addr, ram_scratch, len(data))
        flash_readback = self.read_flash(flash_addr, len(data), ram_scratch=ram_scratch)
        if flash_readback != data:
            raise RuntimeError(
                "THE CHECK AFTER FLW FAILED - flash does not hold what was written. "
                "wanted=%s got=%s" % (data[:16].hex(), flash_readback[:16].hex()))
        return True

    # --- upload through TFTP+AUTOBURN ---------------------------------------

    def _neutralize_wrt_hack(self, loadaddr, total_file_len):
        """Zero the four bytes immediately past the expected end of the received
        file in RAM - a catch against the 'wrt image' hack in `burn_image`: when the
        section length is a multiple of `0x1000` AND the bytes right after the body
        are `DE AD C0 DE`, the bootloader silently adds four bytes to the write
        length. Leftovers from earlier operations in that area of RAM could
        otherwise happen to contain that signature."""
        addr = loadaddr + total_file_len
        self.cmd_eb(addr, b"\x00\x00\x00\x00")

    def send_image_via_tftp(self, image_bytes, remote_name, autoburn, host=None,
                            loadaddr=0xA0A00000, do_eth=True, tftp_kwargs=None,
                            uart_timeout_s=120.0):
        """The full cycle: `ETH` -> `LOADADDR` -> `AUTOBURN` -> neutralise the
        wrt-hack -> TFTP upload (in a separate thread) -> read and log the UART in
        the main thread while the transfer and (with `autoburn`) the write itself
        are running.

        `image_bytes` is an image already built by `rtk_mkimg.py` (header plus
        body). The range `burnAddr..burnAddr+len` is checked BEFORE the transfer
        against `self.allowed_write_ranges` - the bootloader does not check the
        write area at all, so this is the only protection on the TFTP+AUTOBURN path,
        the counterpart of the allow-list in `write_flash()`/`cmd_flw()`.

        Returns `(uart_text, tftp_stats)`. Raises if the TFTP transfer failed (the
        details are in the `rtk_tftp_put` log, whose path is in `stats['logfile']`
        and is reachable through `result` even on failure)."""
        hdr = rtk_mkimg.unpack_header(image_bytes)
        if len(image_bytes) < 16 + hdr["length"]:
            raise ValueError("image_bytes is shorter than the header claims (%d needed, %d present)"
                             % (16 + hdr["length"], len(image_bytes)))
        # What has to be checked is the length of WHAT LANDS IN FLASH, not the length
        # of the body: for cs6c/cr6c the bootloader also writes the 16-byte header,
        # starting at burnAddr (rtk_mkimg.SECTIONS, header_to_flash=True). Measured by
        # body length, the real range came out 16 bytes longer than the checked one,
        # and an image sitting right against the upper bound of the permitted area
        # would pass.
        flash_len = hdr["length"]
        if rtk_mkimg.SECTIONS.get(hdr["sig"], {}).get("header_to_flash"):
            flash_len += 16
        try:
            check_range_allowed(hdr["burn_addr"], flash_len, self.allowed_write_ranges)
        except SafetyError as e:
            raise SafetyError(
                "image %r: %s (AUTOBURN would write there WITHOUT any check by the "
                "bootloader - refused by the driver BEFORE the transfer)" % (remote_name, e))

        summary = ("TFTP: %r (%d bytes, sig=%s, burnAddr=0x%X, len=0x%X), AUTOBURN=%d"
                  % (remote_name, len(image_bytes), hdr["sig"], hdr["burn_addr"], hdr["length"], int(autoburn)))
        print(summary)
        self._log_line(summary)

        if do_eth:
            self.cmd_eth()
        self.cmd_loadaddr(loadaddr)
        self.cmd_autoburn(autoburn)
        self._neutralize_wrt_hack(loadaddr, len(image_bytes))

        tftp_kwargs = dict(tftp_kwargs or {})
        tftp_kwargs.setdefault("host", host or rtk_tftp_put.DEFAULT_HOST)
        result = {}

        def _run_tftp():
            try:
                result["stats"] = rtk_tftp_put.put(image_bytes, remote_name=remote_name, **tftp_kwargs)
            except Exception as e:
                result["error"] = e

        th = threading.Thread(target=_run_tftp, name="tftp-put", daemon=True)
        th.start()

        # Read the UART while the transfer thread is alive (the TFTP transfer itself
        # takes seconds), and AFTER it finishes, until settle_quiet_s of UNBROKEN
        # silence on the line or until overall_deadline, whichever comes first. NOT a
        # fixed short tail: the last ACK is sent BEFORE burn_image, so the TFTP thread
        # finishes almost at once while the actual write and erase on the router can
        # run for many more seconds on a multi-sector image (found live: 522 KB, about
        # 128 sectors - a fixed 3 s tail was NOT enough, the readback issued its FLR in
        # the middle of the progress dots and got junk instead of an answer; the data
        # was unharmed, because FLR safely refused rather than returning a false
        # result). The progress dots are printed with pauses BETWEEN them, so
        # settle_quiet_s has to be comfortably longer than a typical gap between two
        # dots, or we would cut it short again.
        uart_chunks = []
        overall_deadline = time.time() + uart_timeout_s
        settle_quiet_s = 5.0
        last_activity = time.time()
        while True:
            chunk = self.read_quiet(quiet_ms=300, max_wait_s=1.0)
            now = time.time()
            if chunk:
                uart_chunks.append(chunk)
                last_activity = now
            if not th.is_alive() and (now - last_activity) >= settle_quiet_s:
                break
            if now > overall_deadline:
                self._log_line(
                    "!!! uart_timeout_s (%.0fs) expired (the TFTP thread is %s) - never "
                    "saw %.0fs of silence after the last activity on the line" %
                    (uart_timeout_s, "alive" if th.is_alive() else "finished", settle_quiet_s))
                break
        th.join(timeout=5.0)

        uart_text = b"".join(uart_chunks).decode("latin-1")
        self._log_line("<< (UART during TFTP) %r" % uart_text)

        if "error" in result:
            raise result["error"]

        # The thread is a daemon: one that outlives join() keeps sending packets while
        # the caller is already giving the bootloader its next command. This place used
        # to simply return stats=None, and a transfer with a live thread on the line
        # looked like one that had finished successfully with incomplete statistics.
        if th.is_alive():
            raise RuntimeError(
                "the TFTP thread did not finish 5 s after join(): the transfer is still "
                "running and its packets will collide with the next bootloader command. "
                "The state of flash is UNDEFINED - check the log %s" % self.logfile)

        return uart_text, result.get("stats")


# --- CLI --------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default=PORT_DEFAULT)
    ap.add_argument("--baud", type=int, default=BAUD_DEFAULT)
    ap.add_argument("--allow", action="append", default=None,
                    help="an extra permitted write range 'LO-HI' (hex, inclusive), may be "
                         "repeated; it is ADDED to DEFAULT_ALLOWED_WRITE_RANGES, it does not "
                         "replace it. Example: --allow 0xEC0000-0xEC0FFF")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("wait", help="wait for the prompt after a manual restart with WPS")
    p.add_argument("--timeout", type=float, default=15.0, help="seconds to wait (default 15)")
    sub.add_parser("info", help="FLI - identify the flash")
    sub.add_parser("abort", help="send 'N' - clear any stuck (Y)es/(N)o confirmation")

    p = sub.add_parser("read", help="read a region of flash (FLR+DW)")
    p.add_argument("addr", help="hex, for example 0x00B00000")
    p.add_argument("length", help="hex or decimal, bytes")
    p.add_argument("--out", required=True)

    p = sub.add_parser("write", help="write a region of flash (EB+FLW), with the safety catches")
    p.add_argument("addr", help="hex, for example 0x00B00000")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--file", help="file holding the data to write")
    g.add_argument("--pattern", help="hex string (for example 001122...) for a quick test")
    p.add_argument("--yes", action="store_true", help="do not ask for confirmation")

    p = sub.add_parser("verify", help="read a region and compare it with a file")
    p.add_argument("addr")
    p.add_argument("--file", required=True)

    sub.add_parser("eth", help="ETH - bring the bootloader network up (MANDATORY before TFTP)")

    p = sub.add_parser("ipconfig", help="IPCONFIG - show or set the device address (call after eth)")
    p.add_argument("ip", nargs="?", default=None, help="a.b.c.d; with no argument, show the current one")

    p = sub.add_parser("loadaddr", help="LOADADDR - show or set the TFTP receive address")
    p.add_argument("addr", nargs="?", default=None, help="hex; with no argument, show the current one")

    p = sub.add_parser("autoburn", help="AUTOBURN 0|1 - turn automatic writing of received TFTP data on or off")
    p.add_argument("enabled", type=int, choices=[0, 1])

    p = sub.add_parser("dumpram", help="DB/DW - read RAM DIRECTLY (not through FLR, unlike read)")
    p.add_argument("addr", help="hex, for example 0xA0A00000")
    p.add_argument("length", type=int, help="decimal bytes (DB) - the length really is decimal here")
    p.add_argument("--out", default=None, help="save the raw bytes to a file (optional)")

    p = sub.add_parser("tftp", help="upload an image over TFTP (ETH+LOADADDR+AUTOBURN+transfer)")
    p.add_argument("image", help="an image file built by rtk_mkimg.py build")
    p.add_argument("--name", default=None, help="file name for TFTP (default: the basename of the image)")
    p.add_argument("--autoburn", type=int, choices=[0, 1], default=0,
                   help="0 - a dry run, only receive into RAM (the default); "
                        "1 - really write to flash")
    p.add_argument("--host", default=None, help="address of the bootloader console (default: 192.168.1.6)")
    p.add_argument("--loadaddr", default="0xA0A00000", help="TFTP receive address, hex")
    p.add_argument("--skip-eth", action="store_true",
                    help="do not run ETH before the transfer (if it already ran in this session)")
    p.add_argument("--verify", action="store_true",
                    help="after --autoburn 1, read the written data back and compare it with the image")
    p.add_argument("--uart-timeout", type=float, default=120.0,
                    help="seconds to keep reading the UART after the transfer ends (default 120)")

    args = ap.parse_args()

    def _int(s):
        return int(s, 0)

    allowed_ranges = list(DEFAULT_ALLOWED_WRITE_RANGES)
    if args.allow:
        for spec in args.allow:
            lo_s, hi_s = spec.split("-")
            allowed_ranges.append((_int(lo_s), _int(hi_s)))

    with RomLoader(port=args.port, baud=args.baud, allowed_write_ranges=allowed_ranges) as rl:
        print("session log: %s" % rl.logfile)

        if args.cmd == "wait":
            print(rl.wait_prompt(max_wait_s=args.timeout))

        elif args.cmd == "info":
            print(rl.cmd_fli())

        elif args.cmd == "abort":
            print(rl.confirm_no())

        elif args.cmd == "read":
            addr = _int(args.addr)
            length = _int(args.length)
            data = rl.read_flash(addr, length)
            with open(args.out, "wb") as f:
                f.write(data)
            print("read %d bytes from 0x%08X -> %s" % (len(data), addr, args.out))

        elif args.cmd == "write":
            addr = _int(args.addr)
            if args.file:
                with open(args.file, "rb") as f:
                    data = f.read()
            else:
                data = bytes.fromhex(args.pattern)
            ok = rl.write_flash(addr, data, yes=args.yes)
            sys.exit(0 if ok else 1)

        elif args.cmd == "verify":
            addr = _int(args.addr)
            with open(args.file, "rb") as f:
                expected = f.read()
            actual = rl.read_flash(addr, len(expected))
            if actual == expected:
                print("MATCH: %d bytes at 0x%08X are identical to the file" % (len(expected), addr))
            else:
                ndiff = sum(1 for a, b in zip(actual, expected) if a != b)
                print("MISMATCH: %d of %d bytes differ" % (ndiff, len(expected)))
                sys.exit(1)

        elif args.cmd == "eth":
            print(rl.cmd_eth())

        elif args.cmd == "ipconfig":
            print(rl.cmd_ipconfig(args.ip))

        elif args.cmd == "loadaddr":
            addr = _int(args.addr) if args.addr is not None else None
            print(rl.cmd_loadaddr(addr))

        elif args.cmd == "autoburn":
            print(rl.cmd_autoburn(bool(args.enabled)))

        elif args.cmd == "dumpram":
            addr = _int(args.addr)
            data = rl.cmd_db(addr, args.length)
            print("0x%08X: %s" % (addr, data.hex()))
            if args.out:
                with open(args.out, "wb") as f:
                    f.write(data)
                print("-> %s (%d bytes)" % (args.out, len(data)))

        elif args.cmd == "tftp":
            with open(args.image, "rb") as f:
                image_bytes = f.read()
            remote_name = args.name or os.path.basename(args.image)
            loadaddr = _int(args.loadaddr)
            uart_text, stats = rl.send_image_via_tftp(
                image_bytes, remote_name, autoburn=bool(args.autoburn),
                host=args.host, loadaddr=loadaddr, do_eth=not args.skip_eth,
                uart_timeout_s=args.uart_timeout)
            print("--- UART during the transfer and write ---")
            print(uart_text)
            if stats:
                print("--- TFTP statistics ---")
                print("bytes=%(bytes)d seconds=%(seconds).2f kbps=%(kbps).1f "
                      "server_tid=%(server_tid)s retransmits=%(retransmits)d" % stats)
                print("TFTP log: %s" % stats["logfile"])
            if args.autoburn and args.verify:
                hdr = rtk_mkimg.unpack_header(image_bytes)
                body = image_bytes[16:16 + hdr["length"]]
                print("--- readback verification 0x%X, %d bytes ---" % (hdr["burn_addr"], len(body)))
                actual = rl.read_flash(hdr["burn_addr"], len(body))
                if actual == body:
                    print("MATCH: the %d bytes written at 0x%08X are identical to the image"
                          % (len(body), hdr["burn_addr"]))
                else:
                    ndiff = sum(1 for a, b in zip(actual, body) if a != b)
                    print("MISMATCH: %d of %d bytes differ" % (ndiff, len(body)))
                    sys.exit(1)


if __name__ == "__main__":
    main()
