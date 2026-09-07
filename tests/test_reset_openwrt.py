"""Host regressions for the settings-only reset over TFTP.

The arithmetic here is the whole safety argument of the tool: one sector too far
and the erase reaches into vendor_jffs2, which nothing in this project restores.
"""

import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FLASH_TOOLS = ROOT / "tools" / "flash"
sys.path.insert(0, str(FLASH_TOOLS))

import _common  # noqa: E402
import reset_openwrt  # noqa: E402
import rtk_mkimg  # noqa: E402


VENDOR_JFFS2_ADDR = 0xC00000


class LayoutTests(unittest.TestCase):
    def test_the_partition_constants_match_the_kernel_command_line(self):
        # mtdparts: ... 2880k(kernel),3072k(rootfs),6144k(rootfs_data),4096k(vendor_jffs2)
        self.assertEqual(reset_openwrt.ROOTFS_DATA_ADDR, 0x030000 + 2880 * 1024 + 3072 * 1024)
        self.assertEqual(reset_openwrt.ROOTFS_DATA_SIZE, 6144 * 1024)
        self.assertEqual(
            reset_openwrt.ROOTFS_DATA_ADDR + reset_openwrt.ROOTFS_DATA_SIZE,
            VENDOR_JFFS2_ADDR)

    def test_the_erase_formula_is_the_one_the_bootloader_uses(self):
        # nblocks = (dst + len) / erasesize - dst / erasesize + 1
        for body_len in (0x1000, 0x2000, 0x5FF000, 0x123456):
            lo, hi = reset_openwrt.erase_range(body_len)
            dst = reset_openwrt.ROOTFS_DATA_ADDR
            nblocks = (dst + body_len) // 0x1000 - dst // 0x1000 + 1
            self.assertEqual(lo, dst)
            self.assertEqual(hi, dst + nblocks * 0x1000 - 1)


class FullEraseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.name, cls.data, _ = reset_openwrt.build_payload(quick=False)
        cls.info = rtk_mkimg.parse_image(cls.data, verify=True)

    def test_it_erases_the_whole_partition_and_not_one_byte_more(self):
        lo, hi = reset_openwrt.erase_range(self.info["length"])
        self.assertEqual(lo, reset_openwrt.ROOTFS_DATA_ADDR)
        self.assertEqual(hi, VENDOR_JFFS2_ADDR - 1)

    def test_a_full_six_mib_payload_would_have_reached_vendor_jffs2(self):
        # This is the mistake the payload size exists to avoid: it is off by one
        # sector, and the sector it would have taken belongs to another partition.
        _, hi = reset_openwrt.erase_range(reset_openwrt.ROOTFS_DATA_SIZE)
        self.assertGreater(hi, VENDOR_JFFS2_ADDR)

    def test_the_image_is_a_valid_r6cr_aimed_at_the_settings_partition(self):
        self.assertEqual(self.info["sig"], "r6cr")
        self.assertEqual(self.info["burn_addr"], reset_openwrt.ROOTFS_DATA_ADDR)
        self.assertTrue(self.info["checksum_ok"])
        # r6cr writes the body only, so the header must not land in flash.
        self.assertFalse(rtk_mkimg.SECTIONS["r6cr"]["header_to_flash"])
        self.assertFalse(rtk_mkimg.SECTIONS["r6cr"]["reboot"])

    def test_the_plan_passes_the_forbidden_range_check(self):
        plan = _common.build_bootloader_plan_blobs([(self.name, self.data)])
        self.assertEqual(len(plan), 1)
        self.assertEqual(plan[0]["flash_lo"], reset_openwrt.ROOTFS_DATA_ADDR)
        self.assertLess(plan[0]["flash_hi"], VENDOR_JFFS2_ADDR)


class QuickEraseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.name, cls.data, _ = reset_openwrt.build_payload(quick=True)
        cls.info = rtk_mkimg.parse_image(cls.data, verify=True)

    def test_it_destroys_the_jffs2_magic(self):
        body = self.data[len(self.data) - self.info["length"]:]
        self.assertNotEqual(body[:2], b"\x85\x19")

    def test_the_payload_is_not_all_ff(self):
        # The first-boot hook leaves a partition alone when it reads as blank, so
        # an 0xFF payload here would be preserved rather than erased.
        body = self.data[len(self.data) - self.info["length"]:]
        self.assertNotEqual(body, b"\xff" * len(body))

    def test_it_stays_inside_the_settings_partition(self):
        lo, hi = reset_openwrt.erase_range(self.info["length"])
        self.assertGreaterEqual(lo, reset_openwrt.ROOTFS_DATA_ADDR)
        self.assertLess(hi, VENDOR_JFFS2_ADDR)


class InstallerIntegrationTests(unittest.TestCase):
    """--reset-settings has to leave the rebooting section last, or everything
    queued behind it is silently never written."""

    def test_the_flag_adds_the_erase_without_disturbing_the_reboot_order(self):
        import install_openwrt_lan

        image = next(
            (p for p in [
                ROOT.parent / ".tmp/publication-candidate-20260828-1100/firmware/"
                              "openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin",
            ] if p.exists()), None)
        if image is None:
            self.skipTest("no firmware container available on this host")

        plan, _ = install_openwrt_lan.build_plan(str(image), reset_settings=True)
        addrs = [item["burn_addr"] for item in plan]
        self.assertIn(reset_openwrt.ROOTFS_DATA_ADDR, addrs)
        for item in plan[:-1]:
            self.assertFalse(item["reboot"], item["path"])
        self.assertTrue(plan[-1]["reboot"])

        without, _ = install_openwrt_lan.build_plan(str(image), reset_settings=False)
        self.assertNotIn(reset_openwrt.ROOTFS_DATA_ADDR,
                         [item["burn_addr"] for item in without])


if __name__ == "__main__":
    unittest.main()
