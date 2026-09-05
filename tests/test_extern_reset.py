"""Regressions for first-boot cleanup of the external OpenWrt package area."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files"
RESET = BASE / "usr/sbin/hh71vm-extern-reset"
MOUNT = BASE / "usr/sbin/hh71vm-extern-mount"
DEFAULT = BASE / "etc/uci-defaults/95-hh71vm-extern-reset"
SERVICES = BASE / "etc/init.d/hh71vm-extern-services"
XRAY = ROOT / "openwrt-feed/package/net/xray-core/files/hh71vm-xray"
PACKAGE = BASE / "usr/sbin/hh71vm-extern-pkg"


@unittest.skipUnless(os.name == "posix" and shutil.which("flock"), "requires POSIX tools")
class ResetShellTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.share = self.root / "mnt/extern"
        self.pending = self.root / "etc/hh71vm-extern-reset-pending"
        self.opkg_conf = self.root / "etc/opkg/extern.conf"
        for name in ("mnt/extern", "etc/opkg", "var/lock", "proc"):
            (self.root / name).mkdir(parents=True, exist_ok=True)
        self.pending.write_text("pending\n")
        self.opkg_conf.write_text("dest extern unsafe\n")
        self.write_mounts()

        replacements = (
            ("/etc/hh71vm-extern-reset-pending", str(self.pending)),
            ("/etc/opkg/extern.conf", str(self.opkg_conf)),
            ("/var/lock/hh71vm-extern-pkg.lock", str(self.root / "var/lock/package.lock")),
            ("/proc/mounts", str(self.root / "proc/mounts")),
            ("/mnt/extern", str(self.share)),
            ("/mnt", str(self.root / "mnt")),
        )
        source = RESET.read_text(encoding="utf-8")
        for index, (old, _) in enumerate(replacements):
            source = source.replace(old, f"@@PATH{index}@@")
        for index, (_, new) in enumerate(replacements):
            source = source.replace(f"@@PATH{index}@@", new)
        self.script = self.root / "reset"
        self.script.write_text(source, encoding="utf-8")
        self.script.chmod(0o755)

    def write_mounts(self, extra="", options="rw"):
        text = f"//192.168.225.1/shared_rom {self.share} cifs {options} 0 0\n" + extra
        (self.root / "proc/mounts").write_text(text)

    def directory(self, name, files=("payload",)):
        path = self.share / name
        path.mkdir(parents=True)
        for filename in files:
            target = path / filename
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"old")
        return path

    def run_reset(self, success=True, command="run"):
        env = dict(os.environ, TARGET="/untrusted", SHARE="/untrusted",
                   PENDING="/untrusted", OPKG_CONF="/untrusted")
        result = subprocess.run(["sh", str(self.script), command], env=env,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode == 0, success, result.stdout)
        return result.stdout

    def test_clean_first_install_initializes_opkg(self):
        self.run_reset()
        self.assertTrue((self.share / "opkg").is_dir())
        self.assertEqual(list((self.share / "opkg").iterdir()), [])
        self.assertFalse(self.pending.exists())
        self.assertFalse(self.opkg_conf.exists())

    def test_upgrade_removes_every_fixed_openwrt_area(self):
        for name in ("opkg", "bin", "control", "xray-loopback-test", "xray"):
            self.directory(name, ("nested/payload",))
        self.run_reset()
        self.assertTrue((self.share / "opkg").is_dir())
        for name in ("bin", "control", "xray-loopback-test", "xray"):
            self.assertFalse((self.share / name).exists())

    def test_normal_reboot_without_pending_marker_preserves_packages(self):
        self.pending.unlink()
        payload = self.directory("opkg") / "payload"
        self.run_reset()
        self.assertEqual(payload.read_bytes(), b"old")

    def test_missing_or_unmounted_share_fails_closed(self):
        old = self.directory("opkg") / "payload"
        (self.root / "proc/mounts").write_text("")
        self.run_reset(False)
        self.assertEqual(old.read_bytes(), b"old")
        self.assertTrue(self.pending.exists())

    def test_readonly_share_fails_closed(self):
        old = self.directory("opkg") / "payload"
        self.write_mounts(options="ro")
        self.run_reset(False)
        self.assertTrue(old.exists())

    def test_wrong_share_fails_closed(self):
        old = self.directory("opkg") / "payload"
        (self.root / "proc/mounts").write_text(f"//other/share {self.share} cifs rw 0 0\n")
        self.run_reset(False)
        self.assertTrue(old.exists())

    def test_nested_mount_fails_closed(self):
        old = self.directory("opkg") / "payload"
        self.write_mounts(f"tmpfs {self.share}/opkg/usr tmpfs rw 0 0\n")
        self.run_reset(False)
        self.assertTrue(old.exists())

    def test_interrupted_retired_directory_is_finished(self):
        retired = self.directory(".hh71vm-reset-opkg", ("partial",))
        self.directory("control")
        self.run_reset()
        self.assertFalse(retired.exists())
        self.assertFalse((self.share / "control").exists())
        self.assertTrue((self.share / "opkg").is_dir())

    def test_marker_contents_cannot_choose_a_path(self):
        outside = self.share / "backup_conf"
        outside.write_bytes(b"keep")
        self.pending.write_text("../../backup_conf\n")
        self.directory("opkg")
        self.run_reset()
        self.assertEqual(outside.read_bytes(), b"keep")

    def test_unrelated_top_level_files_and_directories_survive(self):
        names = ("backup_conf", "fota", "sqlite3_integrity_check.log",
                 "sqlite3_integrity_check_failed.log", "traceability.txt", "other")
        for name in names:
            (self.share / name).write_bytes(b"keep")
        unrelated_directory = self.directory("modem-owned", ("data",))
        self.directory("opkg")
        self.run_reset()
        for name in names:
            self.assertEqual((self.share / name).read_bytes(), b"keep")
        self.assertEqual((unrelated_directory / "data").read_bytes(), b"old")

    def test_top_level_symlink_is_refused(self):
        outside = self.directory("modem-owned")
        (self.share / "opkg").symlink_to(outside, target_is_directory=True)
        self.run_reset(False)
        self.assertEqual((outside / "payload").read_bytes(), b"old")

    def test_symlink_inside_owned_directory_is_unlinked_not_followed(self):
        outside = self.share / "traceability.txt"
        outside.write_bytes(b"keep")
        opkg = self.directory("opkg", ())
        (opkg / "link").symlink_to(outside)
        self.run_reset()
        self.assertEqual(outside.read_bytes(), b"keep")

    def test_unexpected_file_at_owned_path_is_refused(self):
        path = self.share / "control"
        path.write_bytes(b"unknown")
        self.run_reset(False)
        self.assertEqual(path.read_bytes(), b"unknown")

    def test_status_never_changes_the_share(self):
        old = self.directory("opkg") / "payload"
        output = self.run_reset(command="status")
        self.assertIn("pending: yes", output)
        self.assertEqual(old.read_bytes(), b"old")


class IntegrationTests(unittest.TestCase):
    def test_first_boot_hook_marks_cleanup_and_unpublishes_destination(self):
        source = DEFAULT.read_text(encoding="utf-8")
        self.assertIn("/etc/hh71vm-extern-reset-pending", source)
        self.assertIn("rm -f /etc/opkg/extern.conf", source)

    def test_mount_runs_cleanup_before_publishing_opkg(self):
        source = MOUNT.read_text(encoding="utf-8")
        prepare = source.split("prepare_share() {", 1)[1].split("\n}", 1)[0]
        self.assertLess(prepare.index('"$RESET_HELPER" run'), prepare.index("opkg_dest_sync"))
        self.assertIn("is_mounted && { prepare_share; return $?; }", source)

    def test_external_services_do_not_start_while_cleanup_is_pending(self):
        self.assertIn("[ ! -e /etc/hh71vm-extern-reset-pending ] || return 0",
                      SERVICES.read_text(encoding="utf-8"))

    def test_xray_does_not_run_an_old_external_binary(self):
        source = XRAY.read_text(encoding="utf-8")
        find_binary = source.split("find_binary() {", 1)[1].split("\n}", 1)[0]
        self.assertIn("/mnt/extern/*", find_binary)
        self.assertIn("/etc/hh71vm-extern-reset-pending", find_binary)

    def test_package_operations_serialize_against_cleanup(self):
        source = PACKAGE.read_text(encoding="utf-8")
        begin = source.split("begin_package_operation() {", 1)[1].split("\n}", 1)[0]
        self.assertIn('exec 8>"$PKG_LOCK"', begin)
        self.assertIn("flock 8", begin)
        for function in ("do_install", "do_remove"):
            body = source.split(function + "() {", 1)[1].split("\n}", 1)[0]
            self.assertIn("begin_package_operation", body)

    def test_pending_cleanup_hides_stale_package_state(self):
        source = PACKAGE.read_text(encoding="utf-8")
        listing = source.split("do_list() {", 1)[1].split("\n}", 1)[0]
        status = source.split("do_status() {", 1)[1].split("\n}", 1)[0]
        self.assertIn("cleanup pending", listing)
        self.assertIn("cleanup:   pending", status)

    def test_cleanup_targets_are_a_literal_allowlist(self):
        source = RESET.read_text(encoding="utf-8")
        self.assertIn("for name in opkg bin control xray-loopback-test xray; do", source)
        self.assertNotIn("eval ", source)
        self.assertNotIn("${TARGET", source)

    @unittest.skipUnless(os.name == "posix" and shutil.which("sh"), "requires a POSIX shell")
    def test_all_changed_shell_scripts_parse(self):
        for path in (RESET, MOUNT, DEFAULT, SERVICES, XRAY):
            result = subprocess.run(["sh", "-n", str(path)], text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(result.returncode, 0, path.name + ": " + result.stdout)


if __name__ == "__main__":
    unittest.main()
