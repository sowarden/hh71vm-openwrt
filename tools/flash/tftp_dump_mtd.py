"""
Dump the three MTD partitions of the HH71VM Realtek side over TFTP.

WHY THIS EXISTS: reading binary data with `cat`/`dd` INTO AN INTERACTIVE TTY SESSION
(telnet pty or UART console) corrupts it. The kernel line discipline on those ttys
post-processes output (OPOST): every 0x0A byte becomes 0x0D 0x0A (ONLCR, reversible),
and EVERY 0x09 byte expands into 1-8 spaces up to the next tab stop (XTABS/TAB3,
IRREVERSIBLE - the injected spaces are indistinguishable from real ones). Dumps taken
that way were all damaged: zero 0x09 bytes where about 30000 per partition were
expected.

This version avoids the problem entirely: the read still runs on the router, but THE
DATA travels over a separate UDP TFTP connection instead of the tty. The tty carries
control commands only (start tftp, take an md5sum) - short ASCII lines where ONLCR
does not matter.

SECOND FINDING: `base64` and `od` in the stock firmware are broken/decoy binaries
(`base64 /tmp/file` encodes the file NAME from argv rather than its contents, and `od`
with `-A x -t x1z` prints nothing at all). Hence the transfer goes through `tftp`.

THIRD FINDING: the built-in `tftp` client on the router is NOT RFC 1350 compliant. A
standard TFTP server answers ACK/DATA from a NEW ephemeral port - that is the TID
mechanism of the protocol - but this client ignores such replies completely and gives
up with `tftp: timeout` after about ten WRQ retries (exponential backoff, roughly 8
seconds). Found empirically: if the server answers from the very same port 69 the WRQ
arrived on, the client is happy and the transfer runs normally. So what is implemented
here is a MINIMAL single-threaded TFTP server (WRQ only, octet mode only, no
concurrent transfers - none are needed) with that one deviation from the RFC.

FIREWALL REQUIREMENT: the script listens on UDP port 69, which the host firewall has
to allow. On Windows the rule is bound to a specific interpreter binary, so running a
different `python.exe` than the one the rule was made for means Windows silently drops
the inbound UDP and the WRQ never reaches the server.

    python tftp_dump_mtd.py [output_dir] [--pc-ip 192.168.1.50]

--pc-ip is the address of the computer the router sends TFTP to.

Output (with no argument, into the current directory):
    mtd0-boot_cfg_linux.bin (3 MiB, boot + config + kernel)
    mtd1-rootfs.bin         (9 MiB, squashfs rootfs)
    mtd2-jffs2.bin          (4 MiB, jffs2 - the changeable settings)
"""
import hashlib
import os
import socket
import struct
import sys
import time

ROUTER_IP = '192.168.1.1'
TELNET_PORT = 23
# The computer is expected to hold this static address; override it with --pc-ip.
PC_IP_DEFAULT = '192.168.1.50'
TFTP_PORT = 69

PARTITIONS = [
    ('mtd0', 0x300000, 'boot_cfg_linux'),
    ('mtd1', 0x900000, 'rootfs'),
    ('mtd2', 0x400000, 'jffs2'),
]

OP_WRQ, OP_DATA, OP_ACK, OP_ERROR = 2, 3, 4, 5
MAX_RETRIES = 3


class TelnetControl:
    def __init__(self, host=ROUTER_IP, port=TELNET_PORT):
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.settimeout(2.0)
        time.sleep(0.5)
        self._drain()

    def _drain(self, wait=0.3):
        end = time.time() + wait
        buf = b''
        while time.time() < end:
            try:
                chunk = self.sock.recv(65536)
                if not chunk:
                    break
                buf += chunk
            except socket.timeout:
                break
        return buf

    def cmd(self, c, max_wait=15.0):
        self.sock.sendall(c.encode() + b'\r\n')
        out = b''
        t0 = time.time()
        while True:
            try:
                chunk = self.sock.recv(65536)
                if not chunk:
                    break
                out += chunk
            except socket.timeout:
                if out.rstrip().endswith(b'#'):
                    break
                if time.time() - t0 > max_wait:
                    print(f'  [WARNING: no prompt after {max_wait}s, command may still be running remotely]')
                    break
                continue
        return out

    def md5(self, path):
        r = self.cmd(f'md5sum {path}')
        text = r.decode(errors='replace')
        for line in text.splitlines():
            line = line.strip()
            if len(line) >= 32 and all(ch in '0123456789abcdef' for ch in line[:32]):
                return line[:32]
        return None

    def close(self):
        try:
            self.sock.sendall(b'exit\r\n')
            time.sleep(0.5)
            self._drain(0.5)
        except OSError:
            pass
        self.sock.close()


def tftp_listen(timeout):
    """Bind the TFTP port BEFORE the router is told to send — if nothing is
    listening yet, the router's WRQ gets an ICMP port-unreachable and the
    client fails fast instead of retrying, which we observed empirically."""
    srv = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    srv.bind(('0.0.0.0', TFTP_PORT))
    srv.settimeout(timeout)
    return srv


def tftp_receive_one(srv, expected_size):
    """Minimal non-RFC1350 TFTP WRQ receiver: replies ACK from port 69 itself
    (this router's tftp client ignores replies from any other port)."""
    pkt, addr = srv.recvfrom(65536)
    op = struct.unpack('!H', pkt[:2])[0]
    if op != OP_WRQ:
        raise RuntimeError(f'expected WRQ, got opcode {op}')
    srv.sendto(struct.pack('!HH', OP_ACK, 0), addr)

    out = bytearray()
    expected_block = 1
    t0 = time.time()
    while True:
        try:
            pkt, from_addr = srv.recvfrom(65536)
        except socket.timeout:
            raise TimeoutError(f'timed out after {len(out)}/{expected_size} bytes')
        op = struct.unpack('!H', pkt[:2])[0]
        if op == OP_WRQ:
            srv.sendto(struct.pack('!HH', OP_ACK, 0), from_addr)
            continue
        if op == OP_ERROR:
            code = struct.unpack('!H', pkt[2:4])[0]
            msg = pkt[4:].split(b'\x00')[0]
            raise RuntimeError(f'client ERROR code={code} msg={msg!r}')
        if op != OP_DATA:
            continue
        block = struct.unpack('!H', pkt[2:4])[0]
        payload = pkt[4:]
        if block == expected_block:
            out += payload
            srv.sendto(struct.pack('!HH', OP_ACK, block), from_addr)
            expected_block = (expected_block + 1) & 0xFFFF
            if len(payload) < 512:
                break
        else:
            srv.sendto(struct.pack('!HH', OP_ACK, block), from_addr)

    elapsed = time.time() - t0
    return bytes(out), elapsed


def dump_partition(tc, devname, size, label, outdir, pc_ip):
    """Read one MTD partition over TFTP and verify it.

    Returns (path, md5, unstable).  `unstable` is True when the partition kept
    changing while it was being read, so the copy is a best-effort snapshot
    rather than an exact image.

    WHY THAT CASE EXISTS.  A mounted JFFS2 rewrites its own device: garbage
    collection and wear levelling run on their own, with no file activity at
    all.  On stock, /dev/mtdblock2 is mounted read-write at /jffs2, and three
    md5sums taken back to back returned three different values.  A whole
    partition read therefore cannot match either the "before" or the "after"
    md5 -- the bytes on the wire are a mix of both -- and retrying only
    produces another mix.  Before 2026-08-25 this looped MAX_RETRIES times and
    aborted the whole backup.

    The partition cannot simply be remounted read-only first: busybox mount
    wants /etc/mtab, and stock keeps /etc on a read-only squashfs.

    So the two failures are told apart instead:

      device md5 stable, data differs -> the transfer is at fault, retry
      device md5 changed under us     -> retrying cannot help, keep the snapshot

    A clean read is still preferred: the best-effort copy is only used after
    every attempt has been spent, in case the partition happens to go quiet.
    """
    remote_path = f'/dev/{devname}'
    remote_name = f'{devname}_xfer.bin'
    outfile = os.path.join(outdir, f'{devname}-{label}.bin')
    best_effort = None

    for attempt in range(1, MAX_RETRIES + 1):
        print(f'[{devname}] attempt {attempt}/{MAX_RETRIES}: md5sum before...')
        md5_before = tc.md5(remote_path)
        print(f'[{devname}] device md5 (before) = {md5_before}')

        srv = tftp_listen(timeout=90)
        try:
            tc.sock.sendall(f'tftp -p -l {remote_path} -r {remote_name} {pc_ip}\r\n'.encode())
            try:
                data, elapsed = tftp_receive_one(srv, size)
            except (TimeoutError, RuntimeError) as e:
                print(f'[{devname}] TFTP receive failed: {e}')
                tc._drain(2.0)
                continue
        finally:
            srv.close()

        reply = tc._drain(2.0)
        print(f'[{devname}] router reply: {reply!r}')

        md5_after = tc.md5(remote_path)
        print(f'[{devname}] device md5 (after)  = {md5_after}')

        local_md5 = hashlib.md5(data).hexdigest()
        size_ok = len(data) == size
        md5_ok = local_md5 in (md5_before, md5_after)

        print(f'[{devname}] received {len(data)} bytes in {elapsed:.2f}s, local_md5={local_md5}')
        print(f'[{devname}] size_ok={size_ok} md5_ok={md5_ok}'
              + ('' if md5_before == md5_after else '  (NOTE: device md5 changed during read — expected for mounted jffs2)'))

        if size_ok and md5_ok:
            with open(outfile, 'wb') as f:
                f.write(data)
            return outfile, local_md5, False

        if size_ok and md5_before != md5_after:
            best_effort = (data, local_md5)
            print(f'[{devname}] partition changed while being read; keeping this '
                  f'snapshot as a fallback and trying for a clean one')
            continue

        print(f'[{devname}] verification FAILED (device md5 held still, so the '
              f'transfer is at fault), retrying...')

    if best_effort is not None:
        data, local_md5 = best_effort
        with open(outfile, 'wb') as f:
            f.write(data)
        print(f'[{devname}] never held still across {MAX_RETRIES} attempts -- '
              f'saved a best-effort snapshot, md5 {local_md5}')
        return outfile, local_md5, True

    raise RuntimeError(f'{devname}: exhausted {MAX_RETRIES} attempts, giving up')


def main():
    import argparse

    def tcp_port(value):
        port = int(value)
        if not 1 <= port <= 65535:
            raise argparse.ArgumentTypeError("port must be in the range 1..65535")
        return port

    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("outdir", nargs="?", default=".", help="where to save the dumps (default: current directory)")
    ap.add_argument("--pc-ip", default=PC_IP_DEFAULT,
                    help="address of this computer, the one the router sends TFTP "
                         "to (default %s)" % PC_IP_DEFAULT)
    ap.add_argument("--telnet-port", type=tcp_port, default=TELNET_PORT,
                    help="stock Telnet port (default: %(default)s)")
    args = ap.parse_args()
    outdir = args.outdir
    os.makedirs(outdir, exist_ok=True)

    tc = TelnetControl(port=args.telnet_port)
    checksums = {}
    try:
        for devname, size, label in PARTITIONS:
            outfile, sha_input_md5, _unstable = dump_partition(tc, devname, size, label, outdir, args.pc_ip)
            with open(outfile, 'rb') as f:
                sha = hashlib.sha256(f.read()).hexdigest()
            checksums[os.path.basename(outfile)] = sha
            print(f'[{devname}] OK -> {outfile}  sha256={sha}')
            print()
    finally:
        tc.close()

    # newline='' - otherwise Python on Windows silently turns \n into \r\n and
    # `sha256sum -c` reads the stray \r as part of the file name. The data
    # itself is fine; only the check with a standard sha256sum -c breaks.
    with open(os.path.join(outdir, 'SHA256SUMS.txt'), 'w', newline='') as f:
        for name, sha in checksums.items():
            f.write(f'{sha} *{name}\n')

    print('DONE.')


if __name__ == '__main__':
    main()
