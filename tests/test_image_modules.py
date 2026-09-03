"""The CIFS modules have to be IN the image, not in the feed.

`/mnt/extern` is mounted at boot, so the modules that make the mount possible cannot be
something the owner installs afterwards. Two files decide this and they are applied in
order: `openwrt-feed/build.config` is the base, and `autobuild/lock.json` is written over
it by `rewrite_config()`. A `=y` in the first one alone is silently undone by an `=m` in
the second, which is exactly what happened once.
"""
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / "autobuild/lock.json").read_text(encoding="utf-8"))
CONFIG = (ROOT / "openwrt-feed/build.config").read_text(encoding="utf-8").splitlines()

# Measured on the device: exactly the modules that let `mount -t cifs` succeed.
EXTERN_PACKAGES = (
    "kmod-fs-cifs", "kmod-nls-base",
    "kmod-crypto-hash", "kmod-crypto-manager", "kmod-crypto-aead", "kmod-crypto-pcompress",
    "kmod-crypto-null", "kmod-crypto-hmac", "kmod-crypto-md5", "kmod-crypto-md4",
    "kmod-crypto-des", "kmod-crypto-ecb", "kmod-crypto-sha256",
)


class ExternModulesInImageTests(unittest.TestCase):
    def test_the_lock_selects_them_for_the_image(self):
        for name in EXTERN_PACKAGES:
            key = "CONFIG_PACKAGE_" + name
            self.assertEqual(LOCK["config"].get(key), "y", key + " must be built into the image")

    def test_the_base_configuration_agrees_with_the_lock(self):
        for name in EXTERN_PACKAGES:
            self.assertIn("CONFIG_PACKAGE_" + name + "=y", CONFIG)

    def test_nls_base_is_not_dropped_again(self):
        # kmod-fs-cifs depends on it. Left at =m it drags kmod-fs-cifs back to =m during
        # defconfig, and the image ends up with the crypto modules but no cifs.ko.
        self.assertEqual(LOCK["config"].get("CONFIG_PACKAGE_kmod-nls-base"), "y")


if __name__ == "__main__":
    unittest.main()
