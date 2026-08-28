import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUNDLE = ROOT / "extras" / "modem-extra-tools"
BUILDER = ROOT / "tools" / "release" / "build-package-bundle.py"


class PackageBundleTests(unittest.TestCase):
    def test_modem_bundle_builds_from_published_packages(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run(
                [
                    sys.executable,
                    str(BUILDER),
                    str(BUNDLE),
                    "--output-dir",
                    temporary,
                ],
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            archive_path = Path(result.stdout.strip())
            self.assertEqual(
                archive_path.name, "modem-extra-tools-1.1.0-hh71vm.zip"
            )

            with zipfile.ZipFile(archive_path) as archive:
                root = "modem-extra-tools-1.1.0/"
                names = set(archive.namelist())
                expected = {
                    root + "README.md",
                    root + "install.sh",
                    root + "SHA256SUMS",
                    root + "bundle.env",
                    root + "COMPATIBILITY.txt",
                    root + "LICENSE-APACHE-2.0",
                    root + "LICENSE-GPL-2.0",
                    root + "kmod-hh71vm-ipt-ipopt_4.14.275-1_mipsel_24kc.ipk",
                    root + "luci-app-modem-extra-tools_1.1.0-1_all.ipk",
                    root + "modem-extra-tools_1.1.0-1_mipsel_24kc.ipk",
                }
                self.assertEqual(names, expected)
                self.assertNotIn(b"\r", archive.read(root + "install.sh"))
                self.assertTrue(archive.getinfo(root + "install.sh").external_attr >> 16 & 0o111)

                checksums = {}
                for line in archive.read(root + "SHA256SUMS").decode().splitlines():
                    digest, name = line.split(None, 1)
                    checksums[name.strip()] = digest
                self.assertEqual(set(checksums), {
                    name.removeprefix(root)
                    for name in expected
                    if name != root + "SHA256SUMS"
                })
                for name, digest in checksums.items():
                    self.assertEqual(
                        hashlib.sha256(archive.read(root + name)).hexdigest(), digest
                    )

    def test_builder_output_is_deterministic(self):
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            outputs = []
            for output_dir in (first, second):
                result = subprocess.run(
                    [
                        sys.executable,
                        str(BUILDER),
                        str(BUNDLE),
                        "--output-dir",
                        output_dir,
                    ],
                    capture_output=True,
                    text=True,
                    timeout=30,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                outputs.append(Path(result.stdout.strip()).read_bytes())
            self.assertEqual(outputs[0], outputs[1])

    def test_rejects_invalid_manifests(self):
        manifest = json.loads((BUNDLE / "bundle.json").read_text())
        cases = [
            ("name", "../escape"),
            ("version", "../../escape"),
            ("files", ["../README.md"]),
            ("files", ["C:/escape"]),
            ("files", ["README.md", "README.md"]),
            ("files", ["SHA256SUMS"]),
            ("files", ["README.md", "install.sh", manifest["packages"][0]]),
            ("architecture", "aarch64_generic"),
            ("kernel", "4.14.275-wrong-abi"),
            ("sha256", {}),
            ("sha256", {name: "0" * 64 for name in manifest["packages"]}),
        ]
        for field, value in cases:
            with self.subTest(field=field, value=value), tempfile.TemporaryDirectory() as tmp:
                bundle = Path(tmp) / "bundle"
                shutil.copytree(BUNDLE, bundle)
                data = dict(manifest, **{field: value})
                (bundle / "bundle.json").write_text(json.dumps(data))
                result = subprocess.run(
                    [sys.executable, str(BUILDER), str(bundle), "--output-dir", str(Path(tmp) / "out")],
                    capture_output=True, text=True, timeout=30,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(list(Path(tmp).rglob("*.zip")), [])

    def test_missing_package_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run(
                [sys.executable, str(BUILDER), str(BUNDLE), "--package-dir", tmp,
                 "--output-dir", str(Path(tmp) / "out")],
                capture_output=True, text=True, timeout=30,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(list(Path(tmp).rglob("*.zip")), [])

    def test_helper_hashes_cover_sources_and_arm_binaries(self):
        package = ROOT / "openwrt-feed/package/utils/modem-extra-tools"
        entries = {}
        for line in (package / "files/helpers.sha256").read_text().splitlines():
            digest, name = line.split()
            entries[name] = digest
            path = package / ("src" if name.endswith(".c") else "files") / name
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), digest)
            if name.endswith("-arm"):
                self.assertEqual(path.read_bytes()[:6], b"\x7fELF\x01\x01")
                self.assertEqual(path.read_bytes()[18:20], b"\x28\x00")
        self.assertEqual(set(entries), {
            "hh71-nas.c", "hh71-imei.c", "hh71-nas-arm", "hh71-imei-arm"
        })

    def test_kernel_payload_contains_ten_mips_modules(self):
        package = ROOT / "packages/kmod-hh71vm-ipt-ipopt_4.14.275-1_mipsel_24kc.ipk"
        with tarfile.open(package) as archive:
            payload = archive.extractfile("./data.tar.gz").read()
            control = archive.extractfile("./control.tar.gz").read()
        with tarfile.open(fileobj=io.BytesIO(control)) as archive:
            metadata = archive.extractfile("./control").read().decode()
            self.assertIn("kernel (=4.14.275-1-2709aa412f796f4f2600f70163b49915)", metadata)
        with tarfile.open(fileobj=io.BytesIO(payload)) as archive:
            modules = [member for member in archive if member.name.endswith(".ko")]
            self.assertEqual(len(modules), 10)
            for member in modules:
                data = archive.extractfile(member).read()
                self.assertEqual(data[:6], b"\x7fELF\x01\x01")
                self.assertEqual(data[18:20], b"\x08\x00")
                # Check the package ABI, not an optional embedded vermagic string.
                self.assertEqual(Path(member.name).parent.as_posix(), "lib/modules/4.14.275")

    def test_package_index_generator_keeps_virtual_packages(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name in ("kernel_*.ipk", "libc_*.ipk", "*modem-extra-tools*.ipk"):
                for source in (ROOT / "packages").glob(name):
                    shutil.copyfile(source, Path(tmp) / source.name)
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools/release/build-package-index.py"), tmp],
                capture_output=True, text=True, timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            index = (Path(tmp) / "Packages").read_text()
            self.assertIn("Package: kernel\n", index)
            self.assertIn("Package: libc\n", index)


@unittest.skipUnless(os.name == "posix", "installer mocks require a POSIX shell")
class InstallerGuardTests(unittest.TestCase):
    def test_generated_installer_uses_exact_package_variables(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            built = subprocess.run([sys.executable, str(BUILDER), str(BUNDLE), "--output-dir", tmp],
                                   capture_output=True, text=True, check=True)
            with zipfile.ZipFile(built.stdout.strip()) as archive:
                archive.extractall(base / "unpacked")
            bundle = base / "unpacked/modem-extra-tools-1.1.0"
            commands = base / "bin"
            commands.mkdir()
            checksum = "5c8f819ffc49d05a6ca7acd431a279fbcf73f6e5406f3ad821a2fb93d311325d"
            stubs = {
                "id": "echo 0",
                "cat": "echo hh71vm",
                "opkg": '''if [ "$1" = status ]; then
  echo 'Version: 4.14.275-1-2709aa412f796f4f2600f70163b49915'
elif [ "$1" = install ]; then
  shift
  printf '%s\\n' "$*" >> "$TEST_LOG"
fi''',
                "sha256sum": ":",
                "wget": ":",
                "gzip": ":",
                "usign": ":",
                "awk": f"echo {checksum}",
            }
            for name, body in stubs.items():
                path = commands / name
                path.write_text("#!/bin/sh\n" + body + "\n")
                path.chmod(0o755)
            log = base / "installs"
            environment = dict(os.environ, PATH=str(commands) + os.pathsep + os.environ["PATH"], TEST_LOG=str(log))
            result = subprocess.run(["sh", str(bundle / "install.sh")], env=environment,
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            installs = log.read_text().splitlines()
            self.assertEqual(len(installs), 3)
            self.assertEqual(installs[0], "./kmod-hh71vm-ipt-ipopt_4.14.275-1_mipsel_24kc.ipk")
            self.assertTrue(installs[1].endswith("/iptables-mod-ipopt_1.8.3-1_mipsel_24kc.ipk"))
            self.assertEqual(installs[2], "./modem-extra-tools_1.1.0-1_mipsel_24kc.ipk ./luci-app-modem-extra-tools_1.1.0-1_all.ipk")

    def test_failures_do_not_install_packages(self):
        cases = [
            ({"TEST_UID": "1000"}, "as root"),
            ({"TEST_BOARD": "other-board"}, "HH71VM OpenWrt port"),
            ({"TEST_KERNEL": "wrong"}, "Kernel mismatch"),
            ({"TEST_BAD_SUM": "1"}, "FAILED"),
            ({"TEST_BAD_DOWNLOAD": "1"}, "download failed"),
            ({"TEST_BAD_SIGNATURE": "1"}, "signature failed"),
        ]
        for settings, expected in cases:
            with self.subTest(settings=settings), tempfile.TemporaryDirectory() as tmp:
                base = Path(tmp)
                commands = base / "bin"
                commands.mkdir()
                installer = base / "install.sh"
                installer.write_bytes((BUNDLE / "install.sh").read_bytes().replace(b"\r\n", b"\n"))
                (base / "SHA256SUMS").write_text("test fixture")
                (base / "bundle.env").write_text(
                    "expected_kernel='4.14.275-1-2709aa412f796f4f2600f70163b49915'\n")
                stubs = {
                    "id": 'echo "${TEST_UID:-0}"',
                    "cat": 'echo "${TEST_BOARD:-hh71vm}"',
                    "opkg": '''if [ "$1" = status ]; then
  echo "Version: ${TEST_KERNEL:-4.14.275-1-2709aa412f796f4f2600f70163b49915}"
elif [ "$1" = install ]; then
  echo forbidden-install >> "$TEST_LOG"
  exit 99
fi''',
                    "sha256sum": 'if [ "${TEST_BAD_SUM:-0}" = 1 ]; then echo FAILED >&2; exit 1; fi',
                    "wget": 'if [ "${TEST_BAD_DOWNLOAD:-0}" = 1 ]; then echo "download failed" >&2; exit 1; fi',
                    "gzip": ':',
                    "usign": 'if [ "${TEST_BAD_SIGNATURE:-0}" = 1 ]; then echo "signature failed" >&2; exit 1; fi',
                }
                for name, body in stubs.items():
                    path = commands / name
                    path.write_text("#!/bin/sh\n" + body + "\n")
                    path.chmod(0o755)
                environment = dict(os.environ, PATH=str(commands) + os.pathsep + os.environ["PATH"],
                                   TEST_LOG=str(base / "installs"), **settings)
                result = subprocess.run(["sh", str(installer)], env=environment,
                                        capture_output=True, text=True, timeout=30)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(expected, result.stdout + result.stderr)
                self.assertFalse((base / "installs").exists())


if __name__ == "__main__":
    unittest.main()
