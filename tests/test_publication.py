import re
import subprocess
import sys
import unittest
from pathlib import Path
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[1]
CYRILLIC_UTF8 = re.compile(rb"(?:\xd0[\x80-\xbf]|\xd1[\x80-\xbf])")
EXCLUDED_TEXT_SUFFIXES = {
    ".bin",
    ".gif",
    ".gz",
    ".ipk",
    ".jpeg",
    ".jpg",
    ".png",
    ".pyc",
    ".webp",
}
MODEM_HELPERS = {
    "openwrt-feed/package/utils/modem-extra-tools/files/hh71-nas-arm",
    "openwrt-feed/package/utils/modem-extra-tools/files/hh71-imei-arm",
}


class PublicationBoundaryTests(unittest.TestCase):
    def test_workflows_pin_actions_and_container_has_cross_libc_headers(self):
        workflows = "\n".join(path.read_text() for path in
                              (ROOT / ".github/workflows").glob("*.yml"))
        references = re.findall(r"uses:\s+(actions/[^@\s]+)@([^\s]+)", workflows)
        self.assertTrue(references)
        self.assertTrue(all(re.fullmatch(r"[0-9a-f]{40}", revision)
                            for _, revision in references))
        dockerfile = (ROOT / "autobuild/Dockerfile").read_text()
        self.assertIn("libc6-dev-armel-cross", dockerfile)
        autobuild = (ROOT / ".github/workflows/autobuild.yml").read_text()
        self.assertIn("actions/cache/restore@", autobuild)
        self.assertIn("actions/cache/save@", autobuild)
        self.assertIn("if: always() && steps.identity.outcome == 'success'", autobuild)
        self.assertIn('cache_root="$(dirname "$RUNNER_TEMP")/hh71vm-cache"', autobuild)
        self.assertNotIn("${{ runner.temp }}/hh71vm-cache", autobuild)
        mask = autobuild.index("Mask self-hosted runner identity and paths")
        build_checkout = autobuild.index("actions/checkout@", mask)
        self.assertLess(mask, build_checkout)
        for value in ("$HOME", "$RUNNER_NAME", "$RUNNER_TEMP", "$GITHUB_WORKSPACE"):
            self.assertIn(value, autobuild[mask:build_checkout])

    def test_test_branch_cannot_publish_a_release(self):
        workflow = (ROOT / ".github/workflows/autobuild.yml").read_text()
        recovery = (ROOT / ".github/workflows/release-resume.yml").read_text()
        validation = (ROOT / ".github/workflows/validate.yml").read_text()
        publisher = (ROOT / "autobuild/publish.py").read_text()
        self.assertIn("if: github.ref == 'refs/heads/main'", workflow)
        self.assertIn("if: github.ref == 'refs/heads/openwrt-autobuild'", workflow)
        self.assertNotIn("pull_request:", workflow + validation)
        self.assertIn('args.command == "publish"', publisher)
        self.assertIn('os.environ.get("GITHUB_REF") != "refs/heads/main"', publisher)
        self.assertIn("workflow_dispatch:", recovery)
        self.assertNotIn("push:", recovery)
        self.assertNotIn("self-hosted", recovery)
        self.assertIn("github.ref == 'refs/heads/openwrt-autobuild'", recovery)
        self.assertIn("artifact-ids: ${{ inputs.artifact-id }}", recovery)
        self.assertIn("run-id: ${{ inputs.source-run-id }}", recovery)
        self.assertIn("GH_TOKEN: ${{ github.token }}", recovery)
        self.assertIn("contents: write", recovery)
        self.assertIn("actions: write", recovery)
        self.assertIn('args.command == "resume"', publisher)
        self.assertIn('event != "workflow_dispatch"', publisher)
        self.assertNotIn('api("immutable-releases")', publisher)
        checker = (ROOT / "autobuild/check-workflows.sh").read_text()
        for name in ("autobuild.yml", "release-resume.yml", "validate.yml"):
            self.assertIn(".github/workflows/" + name, checker)

    def test_current_release_has_no_hardware_test_marker(self):
        public = "\n".join((ROOT / path).read_text() for path in
                           ("README.md", "autobuild/build.py", "autobuild/common.py", "autobuild/publish.py"))
        self.assertNotIn("Not tested on hardware", public)
        self.assertNotIn('"hardware_tested": False', public)

    def test_base_image_preserves_https_dependencies(self):
        config = (ROOT / "openwrt-feed/build.config").read_text()
        for name in ("ca-bundle", "libmbedtls", "libustream-mbedtls"):
            self.assertIn(f"CONFIG_PACKAGE_{name}=y\n", config)
        self.assertIn("CONFIG_PACKAGE_kmod-wireguard=m\n", config)

    def test_autosysupgrade_is_fail_closed_and_embedded(self):
        updater = (ROOT /
                   "openwrt-feed/target/linux/rtkmipsel/base-files/usr/sbin/autosysupgrade")
        text = updater.read_text()
        self.assertTrue(text.startswith("#!/bin/sh\n"))
        for marker in (
            "release.json.sig",
            "usign -V",
            "sha256sum",
            "sysupgrade -T",
            "installed build is newer than the latest public Release",
            "--force",
            'architecture" = mipsel_24kc',
            'board_name 2>/dev/null || true)" = hh71vm',
            "releases/download/$tag",
            "--check-json",
            "--status-json",
            "--expected",
            "history_complete",
            'LC_ALL=C sort -r "$temporary/release-order"',
            'valid_date "$history_date"',
        ):
            self.assertIn(marker, text)
        self.assertNotIn("--no-check-certificate", text)
        self.assertIn('[ "$installed" = "$tag" ] && [ "$keep_config" = 1 ] && [ "$check_only" = 0 ]', text)
        inspector = (ROOT / "autobuild/inspect_image.py").read_text()
        self.assertIn('files.get("usr/sbin/autosysupgrade")', inspector)

    def test_luci_firmware_updater_is_narrow_and_confirmed(self):
        frontend = (ROOT /
                    "openwrt-feed/target/linux/rtkmipsel/base-files/www/luci-static/resources/hh71vm/updater.js").read_text()
        patch = (ROOT / "openwrt-feed/patches/luci/200-hh71vm-firmware-updater.patch").read_text()
        for marker in ("Check Updates", "Upgrade Firmware", "--check-json", "--expected",
                       "ui.showModal", "ui.awaitReconnect", "state.update_available"):
            self.assertIn(marker, frontend)
        self.assertIn("/usr/sbin/autosysupgrade", patch)
        self.assertIn("'require hh71vm.updater as updater';", patch)
        self.assertNotIn("--force", frontend)
        self.assertNotIn("innerHTML", frontend)

    def test_release_notes_are_signed_and_human_authored(self):
        common = (ROOT / "autobuild/common.py").read_text()
        builder = (ROOT / "autobuild/build.py").read_text()
        publisher = (ROOT / "autobuild/publish.py").read_text()
        policy = (ROOT / "docs/release-notes.md").read_text()
        self.assertIn('read_release_notes(source / "release-notes.json")', builder)
        self.assertIn('manifest["changelog"]', publisher)
        self.assertIn("validate_changelog", common)
        self.assertIn("human review", policy)
        self.assertNotIn("git log", builder + publisher)

    def test_offline_modem_bootstrap_includes_direct_runtime_dependencies(self):
        guide = (ROOT / "docs/package-feed.md").read_text()
        for package in (
            "libuci-lua_*.ipk",
            "iptables-mod-ipopt_*.ipk",
            "kmod-hh71vm-ipt-ipopt_*.ipk",
            "modem-extra-tools_*.ipk",
            "luci-app-modem-extra-tools_*.ipk",
        ):
            self.assertIn(package, guide)

    def test_generated_publication_directories_are_not_versioned(self):
        for name in ("firmware", "packages", "extras", "dist"):
            with self.subTest(name=name):
                directory = ROOT / name
                self.assertFalse(directory.exists() and any(path.is_file()
                                                             for path in directory.rglob("*")))

    def test_documentation_uses_latest_only_for_human_downloads(self):
        documents = "\n".join(path.read_text(encoding="utf-8")
                              for path in ROOT.rglob("*.md") if ".git" not in path.parts)
        prefix = "https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/"
        for name in (
            "hh71vm-openwrt-flash-bundle.zip",
            "hh71vm-openwrt-flash-bundle.zip.sha256",
            "openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin",
            "openwrt-rtkmipsel-rtl8197f-hh71vm-sysupgrade.bin",
            "openwrt-rtkmipsel-rtl8197f-hh71vm-nfjrom.bin",
            "SHA256SUMS",
        ):
            self.assertIn(prefix + name, documents)
        self.assertNotIn(prefix + "Packages", documents)

    def test_public_text_is_english_only(self):
        offenders = []
        for path in ROOT.rglob("*"):
            if not path.is_file():
                continue
            relative = path.relative_to(ROOT)
            if ".git" in relative.parts or "__pycache__" in relative.parts:
                continue
            if path.suffix.lower() in EXCLUDED_TEXT_SUFFIXES:
                continue
            if any(part.endswith("-logs") for part in relative.parts):
                continue
            if relative.as_posix().endswith("/usr/sbin/iwpriv"):
                continue
            if relative.as_posix() in MODEM_HELPERS:
                self.assertEqual(path.read_bytes()[:4], b"\x7fELF")
                continue
            if CYRILLIC_UTF8.search(path.read_bytes()):
                offenders.append(relative.as_posix())
        self.assertEqual(offenders, [])

    def test_public_text_has_no_private_workspace_paths(self):
        private_paths = (
            re.compile(rb"[A-Za-z]:\\Users\\[^\\/\r\n]+", re.IGNORECASE),
        )
        offenders = []
        for path in ROOT.rglob("*"):
            if not path.is_file():
                continue
            relative = path.relative_to(ROOT)
            if ".git" in relative.parts or "__pycache__" in relative.parts:
                continue
            if path.suffix.lower() in EXCLUDED_TEXT_SUFFIXES:
                continue
            if any(part.endswith("-logs") for part in relative.parts):
                continue
            data = path.read_bytes()
            if any(pattern.search(data) for pattern in private_paths):
                offenders.append(relative.as_posix())
        self.assertEqual(offenders, [])

    def test_rtk_mkimg_selftest_requires_explicit_inputs(self):
        tool = ROOT / "tools" / "flash" / "rtk_mkimg.py"
        result = subprocess.run(
            [sys.executable, str(tool), "selftest"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("--dump0", result.stderr)
        self.assertIn("--kernel-payload", result.stderr)

    def test_relative_markdown_links_resolve(self):
        missing = []
        link_pattern = re.compile(r"!?\[[^]]*\]\(([^)]+)\)")
        for document in ROOT.rglob("*.md"):
            relative = document.relative_to(ROOT)
            if ".git" in relative.parts:
                continue
            text = document.read_text(encoding="utf-8")
            text = re.sub(r"```.*?```", "", text, flags=re.DOTALL)
            for target in link_pattern.findall(text):
                target = target.split("#", 1)[0].strip()
                if not target or "://" in target or target.startswith("mailto:"):
                    continue
                destination = document.parent / unquote(target)
                if not destination.exists():
                    missing.append(f"{relative.as_posix()} -> {target}")
        self.assertEqual(missing, [])

    def test_issue_forms_keep_required_reporting_fields(self):
        template_dir = ROOT / ".github" / "ISSUE_TEMPLATE"
        compatibility = (template_dir / "01-compatibility-report.yml").read_text(
            encoding="utf-8"
        )
        bug = (template_dir / "02-bug-report.yml").read_text(encoding="utf-8")
        config = (template_dir / "config.yml").read_text(encoding="utf-8")

        for marker in (
            "id: outcome",
            "id: model",
            "id: board-revision",
            "id: stock-version",
            "id: image-sha256",
            "id: uart-log",
            "id: wifi-24",
            "id: wifi-5",
            "id: modem",
        ):
            self.assertIn(marker, compatibility)
        for marker in ("id: summary", "id: steps", "id: uart-log"):
            self.assertIn(marker, bug)
        self.assertIn("blank_issues_enabled: false", config)

    def test_device_backups_and_transfer_logs_are_gitignored(self):
        gitignore = (ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
        self.assertIn("backup-stock*/", gitignore)
        self.assertIn("tools/tftp-put-logs/", gitignore)
        self.assertIn("tools/flash/tftp-put-logs/", gitignore)


if __name__ == "__main__":
    unittest.main()
