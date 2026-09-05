"""Host regressions for installing packages onto the /mnt/extern share.

The overlay is 6 MiB and some packages are 34 MB, so the share is the only place they
can go. Two things here have already been wrong once and would be wrong silently:

* `Installed-Size` on OpenWrt 19.07 is the size of the COMPRESSED payload - a large
  package reports 10 534 017 for a file of 34 341 021 - so a space check against it
  under-counts by a factor of three;
* busybox `df` wraps a long device name onto its own line, so `NR==2 {print $4}` reads
  the device name and returns an empty string, which then compares as "fits".
"""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files"
HELPER = BASE / "usr/sbin/hh71vm-extern-pkg"
SERVICES = BASE / "etc/init.d/hh71vm-extern-services"
PROFILE = BASE / "etc/profile.d/hh71vm-extern.sh"
MOUNT = BASE / "usr/sbin/hh71vm-extern-mount"
RESET = BASE / "usr/sbin/hh71vm-extern-reset"
RESET_DEFAULT = BASE / "etc/uci-defaults/95-hh71vm-extern-reset"

HELPER_SOURCE = HELPER.read_text(encoding="utf-8")
# The comments explain the very mistakes the tests forbid, so match against code only.
HELPER_CODE = "\n".join(line for line in HELPER_SOURCE.splitlines()
                        if not line.lstrip().startswith("#"))
SERVICES_SOURCE = SERVICES.read_text(encoding="utf-8")
PROFILE_SOURCE = PROFILE.read_text(encoding="utf-8")


def start_priority(source):
    for line in source.splitlines():
        if line.startswith("START="):
            return int(line.split("=", 1)[1])
    raise AssertionError("no START= in the init script")


class OrderingTests(unittest.TestCase):
    def test_services_start_after_the_mount(self):
        # hh71vm-extern mounts the share at 94, in the background and with retries.
        # Anything that runs from the share has to come after it.
        mount_init = (BASE / "etc/init.d/hh71vm-extern").read_text(encoding="utf-8")
        self.assertGreater(start_priority(SERVICES_SOURCE), start_priority(mount_init))

    def test_services_look_where_opkg_actually_writes_them(self):
        # opkg runs postinst with IPKG_INSTROOT set to the destination, so the enable
        # symlinks land under the share's prefix and procd never sees them.
        self.assertIn("etc/rc.d", SERVICES_SOURCE)
        self.assertIn("opkg", SERVICES_SOURCE)


class SpaceCheckTests(unittest.TestCase):
    def test_the_helper_measures_instead_of_trusting_installed_size(self):
        self.assertIn("ipk_unpacked_kb", HELPER_CODE)
        # If it ever goes back to reading the metadata field, this catches it.
        self.assertNotIn("awk '/^Installed-Size:/", HELPER_CODE)

    def test_free_space_counts_columns_from_the_right(self):
        self.assertIn("$(NF-2)", HELPER_CODE)
        self.assertNotIn("NR==2 {print $4}", HELPER_CODE)

    def test_both_data_member_spellings_are_tried(self):
        # busybox tar and GNU tar disagree about the leading "./".
        self.assertIn("for member in data.tar.gz ./data.tar.gz", HELPER_CODE)


class PathTests(unittest.TestCase):
    def test_the_share_is_appended_never_prepended(self):
        # A program on the share must not be able to shadow one from the image.
        exported = [line for line in PROFILE_SOURCE.splitlines() if "export PATH" in line]
        self.assertEqual(len(exported), 1)
        self.assertIn('"$PATH:', exported[0])

    def test_the_paths_are_added_only_while_the_share_is_there(self):
        self.assertIn("[ -d /mnt/extern/opkg/usr/bin ]", PROFILE_SOURCE)


@unittest.skipUnless(os.name == "posix" and shutil.which("sh"), "requires a POSIX shell")
class ShellTests(unittest.TestCase):
    def test_every_script_parses(self):
        for path in (HELPER, SERVICES, PROFILE, MOUNT, RESET, RESET_DEFAULT):
            result = subprocess.run(["sh", "-n", str(path)],
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(result.returncode, 0, path.name + ": " + result.stdout)

    @unittest.skipUnless(shutil.which("awk"), "requires awk")
    def test_free_space_survives_a_wrapped_df_line(self):
        # The exact shape busybox produces on this board for //192.168.225.1/shared_rom.
        wrapped = ("Filesystem           1K-blocks      Used Available Use% Mounted on\n"
                   "//192.168.225.1/shared_rom\n"
                   "                         56884     17656     36288  33% /mnt/extern\n")
        unwrapped = ("Filesystem           1K-blocks      Used Available Use% Mounted on\n"
                     "/dev/mtdblock5            6144       364      5780   6% /overlay\n")
        for text, expected in ((wrapped, "36288"), (unwrapped, "5780")):
            result = subprocess.run(["awk", "END {print $(NF-2)}"], input=text,
                                    text=True, stdout=subprocess.PIPE)
            self.assertEqual(result.stdout.strip(), expected)

    @unittest.skipUnless(shutil.which("tar") and shutil.which("gzip"), "requires tar and gzip")
    def test_the_measured_size_is_the_uncompressed_one(self):
        # Build a miniature .ipk whose payload compresses well, and confirm the helper's
        # measurement follows the uncompressed length rather than the archive size.
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            payload = work / "payload"
            payload.mkdir()
            (payload / "big").write_bytes(b"\0" * (2 * 1024 * 1024))
            subprocess.run(["tar", "-czf", str(work / "data.tar.gz"), "-C", str(payload), "."],
                           check=True)
            subprocess.run(["tar", "-czf", str(work / "fake.ipk"), "-C", str(work), "data.tar.gz"],
                           check=True)
            self.assertLess((work / "fake.ipk").stat().st_size, 64 * 1024)
            script = HELPER_SOURCE.split("ipk_unpacked_kb() {", 1)[1].split("\n}", 1)[0]
            probe = "ipk_unpacked_kb() {" + script + "\n}\nipk_unpacked_kb \"$1\"\n"
            result = subprocess.run(["sh", "-c", probe, "sh", str(work / "fake.ipk")],
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertGreater(int(result.stdout.strip()), 2000)


if __name__ == "__main__":
    unittest.main()
