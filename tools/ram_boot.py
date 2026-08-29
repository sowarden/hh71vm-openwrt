#!/usr/bin/env python3
"""Load the published HH71VM OpenWrt image into RAM and capture the UART log.

This public tool implements one narrowly defined path:

1. connect to the Realtek bootloader over UART;
2. initialize bootloader Ethernet;
3. set the verified RAM load address to 0x84000000;
4. explicitly disable AUTOBURN;
5. transfer an image whose filename contains ``nfjrom``;
6. capture the subsequent UART output.

The verified HH71VM bootloader checks the TFTP filename before its AUTOBURN path.
``*nfjrom*`` selects execution from RAM instead of ``burn_image``. The tool enforces that
name, sends ``AUTOBURN 0`` as an independent guard, and has no flash read, write, or erase
command.

The router must already be stopped at the ``<RealTek>`` prompt. See
``docs/ram-boot.md`` before using this program.
"""

import argparse
import hashlib
import os
import socket
import struct
import sys
import threading
import time

try:
    import serial
except ImportError:
    serial = None


BAUD = 38400
BOOTLOADER_HOST = "192.168.1.6"
BOOTLOADER_PORT = 69
LOAD_ADDRESS = 0x84000000
REQUIRED_NAME_SUBSTRING = "nfjrom"
EXPECTED_IMAGE_SHA256 = (
    "0dd334f2c05076498bea51668f8ba45ac3fb5651faadfd685c06939d22d8ca52"
)
LOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ram-boot-logs")

TFTP_WRQ = 2
TFTP_DATA = 3
TFTP_ACK = 4
TFTP_ERROR = 5
TFTP_BLOCK_SIZE = 512

ALLOWED_BOOTLOADER_COMMANDS = {
    "",
    "ETH",
    "LOADADDR 0x84000000",
    "AUTOBURN 0",
}


class SafetyError(RuntimeError):
    """Raised before any operation that falls outside the RAM-only path."""


class TftpError(RuntimeError):
    """Raised when the bootloader does not complete the TFTP transfer."""


def validate_ram_filename(name):
    """Require the bootloader's verified RAM-execution filename marker."""
    if REQUIRED_NAME_SUBSTRING not in name.lower():
        raise SafetyError(
            "Refusing filename %r: it does not contain %r. The verified bootloader "
            "selects RAM execution from this filename marker. Do not rename the image."
            % (name, REQUIRED_NAME_SUBSTRING)
        )


def validate_bootloader_command(command):
    """Reject every bootloader command outside the fixed RAM boot sequence."""
    if command not in ALLOWED_BOOTLOADER_COMMANDS:
        raise SafetyError(
            "Refusing bootloader command %r: the public tool allows only the fixed "
            "RAM boot sequence." % command
        )


def sha256_file(path):
    """Return the lowercase SHA-256 digest of a file."""
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


class LogTee:
    """Write sanitized console text to stdout and the persistent session log."""

    def __init__(self, path):
        self.path = path
        self._file = open(path, "a", encoding="utf-8", newline="")
        self._lock = threading.Lock()

    def write(self, value):
        with self._lock:
            encoding = sys.stdout.encoding or "ascii"
            printable = value.encode(encoding, "replace").decode(encoding, "replace")
            sys.stdout.write(printable)
            sys.stdout.flush()
            self._file.write(value)
            self._file.flush()

    def note(self, value):
        self.write("\n*** %s\n" % value)

    def close(self):
        self._file.close()


class BootloaderConsole:
    """Minimal UART transport exposing only the fixed RAM boot commands."""

    def __init__(self, port, baud=BAUD, timeout=1.0):
        if serial is None:
            raise SystemExit(
                "pyserial is required. Run: python -m pip install -r "
                "tools/requirements.txt"
            )
        self._serial = serial.Serial(port, baud, timeout=timeout)

    def close(self):
        self._serial.close()

    def __enter__(self):
        return self

    def __exit__(self, *_exc):
        self.close()

    def read_until_quiet(self, quiet_ms=250, max_wait_seconds=8.0):
        """Read until no new byte arrives for ``quiet_ms`` or the deadline expires."""
        output = bytearray()
        deadline = time.time() + max_wait_seconds
        last_data = time.time()

        while time.time() < deadline:
            chunk = self._serial.read(4096)
            if chunk:
                output.extend(chunk)
                last_data = time.time()
            elif output and (time.time() - last_data) * 1000 >= quiet_ms:
                break
            else:
                time.sleep(0.02)

        return bytes(output)

    def _command(self, command, quiet_ms=250, max_wait_seconds=8.0):
        validate_bootloader_command(command)
        self._serial.reset_input_buffer()
        self._serial.write(command.encode("ascii") + b"\r")
        return self.read_until_quiet(
            quiet_ms=quiet_ms, max_wait_seconds=max_wait_seconds
        ).decode("latin-1")

    def probe(self):
        return self._command("")

    def initialize_ethernet(self):
        response = self._command("ETH", max_wait_seconds=5.0)
        if "Ethernet init Okay" not in response:
            raise RuntimeError(
                "The bootloader did not confirm Ethernet initialization: %r" % response
            )
        return response

    def set_verified_load_address(self):
        return self._command("LOADADDR 0x84000000")

    def disable_autoburn(self):
        return self._command("AUTOBURN 0")


def tftp_wrq_packet(filename):
    """Build an RFC 1350 WRQ packet in octet mode."""
    return (
        struct.pack("!H", TFTP_WRQ)
        + filename.encode("ascii")
        + b"\x00octet\x00"
    )


def tftp_data_packet(block, payload):
    """Build an RFC 1350 DATA packet."""
    return struct.pack("!HH", TFTP_DATA, block & 0xFFFF) + payload


def parse_tftp_packet(packet):
    """Return ``(opcode, block_or_error_code, message)`` for a TFTP response."""
    if len(packet) < 4:
        raise TftpError("Received a truncated TFTP packet: %r" % packet)
    opcode, value = struct.unpack("!HH", packet[:4])
    message = packet[4:].split(b"\x00", 1)[0] if opcode == TFTP_ERROR else b""
    return opcode, value, message


def put_tftp(data, remote_name, log, timeout=2.0, retries=5):
    """Transfer ``data`` to the Realtek bootloader and return transfer statistics."""
    validate_ram_filename(remote_name)
    server_address = None
    retransmits = 0
    started = time.time()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)

    try:
        request = tftp_wrq_packet(remote_name)
        for attempt in range(1, retries + 1):
            sock.sendto(request, (BOOTLOADER_HOST, BOOTLOADER_PORT))
            try:
                packet, address = sock.recvfrom(65536)
            except socket.timeout:
                retransmits += 1
                log.note("TFTP WRQ timeout; retry %d/%d" % (attempt, retries))
                continue

            opcode, value, message = parse_tftp_packet(packet)
            if opcode == TFTP_ERROR:
                raise TftpError(
                    "The bootloader rejected the TFTP request: code=%d message=%r"
                    % (value, message)
                )
            if opcode != TFTP_ACK or value != 0:
                raise TftpError(
                    "Expected ACK block 0, received opcode=%d block=%d"
                    % (opcode, value)
                )
            server_address = address
            break
        else:
            raise TftpError(
                "The TFTP request was not acknowledged. Check ETH initialization, the "
                "direct cable, 192.168.1.50/24, and the host firewall."
            )

        log.note("TFTP server TID locked to %s:%d" % server_address)
        block = 1
        offset = 0

        while True:
            payload = data[offset : offset + TFTP_BLOCK_SIZE]
            outgoing = tftp_data_packet(block, payload)
            acknowledged = False

            for attempt in range(1, retries + 1):
                sock.sendto(outgoing, server_address)
                try:
                    packet, address = sock.recvfrom(65536)
                except socket.timeout:
                    retransmits += 1
                    continue

                opcode, value, message = parse_tftp_packet(packet)
                if opcode == TFTP_ERROR:
                    raise TftpError(
                        "The bootloader returned TFTP ERROR at block %d: code=%d "
                        "message=%r" % (block, value, message)
                    )
                if address != server_address:
                    continue
                if opcode != TFTP_ACK or value != (block & 0xFFFF):
                    continue

                acknowledged = True
                break

            if not acknowledged:
                raise TftpError(
                    "TFTP block %d was not acknowledged after %d attempts"
                    % (block, retries)
                )

            offset += len(payload)
            if block % 256 == 0 or offset == len(data):
                log.note("TFTP progress: %d/%d bytes" % (offset, len(data)))

            if len(payload) < TFTP_BLOCK_SIZE:
                break
            block = (block + 1) & 0xFFFF
    finally:
        sock.close()

    elapsed = time.time() - started
    return {
        "bytes": len(data),
        "blocks": block,
        "seconds": elapsed,
        "kib_per_second": (len(data) / 1024.0 / elapsed) if elapsed else 0.0,
        "server_tid": server_address[1] if server_address else None,
        "retransmits": retransmits,
    }


def stream_uart(console, log, seconds):
    """Copy UART bytes to the console and log for a bounded duration."""
    deadline = time.time() + seconds
    while time.time() < deadline:
        chunk = console.read_until_quiet(quiet_ms=200, max_wait_seconds=1.0)
        if chunk:
            log.write(chunk.decode("latin-1"))


def run_ram_boot(image_path, port, uart_seconds, allow_unverified_image=False):
    """Execute the fixed RAM boot sequence."""
    remote_name = os.path.basename(image_path)
    validate_ram_filename(remote_name)

    actual_hash = sha256_file(image_path)
    if actual_hash != EXPECTED_IMAGE_SHA256 and not allow_unverified_image:
        raise SafetyError(
            "Image SHA-256 is %s, expected %s. Refusing an unverified image. "
            "Developers may use --allow-unverified-image after reviewing the source."
            % (actual_hash, EXPECTED_IMAGE_SHA256)
        )

    with open(image_path, "rb") as handle:
        image_data = handle.read()
    if not image_data:
        raise SafetyError("The image file is empty")

    os.makedirs(LOG_DIR, exist_ok=True)
    log_path = os.path.join(
        LOG_DIR, "ramboot-%s.log" % time.strftime("%Y%m%d-%H%M%S")
    )
    log = LogTee(log_path)

    try:
        log.note("Log file: %s" % log_path)
        log.note("Image: %s (%d bytes)" % (image_path, len(image_data)))
        log.note("SHA-256: %s" % actual_hash)
        log.note(
            "RAM-only guards: nfjrom filename, fixed LOADADDR 0x84000000, "
            "AUTOBURN 0, command whitelist"
        )

        with BootloaderConsole(port=port) as console:
            log.write(console.probe())
            log.note("ETH: initialize bootloader networking")
            log.write(console.initialize_ethernet())
            log.note("LOADADDR: set verified RAM entry address")
            log.write(console.set_verified_load_address())
            log.note("AUTOBURN 0: disable automatic flash programming")
            log.write(console.disable_autoburn())

            result = {}

            def transfer():
                try:
                    result["stats"] = put_tftp(
                        image_data, remote_name=remote_name, log=log
                    )
                except Exception as error:  # Preserve every transfer failure in the log.
                    result["error"] = error

            log.note(
                "Starting TFTP; the bootloader should print 'Jump to 0x84000000'"
            )
            worker = threading.Thread(target=transfer, name="tftp-put", daemon=True)
            worker.start()

            while worker.is_alive():
                chunk = console.read_until_quiet(
                    quiet_ms=200, max_wait_seconds=1.0
                )
                if chunk:
                    log.write(chunk.decode("latin-1"))
            worker.join(timeout=5.0)

            if "error" in result:
                log.note("TFTP failed: %s" % result["error"])
                stream_uart(console, log, 10.0)
                raise result["error"]

            stats = result["stats"]
            log.note(
                "TFTP complete: %(bytes)d bytes in %(seconds).1f s "
                "(%(kib_per_second).0f KiB/s), TID %(server_tid)s, "
                "%(retransmits)d retransmits" % stats
            )
            log.note("Capturing UART for %.0f seconds" % uart_seconds)
            stream_uart(console, log, uart_seconds)

        log.note("Log saved: %s" % log_path)
        return log_path
    finally:
        log.close()


def listen_only(port, uart_seconds):
    """Capture UART without sending any bootloader command."""
    os.makedirs(LOG_DIR, exist_ok=True)
    log_path = os.path.join(
        LOG_DIR, "ramboot-listen-%s.log" % time.strftime("%Y%m%d-%H%M%S")
    )
    log = LogTee(log_path)
    try:
        log.note("Listen-only mode for %.0f seconds" % uart_seconds)
        with BootloaderConsole(port=port) as console:
            stream_uart(console, log, uart_seconds)
        log.note("Log saved: %s" % log_path)
        return log_path
    finally:
        log.close()


def build_argument_parser():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("image", nargs="?", help="published *-nfjrom.bin RAM image")
    parser.add_argument("--port", required=True, help="serial port, for example COM8")
    parser.add_argument(
        "--uart-seconds",
        type=float,
        default=180.0,
        help="UART capture duration after transfer (default: 180 seconds)",
    )
    parser.add_argument(
        "--listen-only",
        action="store_true",
        help="capture UART without sending any command",
    )
    parser.add_argument(
        "--allow-unverified-image",
        action="store_true",
        help="developer option: allow an nfjrom image with a different SHA-256",
    )
    return parser


def main():
    args = build_argument_parser().parse_args()
    if args.uart_seconds <= 0:
        raise SystemExit("--uart-seconds must be greater than zero")

    if args.listen_only:
        if args.image:
            raise SystemExit("Do not provide an image with --listen-only")
        listen_only(args.port, args.uart_seconds)
        return

    if not args.image:
        raise SystemExit("An image path is required unless --listen-only is used")

    try:
        run_ram_boot(
            image_path=args.image,
            port=args.port,
            uart_seconds=args.uart_seconds,
            allow_unverified_image=args.allow_unverified_image,
        )
    except (OSError, RuntimeError, TftpError) as error:
        raise SystemExit("ERROR: %s" % error) from error


if __name__ == "__main__":
    main()
