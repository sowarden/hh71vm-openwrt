import importlib.util
import pathlib
import unittest


MODULE_PATH = pathlib.Path(__file__).parents[1] / "tools" / "ram_boot.py"
SPEC = importlib.util.spec_from_file_location("ram_boot", MODULE_PATH)
RAM_BOOT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RAM_BOOT)


class SafetyTests(unittest.TestCase):
    def test_ram_filename_is_required(self):
        RAM_BOOT.validate_ram_filename("openwrt-hh71vm-nfjrom.bin")
        with self.assertRaises(RAM_BOOT.SafetyError):
            RAM_BOOT.validate_ram_filename("openwrt-hh71vm.bin")

    def test_only_fixed_ram_commands_are_allowed(self):
        for command in RAM_BOOT.ALLOWED_BOOTLOADER_COMMANDS:
            RAM_BOOT.validate_bootloader_command(command)

        for command in (
            "AUTOBURN 1",
            "FLW 0x0 0x84000000 0x1000",
            "ERASECHIP 1",
            "LOADADDR 0xA0A00000",
            "IPCONFIG 192.168.1.7",
        ):
            with self.assertRaises(RAM_BOOT.SafetyError):
                RAM_BOOT.validate_bootloader_command(command)

    def test_wrq_packet_is_octet_mode(self):
        packet = RAM_BOOT.tftp_wrq_packet("image-nfjrom.bin")
        self.assertEqual(packet, b"\x00\x02image-nfjrom.bin\x00octet\x00")

    def test_tftp_packet_parser_rejects_truncation(self):
        with self.assertRaises(RAM_BOOT.TftpError):
            RAM_BOOT.parse_tftp_packet(b"\x00\x04\x00")


if __name__ == "__main__":
    unittest.main()
