import contextlib
import hashlib
import io
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
FLASH_TOOLS = ROOT / "tools" / "flash"
sys.path.insert(0, str(FLASH_TOOLS))
sys.path.insert(0, str(ROOT / "autobuild"))

import flash_bundle  # noqa: E402
import install_openwrt_lan  # noqa: E402
import rtk_mkimg  # noqa: E402
import rtk_tftp_put  # noqa: E402
import restore_stock_lan  # noqa: E402
import tftp_dump_mtd  # noqa: E402


TEST_IMAGE_DIRECTORY = tempfile.TemporaryDirectory()
FIRMWARE = Path(TEST_IMAGE_DIRECTORY.name) / "openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin"
FIRMWARE.write_bytes(
    rtk_mkimg.build_image("r6cr", 0x300000, b"R" * 8192)
    + rtk_mkimg.build_image("cr6c", 0x030000, b"K" * 8192)
)


class VirtualRealtekRouter:
    """A byte-level model of the stock flash and the LAN bootloader write path."""

    FLASH_SIZE = 0x1000000
    SECTOR_SIZE = 0x1000

    def __init__(self):
        pattern = bytes(range(256))
        self.flash = bytearray((pattern * (self.FLASH_SIZE // len(pattern) + 1))[
            :self.FLASH_SIZE
        ])
        self.flash[0x7D60:0x7D62] = b"\x1f\x8b"
        self.flash[0xA26000:0xC00000] = b"\xff" * (0xC00000 - 0xA26000)
        self.original = bytes(self.flash)
        self.mode = "stock"
        self.wps_entries = 0
        self.written_plans = []

    def enter_bootloader(self, pc_ip, auto_yes=False, timeout=180.0):
        self.assert_mode("stock", "openwrt")
        self.assert_equal(pc_ip, "192.168.1.50")
        self.mode = "bootloader"
        self.wps_entries += 1
        return "02:00:00:00:00:06"

    def send_plan(self, plan, host="192.168.1.6", pause=1.5):
        self.assert_equal(self.mode, "bootloader")
        self.assert_equal(host, "192.168.1.6")
        self.written_plans.append(plan)
        for item in plan:
            payload = item["data"] if item["header_to_flash"] else item["data"][16:]
            self.assert_equal(len(payload), item["flash_hi"] - item["flash_lo"] + 1)
            self._bootloader_write(item["flash_lo"], payload)
            if item["reboot"]:
                self.mode = "openwrt-first-boot"
        return [{"bytes": len(item["data"])} for item in plan]

    def _bootloader_write(self, address, payload):
        first_sector = address // self.SECTOR_SIZE
        blocks = ((address + len(payload)) // self.SECTOR_SIZE
                  - first_sector + 1)
        erase_start = first_sector * self.SECTOR_SIZE
        erase_end = min((first_sector + blocks) * self.SECTOR_SIZE, self.FLASH_SIZE)
        self.flash[erase_start:erase_end] = b"\xff" * (erase_end - erase_start)
        self.flash[address:address + len(payload)] = payload

    def finish_first_openwrt_boot(self):
        self.assert_equal(self.mode, "openwrt-first-boot")
        self.flash[0x600000:0xC00000] = b"\xff" * 0x600000
        self.mode = "openwrt"

    def normal_power_cycle(self):
        self.assert_equal(self.mode, "bootloader")
        self.mode = "stock"

    def partition(self, devname):
        base, size = {
            "mtd0": (0x000000, 0x300000),
            "mtd1": (0x300000, 0x900000),
            "mtd2": (0xC00000, 0x400000),
        }[devname]
        return bytes(self.flash[base:base + size])

    def assert_mode(self, *allowed):
        if self.mode not in allowed:
            raise AssertionError("router mode %r is not one of %r" % (self.mode, allowed))

    @staticmethod
    def assert_equal(actual, expected):
        if actual != expected:
            raise AssertionError("%r != %r" % (actual, expected))


class FlashWorkflowSimulationTests(unittest.TestCase):
    def test_virtual_flash_layout_matches_published_first_boot_code(self):
        config = (ROOT / "openwrt-feed" / "target" / "linux" / "rtkmipsel"
                  / "rtl8197f" / "config-4.14").read_text(encoding="utf-8")
        self.assertIn(
            "2880k(kernel),3072k(rootfs),6144k(rootfs_data),4096k(vendor_jffs2)",
            config,
        )
        hook = (ROOT / "openwrt-feed" / "target" / "linux" / "rtkmipsel"
                / "base-files" / "lib" / "preinit"
                / "79_wipe_stale_rootfs_data.sh").read_text(encoding="utf-8")
        self.assertIn("find_mtd_index rootfs_data", hook)
        self.assertIn("mtd erase rootfs_data", hook)
        self.assertNotIn("mtd erase vendor_jffs2", hook)

    def test_backup_refuses_three_partitions_with_wrong_sizes(self):
        class WrongLayout:
            @staticmethod
            def cmd(_command):
                return (
                    b'dev:    size   erasesize  name\n'
                    b'mtd0: 00300000 00001000 "boot+cfg+linux"\n'
                    b'mtd1: 00800000 00001000 "rootfs"\n'
                    b'mtd2: 00500000 00001000 "jffs2 file"\n'
                )

        with self.assertRaises(install_openwrt_lan._lan.LanError):
            install_openwrt_lan.check_stock_layout(WrongLayout())

    def test_stock_backup_tftp_receiver_uses_port_69_socket_for_acks(self):
        client = ("192.168.1.1", 31000)
        payload = bytes(range(256)) * 4
        packets = [
            (struct.pack("!H", tftp_dump_mtd.OP_WRQ)
             + b"mtd0_xfer.bin\x00octet\x00", client),
            (struct.pack("!HH", tftp_dump_mtd.OP_DATA, 1) + payload[:512], client),
            (struct.pack("!HH", tftp_dump_mtd.OP_DATA, 2) + payload[512:], client),
            (struct.pack("!HH", tftp_dump_mtd.OP_DATA, 3), client),
        ]

        class FakeServer:
            def __init__(self):
                self.sent = []

            def recvfrom(self, _size):
                return packets.pop(0)

            def sendto(self, packet, address):
                self.sent.append((packet, address))

        server = FakeServer()
        received, _elapsed = tftp_dump_mtd.tftp_receive_one(server, len(payload))
        self.assertEqual(received, payload)
        self.assertEqual(
            [(struct.unpack("!HH", packet), address) for packet, address in server.sent],
            [
                ((tftp_dump_mtd.OP_ACK, 0), client),
                ((tftp_dump_mtd.OP_ACK, 1), client),
                ((tftp_dump_mtd.OP_ACK, 2), client),
                ((tftp_dump_mtd.OP_ACK, 3), client),
            ],
        )

    def test_bootloader_tftp_sender_latches_reply_tid(self):
        server = ("192.168.1.6", 2098)

        class FakeSocket:
            def __init__(self):
                self.sent = []

            def settimeout(self, _timeout):
                pass

            def sendto(self, packet, address):
                self.sent.append((packet, address))

            def recvfrom(self, _size):
                packet, _address = self.sent[-1]
                opcode = struct.unpack("!H", packet[:2])[0]
                block = 0 if opcode == rtk_tftp_put.OP_WRQ else struct.unpack(
                    "!H", packet[2:4]
                )[0]
                return struct.pack("!HH", rtk_tftp_put.OP_ACK, block), server

            def close(self):
                pass

        fake_socket = FakeSocket()
        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
            rtk_tftp_put.socket, "socket", return_value=fake_socket
        ), contextlib.redirect_stdout(io.StringIO()):
            stats = rtk_tftp_put.put(
                b"A" * 1024,
                remote_name="rootfs-r6cr.img",
                logfile=str(Path(temp_dir) / "put.log"),
            )

        self.assertEqual(stats["bytes"], 1024)
        self.assertEqual(stats["blocks"], 3)
        self.assertEqual(fake_socket.sent[0][1], ("192.168.1.6", 69))
        self.assertTrue(all(address == server for _packet, address in fake_socket.sent[1:]))

    def test_offline_command_accepts_a_downloaded_image_path(self):
        result = subprocess.run(
            [
                sys.executable,
                "tools/flash/install_openwrt_lan.py",
                "--image",
                str(FIRMWARE),
                "--dry-run",
                "--skip-backup",
                "--yes",
                "--pc-ip",
                "192.168.1.50",
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("DRY RUN: nothing was written to flash", result.stdout)

    def test_generated_flash_bundle_runs_the_documented_offline_dry_run(self):
        with tempfile.TemporaryDirectory() as temporary:
            candidate = Path(temporary) / "candidate"
            candidate.mkdir()
            for name in flash_bundle.IMAGE_ASSETS.values():
                (candidate / name).write_bytes(FIRMWARE.read_bytes())
            commit = "c" * 40
            tag = "hh71vm-cccccccccccc-r7-a1"
            flash_bundle.create(ROOT, candidate, tag, commit, 7, 1)
            extracted = Path(temporary) / "extracted"
            with zipfile.ZipFile(candidate / flash_bundle.BUNDLE_ASSET) as archive:
                archive.extractall(extracted)
            bundle = extracted / flash_bundle.BUNDLE_ROOT
            verified = subprocess.run([sys.executable, "verify_bundle.py"], cwd=bundle,
                                      capture_output=True, text=True, timeout=30)
            self.assertEqual(verified.returncode, 0, verified.stdout + verified.stderr)
            result = subprocess.run([
                sys.executable,
                "tools/flash/install_openwrt_lan.py",
                "--image",
                "firmware/" + flash_bundle.IMAGE_ASSETS["fwupg"],
                "--dry-run",
                "--skip-backup",
                "--yes",
                "--pc-ip",
                "192.168.1.50",
            ], cwd=bundle, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("DRY RUN: nothing was written to flash", result.stdout)

    def test_missing_wps_bootloader_aborts_before_first_transfer(self):
        argv = [
            "tools/flash/install_openwrt_lan.py",
            "--image",
            str(FIRMWARE),
            "--skip-backup",
            "--yes",
            "--pc-ip",
            "192.168.1.50",
        ]
        send_plan = mock.Mock()
        with (
            mock.patch.object(sys, "argv", argv),
            mock.patch.object(
                install_openwrt_lan._lan,
                "guide_enter_bootloader",
                side_effect=install_openwrt_lan._lan.LanError("virtual ARP timeout"),
            ),
            mock.patch.object(install_openwrt_lan._lan, "send_plan", send_plan),
            contextlib.redirect_stdout(io.StringIO()),
        ):
            with self.assertRaises(install_openwrt_lan._lan.LanError):
                install_openwrt_lan.main()
        send_plan.assert_not_called()

    def test_restore_cli_reports_bad_backup_without_traceback(self):
        result = subprocess.run(
            [
                sys.executable,
                "tools/flash/restore_stock_lan.py",
                "--backup-dir",
                "backup-stock-does-not-exist",
                "--dry-run",
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("REFUSED:", result.stdout)
        self.assertNotIn("Traceback", result.stdout + result.stderr)

    def test_backup_install_first_boot_and_stock_rollback(self):
        router = VirtualRealtekRouter()
        protected = {
            "boot": router.original[0x000000:0x020000],
            "hwsetting": router.original[0x020000:0x024000],
            "config": router.original[0x024000:0x030000],
            "vendor_jffs2": router.original[0xC00000:0x1000000],
        }

        class FakeTelnetControl:
            def __init__(self, host="192.168.1.1", port=23):
                self.assert_equal(host, "192.168.1.1")
                self.assert_equal(port, 2323)

            def cmd(self, command):
                if command != "cat /proc/mtd":
                    raise AssertionError("unexpected Telnet command: %s" % command)
                return (
                    b'dev:    size   erasesize  name\n'
                    b'mtd0: 00300000 00001000 "boot+cfg+linux"\n'
                    b'mtd1: 00900000 00001000 "rootfs"\n'
                    b'mtd2: 00400000 00001000 "jffs2 file"\n'
                )

            def close(self):
                pass

            @staticmethod
            def assert_equal(actual, expected):
                if actual != expected:
                    raise AssertionError("%r != %r" % (actual, expected))

        dump_calls = []

        def virtual_tftp_dump(_tc, devname, size, label, outdir, pc_ip):
            self.assertEqual(pc_ip, "192.168.1.50")
            dump_calls.append(devname)
            data = router.partition(devname)
            self.assertEqual(len(data), size)
            path = Path(outdir) / ("%s-%s.bin" % (devname, label))
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            return str(path), hashlib.md5(data).hexdigest(), False

        with tempfile.TemporaryDirectory() as temp_dir:
            backup = Path(temp_dir) / "backup-stock"
            def run_install(extra_args):
                argv = [
                    "tools/flash/install_openwrt_lan.py",
                    "--image",
                    str(FIRMWARE),
                    "--backup-dir",
                    str(backup),
                ] + extra_args
                with (
                    mock.patch.object(sys, "argv", argv),
                    mock.patch.object(install_openwrt_lan.tftp_dump_mtd, "TelnetControl",
                                      FakeTelnetControl),
                    mock.patch.object(install_openwrt_lan.tftp_dump_mtd, "dump_partition",
                                      side_effect=virtual_tftp_dump),
                    mock.patch.object(install_openwrt_lan._lan, "require_router_net",
                                      return_value="192.168.1.50"),
                    mock.patch.object(install_openwrt_lan._lan, "guide_enter_bootloader",
                                      side_effect=router.enter_bootloader),
                    mock.patch.object(install_openwrt_lan._lan, "send_plan",
                                      side_effect=router.send_plan),
                    mock.patch.object(install_openwrt_lan._common, "confirm",
                                      return_value=True),
                    contextlib.redirect_stdout(io.StringIO()),
                ):
                    return install_openwrt_lan.main()

            self.assertEqual(run_install(["--dry-run"]), 0)
            self.assertEqual(dump_calls, ["mtd0", "mtd1", "mtd2"])
            self.assertEqual(router.wps_entries, 0)
            self.assertEqual(router.written_plans, [])

            self.assertEqual(run_install([]), 0)
            self.assertEqual(dump_calls, ["mtd0", "mtd1", "mtd2"])

            self.assertTrue((backup / "backup-manifest.json").is_file())
            self.assertEqual((backup / "mtd0-boot_cfg_linux.bin").read_bytes(),
                             router.original[0x000000:0x300000])
            self.assertEqual((backup / "mtd1-rootfs.bin").read_bytes(),
                             router.original[0x300000:0xC00000])
            self.assertEqual((backup / "mtd2-jffs2.bin").read_bytes(),
                             protected["vendor_jffs2"])
            self.assertEqual(router.wps_entries, 1)
            self.assertEqual([item["sig"] for item in router.written_plans[0]],
                             ["r6cr", "cr6c"])
            for item in router.written_plans[0]:
                payload = item["data"] if item["header_to_flash"] else item["data"][16:]
                self.assertEqual(
                    router.flash[item["flash_lo"]:item["flash_hi"] + 1], payload
                )
            self.assertEqual(router.flash[0x000000:0x020000], protected["boot"])
            self.assertEqual(router.flash[0x020000:0x024000], protected["hwsetting"])
            self.assertEqual(router.flash[0x024000:0x030000], protected["config"])
            self.assertEqual(router.flash[0xC00000:0x1000000], protected["vendor_jffs2"])

            router.finish_first_openwrt_boot()
            self.assertEqual(router.flash[0x600000:0xC00000], b"\xff" * 0x600000)
            self.assertEqual(router.flash[0xC00000:0x1000000], protected["vendor_jffs2"])

            def run_restore(extra_args):
                argv = [
                    "tools/flash/restore_stock_lan.py",
                    "--backup-dir",
                    str(backup),
                ] + extra_args
                output = io.StringIO()
                with (
                    mock.patch.object(sys, "argv", argv),
                    mock.patch.object(restore_stock_lan._lan, "require_router_net",
                                      return_value="192.168.1.50"),
                    mock.patch.object(restore_stock_lan._lan, "guide_enter_bootloader",
                                      side_effect=router.enter_bootloader),
                    mock.patch.object(restore_stock_lan._lan, "send_plan",
                                      side_effect=router.send_plan),
                    mock.patch.object(restore_stock_lan._common, "confirm",
                                      return_value=True),
                    contextlib.redirect_stdout(output),
                ):
                    result = restore_stock_lan.main()
                return result, output.getvalue()

            dry_result, _dry_output = run_restore(["--dry-run"])
            self.assertEqual(dry_result, 0)
            self.assertEqual(router.wps_entries, 1)
            self.assertEqual(len(router.written_plans), 1)
            restore_result, restore_output = run_restore([])
            self.assertEqual(restore_result, 0)
            self.assertIn("do NOT hold the button", restore_output)

            self.assertEqual(router.wps_entries, 2)
            self.assertEqual(router.mode, "bootloader")
            restored_plan = router.written_plans[1]
            self.assertTrue(all(item["sig"] == "r6cr" for item in restored_plan))
            self.assertTrue(all(item["flash_hi"] < 0xC00000 for item in restored_plan))
            _plan, restore_end, _capped = restore_stock_lan.build_plan(router.original)
            self.assertEqual(router.flash[0x030000:restore_end],
                             router.original[0x030000:restore_end])
            self.assertEqual(router.flash[0x000000:0x020000], protected["boot"])
            self.assertEqual(router.flash[0x020000:0x024000], protected["hwsetting"])
            self.assertEqual(router.flash[0x024000:0x030000], protected["config"])
            self.assertEqual(router.flash[0xC00000:0x1000000], protected["vendor_jffs2"])

            router.normal_power_cycle()
            self.assertEqual(router.mode, "stock")


if __name__ == "__main__":
    unittest.main()
