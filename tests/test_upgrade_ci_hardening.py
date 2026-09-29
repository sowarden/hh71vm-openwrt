"""Guards on the flashing path, the extern service runner and the release workflow."""

import gzip
import os
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files"
PLATFORM = (BASE / "lib/upgrade/platform.sh").read_text()


@unittest.skipUnless(os.name == "posix" and shutil.which("sh") and shutil.which("hexdump"),
                     "requires a POSIX shell and hexdump")
class CompressedImages(unittest.TestCase):
    def compressed(self, payload):
        function = re.search(r"image_is_compressed\(\) \{.*?\n\}\n", PLATFORM, re.S).group(0)
        with tempfile.NamedTemporaryFile() as image:
            image.write(payload)
            image.flush()
            probe = function + 'image_is_compressed "$1"'
            return subprocess.run(["sh", "-c", probe, "sh", image.name]).returncode == 0

    def test_a_gzip_image_is_recognised(self):
        self.assertTrue(self.compressed(gzip.compress(b"cr6c" + b"\0" * 64)))

    def test_a_published_image_is_not(self):
        self.assertFalse(self.compressed(b"cr6c" + b"\0" * 64))


class FlashingPath(unittest.TestCase):
    def test_both_check_and_write_refuse_compressed_images(self):
        check = PLATFORM.split("platform_check_image() {", 1)[1].split("\n}", 1)[0]
        write = PLATFORM.split("platform_do_upgrade() {", 1)[1]
        self.assertIn('image_is_compressed "$1"', check)
        self.assertLess(write.index('image_is_compressed "$image"'), write.index("mtd write - kernel"))


class ExternServices(unittest.TestCase):
    def test_scripts_come_from_the_configured_share(self):
        runner = (BASE / "etc/init.d/hh71vm-extern-services").read_text()
        self.assertNotIn("PREFIX=/mnt/extern/opkg", runner)
        self.assertIn("uci -q get hh71vm-extern.extern.target", runner)
        self.assertIn('script="${prefix}/etc/init.d/', runner)


class ReleaseWorkflow(unittest.TestCase):
    def test_dispatch_inputs_are_never_script_text(self):
        workflow = (ROOT / ".github/workflows/release-resume.yml").read_text()
        for block in re.findall(r"\n\s+run: \|\n((?:\s{10,}.*\n?)+)", workflow):
            self.assertNotIn("${{ inputs.", block)
        self.assertIn("RESUME_TAG: ${{ inputs.tag }}", workflow)


if __name__ == "__main__":
    unittest.main()
