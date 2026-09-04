"""Host regressions for the Xray packages and the Go toolchain corrections.

The binary this package builds is unusable on the board unless two corrections are made
to the Go toolchain first: without them it either dies in `futexwakeup` before `main()`
(`_ENOSYS` is 89 on MIPS, not 38) or panics the kernel (an eventfd registered in an
epoll set). Both used to be applied by hand. These tests exist so they cannot quietly
stop being applied by the build.

The patcher is exercised against the real go1.26.1 sources, kept in tests/data, rather
than against a paraphrase of them - a patch that no longer matches upstream is exactly
the failure worth catching.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "openwrt-feed/package/net/xray-core"
MAKEFILE = (PACKAGE / "Makefile").read_text(encoding="utf-8")
EXAMPLE = (PACKAGE / "files/config.example.json").read_text(encoding="utf-8")
# The comments in these files talk about the very strings the tests forbid, so the
# checks below look at the code, not at the prose explaining it.
RECIPES = "\n".join(line for line in MAKEFILE.splitlines()
                    if not line.lstrip().startswith("#"))
EXAMPLE_JSON = "\n".join(line for line in EXAMPLE.splitlines()
                         if not line.strip().startswith("//"))
GO_FIXTURES = ROOT / "tests/data/go1.26.1"
LOCK = json.loads((ROOT / "autobuild/lock.json").read_text(encoding="utf-8"))
CONFIG = (ROOT / "openwrt-feed/build.config").read_text(encoding="utf-8").splitlines()


class PackageSplitTests(unittest.TestCase):
    def test_the_service_package_does_not_depend_on_the_binary(self):
        # The dependency looks obviously right and is the trap: `opkg install xray`
        # would then pull 34 MB into a 6 MiB overlay. hh71vm-xray locates the binary.
        service = MAKEFILE.split("define Package/xray\n", 1)[1].split("endef", 1)[0]
        self.assertNotIn("xray-core", service)

    def test_both_packages_are_selected_in_both_configuration_files(self):
        # lock.json is written over build.config by rewrite_config(), so a selection in
        # one file alone silently does not happen.
        for name in ("xray", "xray-core"):
            key = "CONFIG_PACKAGE_" + name
            self.assertEqual(LOCK["config"].get(key), "m", key + " missing from lock.json")
            self.assertIn(key + "=m", CONFIG, key + " missing from build.config")

    def test_the_binary_is_not_built_into_the_image(self):
        # 34 MB against about 100 KB of rootfs slack. =y would fail the image size check
        # in a way that reads as an unrelated build error.
        for name in ("xray", "xray-core"):
            self.assertNotEqual(LOCK["config"].get("CONFIG_PACKAGE_" + name), "y")


class GoToolchainTests(unittest.TestCase):
    def test_both_host_architectures_are_pinned(self):
        # Both supported build-host architectures need pinned hashes. A missing hash
        # would make the download unverified rather than fail loudly.
        self.assertIn("GO_HASH_amd64:=", MAKEFILE)
        self.assertIn("GO_HASH_arm64:=", MAKEFILE)

    def test_the_patcher_runs_inside_prepare(self):
        prepare = MAKEFILE.split("define Build/Prepare", 1)[1].split("endef", 1)[0]
        self.assertIn("patch-go-toolchain.py", prepare)

    def test_the_module_cache_is_outside_the_package_build_directory(self):
        # Go marks its module cache read-only, so `rm -rf $(PKG_BUILD_DIR)` fails and
        # takes `make package/xray-core/clean` with it.
        self.assertIn("GO_WORK_DIR:=$(BUILD_DIR)/", MAKEFILE)
        self.assertNotIn("GO_WORK_DIR:=$(PKG_BUILD_DIR)", MAKEFILE)

    def test_no_tar_variable(self):
        # OpenWrt 19.07 defines no $(TAR); it expanded to nothing and the recipe became
        # " -xzf ...", which failed as an unreadable parallel-make error.
        self.assertNotIn("$(TAR)", RECIPES)


@unittest.skipUnless(GO_FIXTURES.is_dir(), "requires the go1.26.1 source fixtures")
class ToolchainPatcherTests(unittest.TestCase):
    """Run the real patcher over the real upstream files."""

    def patch(self, goroot):
        return subprocess.run(
            [sys.executable, str(PACKAGE / "src/patch-go-toolchain.py"), str(goroot),
             "--netpoll", str(PACKAGE / "src/go-netpoll-pipe.py")],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def make_goroot(self, temporary):
        goroot = Path(temporary) / "goroot"
        (goroot / "src/runtime").mkdir(parents=True)
        for name in ("defs_linux_mipsx.go", "netpoll_epoll.go"):
            shutil.copyfile(GO_FIXTURES / "src/runtime" / name, goroot / "src/runtime" / name)
        return goroot

    def test_it_corrects_both_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            goroot = self.make_goroot(temporary)
            result = self.patch(goroot)
            self.assertEqual(result.returncode, 0, result.stdout)
            defs = (goroot / "src/runtime/defs_linux_mipsx.go").read_text(encoding="utf-8")
            netpoll = (goroot / "src/runtime/netpoll_epoll.go").read_text(encoding="utf-8")
            # 0x59 is 89: ENOSYS on MIPS. 0x26 is 38, the value every other architecture
            # uses, and the reason Go's futex_time64 fallback never fires on 4.14 here.
            self.assertIn("_ENOSYS = 0x59", defs)
            self.assertNotIn("_ENOSYS = 0x26", defs)
            self.assertNotIn("netpollEventFd", netpoll)
            self.assertIn("netpollBreakRd", netpoll)
            self.assertIn("nonblockingPipe", netpoll)

    def test_it_is_idempotent(self):
        with tempfile.TemporaryDirectory() as temporary:
            goroot = self.make_goroot(temporary)
            self.assertEqual(self.patch(goroot).returncode, 0)
            second = self.patch(goroot)
            self.assertEqual(second.returncode, 0, second.stdout)

    def test_it_fails_loudly_when_upstream_moves(self):
        # A silently skipped correction produces a binary that panics the board, and it
        # looks exactly like a good one until it does.
        with tempfile.TemporaryDirectory() as temporary:
            goroot = self.make_goroot(temporary)
            path = goroot / "src/runtime/defs_linux_mipsx.go"
            path.write_text(path.read_text(encoding="utf-8").replace("_ENOSYS = 0x26",
                                                                     "_ENOSYS = 0x27"),
                            encoding="utf-8")
            result = self.patch(goroot)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("_ENOSYS", result.stdout)


class ShippedConfigurationTests(unittest.TestCase):
    def test_the_example_does_not_use_xtls_vision(self):
        # Measured 2026-09-03: with xtls-rprx-vision this board truncates downloads from
        # real HTTPS sites 15 times out of 16, and completes 6 of 6 without it.
        self.assertNotIn("xtls-rprx-vision", EXAMPLE_JSON)
        self.assertNotIn('"flow"', EXAMPLE_JSON)

    def test_the_example_explains_why(self):
        self.assertIn("vision", EXAMPLE.lower())
        self.assertIn("15", EXAMPLE)

    def test_the_example_keeps_lan_traffic_off_the_proxy(self):
        # Without this rule, enabling the proxy takes the web interface and SSH with it.
        self.assertIn("192.168.0.0/16", EXAMPLE)

    def test_the_service_is_off_by_default(self):
        settings = (PACKAGE / "files/xray.config").read_text(encoding="utf-8")
        self.assertIn("option enabled '0'", settings)


@unittest.skipUnless(os.name == "posix" and shutil.which("sh"), "requires a POSIX shell")
class LauncherShellTests(unittest.TestCase):
    def test_the_launcher_parses(self):
        for name in ("files/hh71vm-xray", "files/xray.init"):
            result = subprocess.run(["sh", "-n", str(PACKAGE / name)],
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(result.returncode, 0, name + ": " + result.stdout)

    def test_the_launcher_prefers_the_share_over_usr_bin(self):
        source = (PACKAGE / "files/hh71vm-xray").read_text(encoding="utf-8")
        candidates = source.split("candidates() {", 1)[1].split("\n}", 1)[0]
        self.assertLess(candidates.index("EXTERN_PREFIX"), candidates.index("/usr/bin/xray"))


if __name__ == "__main__":
    unittest.main()
