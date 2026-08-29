import gzip
import hashlib
import re
import subprocess
import sys
import unittest
from pathlib import Path
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"
RAM_IMAGE = FIRMWARE / "openwrt-rtkmipsel-rtl8197f-hh71vm-nfjrom.bin"
RAM_IMAGE_SHA256 = "0dd334f2c05076498bea51668f8ba45ac3fb5651faadfd685c06939d22d8ca52"
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
        manifest = (FIRMWARE / "openwrt-rtkmipsel-rtl8197f-hh71vm.manifest").read_text()
        packages = {line.split(" - ", 1)[0] for line in manifest.splitlines()}
        self.assertTrue({"ca-bundle", "libmbedtls12", "libustream-mbedtls20150806",
                         "iptables", "libxtables12"}.issubset(packages))
        config = (ROOT / "openwrt-feed/build.config").read_text()
        for name in ("ca-bundle", "libmbedtls", "libustream-mbedtls"):
            self.assertIn(f"CONFIG_PACKAGE_{name}=y\n", config)
        self.assertIn("CONFIG_PACKAGE_kmod-wireguard=m\n", config)

    def test_published_ram_image_hash(self):
        digest = hashlib.sha256(RAM_IMAGE.read_bytes()).hexdigest()
        self.assertEqual(digest, RAM_IMAGE_SHA256)

    def test_every_published_file_has_a_matching_checksum(self):
        """Nothing ships from firmware/ without a digest anyone can check.

        This replaced a rule that forbade flash images outright, which belonged to
        the RAM-only test release.
        """
        listed = {}
        for line in (FIRMWARE / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            digest, name = line.split(None, 1)
            listed[name.strip().lstrip("*")] = digest

        for path in sorted(FIRMWARE.iterdir()):
            if not path.is_file() or path.name == "SHA256SUMS":
                continue
            with self.subTest(name=path.name):
                self.assertIn(path.name, listed)
                self.assertEqual(
                    hashlib.sha256(path.read_bytes()).hexdigest(), listed[path.name]
                )

        for name in listed:
            with self.subTest(listed=name):
                self.assertTrue((FIRMWARE / name).is_file())

    def test_public_text_is_english_only(self):
        offenders = []
        for path in ROOT.rglob("*"):
            if not path.is_file():
                continue
            relative = path.relative_to(ROOT)
            if ".git" in relative.parts or "__pycache__" in relative.parts or relative.parts[0] == "dist":
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
            if ".git" in relative.parts or "__pycache__" in relative.parts or relative.parts[0] == "dist":
                continue
            if path.suffix.lower() in EXCLUDED_TEXT_SUFFIXES:
                continue
            if any(part.endswith("-logs") for part in relative.parts):
                continue
            data = path.read_bytes()
            if any(pattern.search(data) for pattern in private_paths):
                offenders.append(relative.as_posix())
        self.assertEqual(offenders, [])

    def test_package_index_matches_published_packages(self):
        package_dir = ROOT / "packages"
        index = (package_dir / "Packages").read_text(encoding="utf-8")
        self.assertEqual(gzip.decompress((package_dir / "Packages.gz").read_bytes()),
                         index.encode("utf-8"))

        entries = {}
        for block in index.strip().split("\n\n"):
            fields = dict(line.split(": ", 1) for line in block.splitlines()
                          if ": " in line)
            entries[fields["Filename"]] = fields

        for name, fields in entries.items():
            path = package_dir / name
            with self.subTest(name=name):
                self.assertTrue(path.is_file())
                self.assertEqual(path.stat().st_size, int(fields["Size"]))
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),
                                 fields["SHA256sum"])

        published = {path.name for path in package_dir.glob("*.ipk")}
        self.assertEqual(set(entries), published)

    def test_flash_image_can_be_validated_without_router_access(self):
        tool = ROOT / "tools" / "flash" / "install_openwrt_lan.py"
        image = FIRMWARE / "openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin"
        result = subprocess.run(
            [
                sys.executable,
                str(tool),
                "--image",
                str(image),
                "--dry-run",
                "--skip-backup",
                "--yes",
                "--pc-ip",
                "192.168.1.50",
            ],
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("DRY RUN: nothing was written to flash", result.stdout)
        ranges = [(int(start, 16), int(end, 16)) for start, end in re.findall(
            r"reaches flash: 0x([0-9A-F]+)\.\.0x([0-9A-F]+)", result.stdout
        )]
        self.assertEqual(ranges, [(0x300000, 0x5BE001), (0x030000, 0x1C6B71)])
        self.assertTrue(all(end < 0xC00000 for _, end in ranges))

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
