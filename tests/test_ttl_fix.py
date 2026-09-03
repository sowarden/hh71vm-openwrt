"""Host regressions for the TTL Fix: userspace iptables extensions and failure handling."""
import shutil
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TTL = ROOT / "openwrt-feed/package/utils/modem-extra-tools/files/ttl.lua"
BUILD = ROOT / "autobuild/build.py"


class IpoptUserspaceTests(unittest.TestCase):
    """The kernel targets are built out of tree, so the matching .so files must be named."""

    def test_build_names_the_ipopt_extensions_explicitly(self):
        build = BUILD.read_text(encoding="utf-8")
        self.assertIn('anchor = "$(eval $(call BuildPlugin,iptables-mod-ipopt,$(IPT_IPOPT-m)))"', build)
        self.assertIn('raise ValueError("ipopt userspace plugin patch context changed")', build)
        for extension in ("ipt_TTL", "ipt_ttl", "xt_HL", "xt_hl"):
            self.assertIn(extension, build)

    def test_kernel_modules_and_userspace_extensions_stay_in_step(self):
        modules = (ROOT / "openwrt-feed/package/utils/hh71vm-ipt-ipopt/Makefile").read_text(encoding="utf-8")
        build = BUILD.read_text(encoding="utf-8")
        extensions = next(
            line.split('"')[1] for line in build.splitlines() if line.strip().startswith("extensions = ")
        ).split()
        for module in modules.split("IPOPT_MODULES:=")[1].splitlines()[0].split():
            # xt_HL and xt_hl carry the IPv6 names; their IPv4 halves are separate libraries.
            self.assertIn(module, extensions, module + " has no userspace extension")
        self.assertIn("ipt_TTL", extensions)
        self.assertIn("ipt_ttl", extensions)


class TtlFailureHandlingTests(unittest.TestCase):
    def test_the_real_iptables_diagnostic_reaches_the_user(self):
        ttl = TTL.read_text(encoding="utf-8")
        self.assertNotIn("missing module or incompatible userspace", ttl)
        self.assertIn("rejected the TTL/HL rules: ", ttl)
        self.assertIn("' 2>' .. c.quote(report)", ttl)
        self.assertIn("install iptables-mod-ipopt and kmod-ipt-ipopt", ttl)

    def test_a_failed_rollback_switches_the_feature_off(self):
        ttl = TTL.read_text(encoding="utf-8")
        self.assertIn("off.enabled=false", ttl)
        self.assertIn("TTL Fix switched off", ttl)
        # The saved state must never be one the board cannot reapply at the next boot.
        self.assertIn("local cleared,clear_error=pcall(function() T.apply(off); T.save(off) end)", ttl)

    @unittest.skipUnless(shutil.which("luac5.1") or shutil.which("luac"), "no Lua compiler")
    def test_the_backend_still_compiles(self):
        luac = shutil.which("luac5.1") or shutil.which("luac")
        subprocess.run([luac, "-p", str(TTL)], check=True)


if __name__ == "__main__":
    unittest.main()
