#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rtk_tftp_put.py - a minimal TFTP client (RFC 1350, WRQ/octet only) for sending
images to the Realtek `<RealTek>` bootloader console on the HH71VM Realtek side.

WHY A CUSTOM CLIENT RATHER THAN A STANDARD `tftp`
    The bootloader answers ACK/DATA NOT from port 69 (where the WRQ arrived) but
    from an INCREASING TID that starts at 2098 and goes up by one after every
    completed transfer. That is correct RFC 1350 behaviour, but the OPPOSITE of
    the tftp client in the router's running stock system, which required the
    server to answer from port 69 itself (see tftp_dump_mtd.py in this directory).
    So the server TID here is latched from the FIRST ACK received and used for
    every later packet of that transfer - hard-coding 69, or any other port, does
    not work.

    IMPORTANT: the bootloader inspects the file NAME before deciding what to do
    with it. `boot.img` (exact match) or any name containing `nfjrom` switches it
    from "write to flash through burn_image" to "jump to the code in RAM"
    (`Jump to 0x...`, `jalr $t9`) - BEFORE the AUTOBURN check. Such names are
    refused here unless an explicit flag is given.

FIREWALL REQUIREMENT (same as tftp_dump_mtd.py): inbound UDP replies from an
arbitrary TID are only allowed by an explicit host firewall rule for the specific
python.exe binary (Inbound, UDP, Any local port, Private+Public profiles). Run the
interpreter the rule was created for, not a different one from a venv or pyenv.

EXAMPLES
    python rtk_tftp_put.py test-a.img --host 192.168.1.6 --name test-a.img
    python rtk_tftp_put.py boot.img --name boot.img --i-know-this-executes
"""

import argparse
import os
import socket
import struct
import sys
import time

OP_RRQ, OP_WRQ, OP_DATA, OP_ACK, OP_ERROR = 1, 2, 3, 4, 5
BLOCK_SIZE = 512
DEFAULT_HOST = "192.168.1.6"   # default address of the <RealTek> console
DEFAULT_PORT = 69

# Names that make the bootloader "Jump to 0x..." instead of writing to flash.
# The comparison here is deliberately wider than the bootloader's own (which is
# byte-exact): a safety catch should block with room to spare, not exactly.
FORBIDDEN_NAME_EXACT = {"boot.img"}
FORBIDDEN_NAME_SUBSTR = ("nfjrom",)

LOGDIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tftp-put-logs")


class TftpError(RuntimeError):
    pass


def check_forbidden_name(name, override=False):
    """Raise TftpError if the name is forbidden (see FORBIDDEN_* above), unless
    override=True (--i-know-this-executes)."""
    if override:
        return
    lname = name.lower()
    if lname in FORBIDDEN_NAME_EXACT or any(s in lname for s in FORBIDDEN_NAME_SUBSTR):
        raise TftpError(
            "file name %r is refused: the bootloader recognises boot.img and "
            "*nfjrom* names and, instead of writing to flash, JUMPS to the "
            "received data and runs it as code. Rename the file, or pass "
            "override=True / --i-know-this-executes if that is what you "
            "actually want." % name)


def _pkt_wrq(filename, mode="octet"):
    return (struct.pack("!H", OP_WRQ) + filename.encode("ascii") + b"\x00"
            + mode.encode("ascii") + b"\x00")


def _pkt_data(block, payload):
    return struct.pack("!HH", OP_DATA, block & 0xFFFF) + payload


class _Logger:
    """Write every packet to a file (always) and print only a summary plus
    occasional checkpoints - otherwise a 500 KB file (more than 1000 blocks)
    drowns the terminal in per-line output."""

    def __init__(self, logfile, stdout_every=64):
        os.makedirs(LOGDIR, exist_ok=True)
        self.f = open(logfile, "a", encoding="utf-8")
        self.stdout_every = stdout_every
        self._last_stdout_block = -1

    def pkt(self, direction, block, extra=""):
        # time.strftime (unlike datetime.strftime) does not understand %f on
        # this platform (ValueError: Invalid format string), so milliseconds
        # are computed by hand from time.time().
        now = time.time()
        ts = "%s.%03d" % (time.strftime("%H:%M:%S", time.localtime(now)), int(now * 1000) % 1000)
        line = "[%s] %s block=%s %s" % (ts, direction, block, extra)
        self.f.write(line + "\n")
        self.f.flush()

    def note(self, msg, to_stdout=True):
        line = "[%s] %s" % (time.strftime("%H:%M:%S"), msg)
        self.f.write(line + "\n")
        self.f.flush()
        if to_stdout:
            print(msg)

    def progress(self, block, sent, total):
        if block - self._last_stdout_block >= self.stdout_every or sent >= total:
            print("  ... block %d, %d/%d bytes" % (block, sent, total))
            self._last_stdout_block = block

    def close(self):
        self.f.close()


def put(data, host=DEFAULT_HOST, port=DEFAULT_PORT, remote_name="upload.bin",
        timeout=2.0, retries=5, logfile=None, name_override=False,
        wrq_timeout=None, wrq_retries=None):
    """Send the bytes in `data` to (host, port) under remote_name over TFTP
    WRQ/octet. Returns a dict of statistics: bytes, blocks, seconds, kbps,
    server_host, server_tid, retransmits.

    wrq_timeout/wrq_retries give the FIRST WRQ its own patience. That is needed
    when the bootloader may still be busy writing the previous section to flash:
    it is single-threaded, our packets simply sit in a buffer meanwhile and are
    printed in one burst after `Flash Write Successed!`. Extra WRQs in that burst
    are harmful - each one resets the receive state of the server - so the
    patience is spent on few, widely spaced attempts rather than many quick
    ones."""
    check_forbidden_name(remote_name, override=name_override)

    if logfile is None:
        logfile = os.path.join(LOGDIR, "put-%s-%s.log"
                               % (time.strftime("%Y%m%d-%H%M%S"), remote_name.replace("/", "_")))
    log = _Logger(logfile)
    log.note("=== WRQ %r -> %s:%d (%d bytes) ===" % (remote_name, host, port, len(data)))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    t0 = time.time()
    retransmits = 0
    server_addr = None

    try:
        # --- WRQ, wait for the first ACK: it tells us the real server TID ---
        wrq = _pkt_wrq(remote_name)
        n_wrq = retries if wrq_retries is None else wrq_retries
        t_wrq = timeout if wrq_timeout is None else wrq_timeout
        if (n_wrq, t_wrq) != (retries, timeout):
            log.note("WRQ patience: %d attempts of %.1f s (up to %.0f s)"
                     % (n_wrq, t_wrq, n_wrq * t_wrq))
        sock.settimeout(t_wrq)
        for attempt in range(1, n_wrq + 1):
            log.pkt("SEND", "WRQ", "attempt=%d name=%r -> %s:%d" % (attempt, remote_name, host, port))
            sock.sendto(wrq, (host, port))
            try:
                pkt, addr = sock.recvfrom(65536)
            except socket.timeout:
                retransmits += 1
                continue
            op = struct.unpack("!H", pkt[:2])[0]
            if op == OP_ERROR:
                code = struct.unpack("!H", pkt[2:4])[0]
                msg = pkt[4:].split(b"\x00")[0]
                raise TftpError("server refused the WRQ: code=%d msg=%r" % (code, msg))
            if op != OP_ACK:
                raise TftpError("expected an ACK to the WRQ, got opcode=%d: %r" % (op, pkt[:16]))
            ack_block = struct.unpack("!H", pkt[2:4])[0]
            if ack_block != 0:
                raise TftpError("ACK to the WRQ carries an unexpected block number %d (wanted 0)" % ack_block)
            server_addr = addr
            sock.settimeout(timeout)
            log.pkt("RECV", 0, "ACK from %s:%d (server TID latched)" % server_addr)
            log.note("server TID = %d (latched from the first ACK, NOT port %d)" % (server_addr[1], port))
            break
        else:
            raise TftpError("WRQ was not acknowledged in %d attempts (timeout=%.1fs each, "
                            "%.0f s in total) - check that ETH has been run, the cable, "
                            "and the host firewall rule for python.exe; if another "
                            "section was written just before, the bootloader may not "
                            "have finished writing it to flash yet"
                            % (n_wrq, t_wrq, n_wrq * t_wrq))

        # --- DATA blocks, strictly to the latched server TID ---
        block = 1
        sent = 0
        offset = 0
        while True:
            chunk = data[offset:offset + BLOCK_SIZE]
            pkt_out = _pkt_data(block, chunk)
            acked = False
            for attempt in range(1, retries + 1):
                log.pkt("SEND", block, "len=%d attempt=%d" % (len(chunk), attempt))
                sock.sendto(pkt_out, server_addr)
                try:
                    pkt, addr = sock.recvfrom(65536)
                except socket.timeout:
                    retransmits += 1
                    log.pkt("TIMEOUT", block, "attempt=%d, retransmit" % attempt)
                    continue
                op = struct.unpack("!H", pkt[:2])[0]
                if op == OP_ERROR:
                    code = struct.unpack("!H", pkt[2:4])[0]
                    msg = pkt[4:].split(b"\x00")[0]
                    raise TftpError("server sent an ERROR on block %d: code=%d msg=%r" % (block, code, msg))
                if op != OP_ACK:
                    log.pkt("IGNORE", block, "unexpected opcode=%d" % op)
                    continue
                ack_block = struct.unpack("!H", pkt[2:4])[0]
                if ack_block != (block & 0xFFFF):
                    log.pkt("IGNORE", block, "ACK for block %d rather than the current one - possibly a duplicate" % ack_block)
                    continue
                if addr != server_addr:
                    log.pkt("WARN", block, "ACK arrived from %s:%d rather than the latched %s:%d - accepted anyway"
                           % (addr[0], addr[1], server_addr[0], server_addr[1]))
                acked = True
                log.pkt("RECV", block, "ACK from %s:%d" % addr)
                break
            if not acked:
                raise TftpError("block %d was not acknowledged in %d attempts" % (block, retries))
            sent += len(chunk)
            offset += len(chunk)
            log.progress(block, sent, len(data))
            if len(chunk) < BLOCK_SIZE:
                break
            block = (block + 1) & 0xFFFF
    finally:
        sock.close()

    elapsed = time.time() - t0
    stats = dict(
        bytes=len(data),
        blocks=block,
        seconds=elapsed,
        kbps=(len(data) / 1024.0 / elapsed) if elapsed > 0 else 0.0,
        server_host=server_addr[0] if server_addr else None,
        server_tid=server_addr[1] if server_addr else None,
        retransmits=retransmits,
        logfile=logfile,
    )
    log.note("=== done: %(bytes)d bytes, %(blocks)d blocks, %(seconds).2fs, "
             "%(kbps).1f KB/s, server TID=%(server_tid)s, retransmits=%(retransmits)d ==="
             % stats)
    log.close()
    return stats


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", help="file to send (normally an image already built by rtk_mkimg.py)")
    ap.add_argument("--host", default=DEFAULT_HOST)
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    ap.add_argument("--name", default=None, help="file name to use for TFTP (default: the basename)")
    ap.add_argument("--timeout", type=float, default=2.0)
    ap.add_argument("--retries", type=int, default=5)
    ap.add_argument("--i-know-this-executes", action="store_true",
                    help="allow the forbidden names (boot.img, *nfjrom*) - they "
                         "make the bootloader RUN the data instead of writing it")
    ap.add_argument("--log", default=None, help="path to the log file (default: automatic, in tftp-put-logs/)")
    args = ap.parse_args()

    with open(args.file, "rb") as f:
        data = f.read()
    remote_name = args.name or os.path.basename(args.file)

    stats = put(data, host=args.host, port=args.port, remote_name=remote_name,
               timeout=args.timeout, retries=args.retries, logfile=args.log,
               name_override=args.i_know_this_executes)

    print("bytes=%(bytes)d seconds=%(seconds).2f kbps=%(kbps).1f "
          "server_tid=%(server_tid)s retransmits=%(retransmits)d" % stats)
    print("log: %s" % stats["logfile"])


if __name__ == "__main__":
    main()
