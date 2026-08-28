"""Host checks for immutable releases; these do not execute router firmware."""
import base64
import copy
import gzip
import importlib.util
import io
import json
import lzma
import os
import shutil
import subprocess
import sys
import struct
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "autobuild"))
import common as auto


def module(name):
    spec = importlib.util.spec_from_file_location("autobuild_" + name, ROOT / "autobuild" / (name + ".py"))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


builder = module("build")
publisher = module("publish")
images = module("inspect_image")
COMMIT = "a" * 40
TAG = auto.identity(COMMIT, 42, 1)
KERNEL = "4.14.275-1-" + "b" * 32
KEY = auto.public_key(b"untrusted comment: test\n" + base64.b64encode(b"Ed" + b"12345678" + b"x" * 32) + b"\n")[1]


def package(name, depends="", version="1", **extra):
    record = dict(Package=name, Version=version, Architecture="mipsel_24kc", Description="Test package", **extra)
    if depends:
        record["Depends"] = depends
    return record


def tar_bytes(files):
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as archive:
        for name, content in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(content)
            archive.addfile(info, io.BytesIO(content))
    return buffer.getvalue()


def write_ipk(directory, record, payload=None):
    path = directory / "{Package}_{Version}_{Architecture}.ipk".format(**record)
    control = "".join(f"{key}: {value}\n" for key, value in record.items()).encode()
    path.write_bytes(tar_bytes({"debian-binary": b"2.0\n", "control.tar.gz": tar_bytes({"control": control}),
                               "data.tar.gz": tar_bytes(payload or {"usr/share/test": b"test"})}))
    return path


def candidate(directory):
    entries = []
    for name in auto.ROOTS:
        p = write_ipk(directory, package(name))
        entries.append((p, auto.ipk(p)))
    index = auto.make_index(entries)
    (directory / "Packages").write_bytes(index)
    (directory / "Packages.gz").write_bytes(gzip.compress(index, mtime=0))
    (directory / "hh71vm-feed.pub").write_bytes(KEY)
    auto.write_json(directory / "image-packages.json", [package("kernel", version=KERNEL)])
    for name in ("build.config", "build-evidence.json", "build-environment.json", "source-lock.json",
                 "source-delta.tar.gz", "upstream-sources.tar.gz", "upstream-buildsystem.tar.gz",
                 "download-checksums.json", "packages-bundle.zip"):
        (directory / name).write_bytes(b"test fixture\n")
    manifest = dict(schema=1, tag=TAG, source_commit=COMMIT, run_id=42, run_attempt=1,
                    architecture=auto.ARCHITECTURE, kernel=KERNEL, feed_url=auto.feed_url(TAG),
                    key_id=auto.public_key(KEY)[0], hardware_tested=False,
                    files={p.name: auto.sha256(p) for p in directory.iterdir()})
    auto.write_json(directory / "release.json", manifest)
    return manifest


class PackageTests(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("HH71VM_TEST_OPKG"), "optional pinned host opkg integration")
    def test_real_opkg_solver_accepts_complete_and_rejects_missing_dependency(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            output = root / "candidate"
            output.mkdir()
            candidate(output)
            image = root / "image"
            (image / "usr/lib/opkg/info").mkdir(parents=True)
            (image / "usr/lib/opkg/status").write_text(
                f"Package: kernel\nVersion: {KERNEL}\nArchitecture: mipsel_24kc\nStatus: install ok installed\n\n")
            (image / "usr/lib/opkg/info/kernel.list").write_text("")
            build = root / "build"
            (build / "staging_dir/host/bin").mkdir(parents=True)
            (build / "staging_dir/host/bin/opkg").symlink_to(os.environ["HH71VM_TEST_OPKG"])
            builder.solver_probe(build, image, output)
            index = output / "Packages"
            index.write_text(index.read_text().replace("Package: wireguard-tools\n", "Package: wireguard-tools\nDepends: missing-dependency\n"))
            with self.assertRaisesRegex(ValueError, "probe failed"):
                builder.solver_probe(build, image, output)

    def test_build_identity_never_uses_latest(self):
        tags = {auto.identity(COMMIT, 42, 1), auto.identity(COMMIT, 42, 2), auto.identity(COMMIT, 43, 1)}
        self.assertEqual(len(tags), 3)
        for tag in ("latest", "../escape", TAG + "/Packages"):
            with self.assertRaises(ValueError):
                auto.feed_url(tag)

    def test_existing_package_payloads_parse(self):
        paths = list((ROOT / "packages").glob("*.ipk"))
        self.assertTrue(paths)
        for path in paths:
            with self.subTest(path=path.name):
                auto.ipk(path)

    def test_empty_and_duplicate_control_fields(self):
        self.assertEqual(auto.fields("Depends:\nDescription: test\n continuation\n")["Depends"], "")
        with self.assertRaises(ValueError):
            auto.fields("Package: x\nPackage: y\n")

    @unittest.skipUnless(shutil.which("dpkg"), "requires dpkg version comparison")
    def test_dependency_closure_abi_and_providers(self):
        base = [package("kernel", version=KERNEL), package("libc", version="1.1")]
        pkgs = [package("app", "virtual-feature, libc (>= 1.0)"),
                package("kmod-custom", "kernel (= " + KERNEL + ")", Provides="virtual-feature")]
        self.assertEqual(auto.validate_closure(pkgs, base, roots=("app",)), KERNEL)
        for dependency in ("missing", "libc (>= 2.0)", "virtual-feature (= 1)"):
            broken = copy.deepcopy(pkgs)
            broken[0]["Depends"] = dependency
            with self.assertRaises(ValueError):
                auto.validate_closure(broken, base, roots=("app",))
        pkgs[1]["Depends"] = "kernel (= 4.14.275-1-other)"
        with self.assertRaisesRegex(ValueError, "ABI"):
            auto.validate_closure(pkgs, base, roots=("app",))

    def test_kernel_replacement_and_base_version_drift_rejected(self):
        base = [package("kernel", version=KERNEL), package("libc")]
        for pkgs in ([package("kernel", version=KERNEL)], [package("libc", version="2")]):
            with self.assertRaises(ValueError):
                auto.validate_closure(pkgs, base, roots=())

    def test_required_kmod_must_contain_mips_module(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            with self.assertRaisesRegex(ValueError, "empty"):
                auto.ipk(write_ipk(root, package("kmod-wireguard")))
            p = write_ipk(root, package("kmod-wireguard"), {"lib/modules/test.ko": b"not ELF"})
            with self.assertRaisesRegex(ValueError, "MIPS"):
                auto.ipk(p)

    def test_archive_traversal_and_renamed_package_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            p = write_ipk(root, package("test"), {"../bad": b"test"})
            with self.assertRaisesRegex(ValueError, "unsafe"):
                auto.ipk(p)
            p = write_ipk(root, package("test"))
            renamed = p.with_name("other_1_mipsel_24kc.ipk")
            p.rename(renamed)
            with self.assertRaisesRegex(ValueError, "filename"):
                auto.ipk(renamed)

    def test_candidate_inventory_and_tamper(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            candidate(root)
            auto.validate_candidate(root, TAG, COMMIT)
            (root / "unexpected.txt").write_text("extra")
            with self.assertRaisesRegex(ValueError, "unexpected"):
                auto.validate_candidate(root)
            (root / "unexpected.txt").unlink()
            (root / "Packages.gz").write_bytes(b"bad")
            with self.assertRaisesRegex(ValueError, "hash"):
                auto.validate_candidate(root)

    def test_config_override_removes_old_assignment(self):
        result = builder.rewrite_config("CONFIG_A=y\n# CONFIG_B is not set\nCONFIG_C=y\n",
                                        {"CONFIG_A": "n", "CONFIG_B": "m"})
        self.assertEqual(result, "CONFIG_C=y\n# CONFIG_A is not set\nCONFIG_B=m\n")

    def test_cached_downloads_require_pinned_hashes_and_bounded_repair(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            (root / "dl").mkdir()
            cached = root / "dl/source.tar.gz"
            cached.write_bytes(b"good source")
            manifest = root / "download-manifest.txt"
            manifest.write_text("source.tar.gz " + auto.sha256(cached) + "\n")
            self.assertEqual(builder.verify_downloads(root), [])
            cached.write_bytes(b"corrupt")
            with self.assertRaisesRegex(ValueError, "checksum"):
                builder.verify_downloads(root)
            self.assertEqual(builder.verify_downloads(root, repair=True), [cached.name])
            self.assertFalse(cached.exists())
            manifest.write_text("../outside " + "0" * 64 + "\n")
            with self.assertRaises(ValueError):
                builder.verify_downloads(root, repair=True)
            manifest.write_text("source.tar.gz x\n")
            with self.assertRaisesRegex(ValueError, "pinned"):
                builder.download_inventory(root)


class FakeGitHub:
    def __init__(self):
        self.enabled = True
        self.release = None
        self.ref = None
        self.files = {}
        self.mutations = []
        self.interrupt_after = None
        self.corrupt_public = False

    def api(self, endpoint, method="GET", payload=None, missing=False):
        if method != "GET":
            self.mutations.append((endpoint, method))
        if endpoint == "immutable-releases":
            return {"enabled": self.enabled}
        if endpoint.startswith("git/ref/tags/"):
            return self.ref
        if endpoint == "releases" and method == "POST":
            self.release = dict(payload, id=1, immutable=False)
            return self.release
        if endpoint == "releases/1" and method == "PATCH":
            self.release.update(draft=False, immutable=True)
            self.ref = {"object": {"type": "commit", "sha": COMMIT}}
        if endpoint == "releases/1":
            return self.release
        raise AssertionError((endpoint, method))

    def find_release(self, tag):
        return self.release

    def assets(self, release_id):
        return [{"id": i, "name": name} for i, name in enumerate(sorted(self.files))]

    def upload(self, tag, path):
        if self.interrupt_after is not None and len(self.files) == self.interrupt_after:
            raise OSError("simulated interrupted upload")
        if path.name in self.files:
            raise AssertionError("overwrite")
        self.files[path.name] = path.read_bytes()

    def asset_hash(self, asset):
        return auto.digest(self.files[asset["name"]])

    def anonymous_hash(self, url):
        return "corrupt" if self.corrupt_public else auto.digest(self.files[url.rsplit("/", 1)[1]])


@unittest.skipUnless(shutil.which("mksquashfs") and shutil.which("unsquashfs"), "requires SquashFS tools")
class ImageInspectionTests(unittest.TestCase):
    def test_embedded_images_and_metadata_are_inspected(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            output = root / "candidate"
            output.mkdir()
            manifest = candidate(output)
            key_id = manifest["key_id"]
            elf = bytearray(20)
            elf[:6] = b"\x7fELF\x01\x01"
            elf[18:20] = b"\x08\x00"
            files = {
                "usr/share/hh71vm-feed/release.conf": f"release={TAG}\nkernel={KERNEL}\nkey_id={key_id}\n".encode(),
                "usr/share/hh71vm-feed/release.pub": KEY,
                "etc/opkg/keys/" + key_id: KEY,
                "etc/opkg.conf": b"option check_signature\n",
                "etc/opkg/hh71vm.conf": ("src/gz hh71vm " + auto.feed_url(TAG) + "\n").encode(),
                "usr/lib/opkg/status": "".join(f"{k}: {v}\n" for k, v in package("kernel", version=KERNEL).items()).encode(),
                "usr/sbin/xtables-legacy-multi": bytes(elf),
            }
            for name, source in (("usr/libexec/hh71vm-feed-reconcile", "reconcile.sh"),
                                 ("etc/uci-defaults/99-hh71vm-feed", "99-hh71vm-feed")):
                files[name] = (ROOT / "autobuild/package/hh71vm-feed/files" / source).read_bytes().replace(b"\r\n", b"\n")
            tree = root / "filesystem"
            for name, data in files.items():
                path = tree / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            squash = root / "rootfs.bin"
            subprocess.run(["mksquashfs", str(tree), str(squash), "-noappend", "-no-progress", "-all-root", "-processors", "1"],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            body = squash.read_bytes()
            body = body[:8] + struct.pack(">I", len(body) - 640) + body[12:]
            def checked(data):
                return data + struct.pack(">H", (-sum(x[0] for x in struct.iter_unpack(">H", data))) & 0xffff)
            body = checked(body)
            kernel_body = checked(b"simulated kernel")
            kernel = b"cr6c" + struct.pack(">III", 0x80000000, 0x30000, len(kernel_body)) + kernel_body
            (output / "test-fwupg.bin").write_bytes(kernel + b"r6cr" + struct.pack(">III", 0, 0x300000, len(body)) + body)
            (output / "test-sysupgrade.bin").write_bytes(kernel.ljust(2949120, b"\xff") + body)
            cpio = bytearray()
            for name, data in list(files.items()) + [("TRAILER!!!", b"")]:
                values = [0, 0o100644, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(name) + 1, 0]
                cpio += ("070701" + "".join(f"{v:08x}" for v in values)).encode() + name.encode() + b"\0"
                cpio += b"\0" * (-len(cpio) % 4)
                cpio += data
                cpio += b"\0" * (-len(cpio) % 4)
            (output / "test-nfjrom.bin").write_bytes(b"simulated loader".ljust(64, b"\0") + lzma.compress(bytes(cpio), format=lzma.FORMAT_ALONE))
            result = images.inspect_release_images(None, output, TAG, KEY, KERNEL)
            self.assertEqual(len(result), 3)
            with self.assertRaisesRegex(ValueError, "manifest ABI"):
                images.inspect_release_images(None, output, TAG, KEY, "other")
            auto.write_json(output / "image-packages.json", [package("kernel", version="other")])
            with self.assertRaisesRegex(ValueError, "package inventory"):
                images.inspect_release_images(None, output, TAG, KEY)


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.manifest = candidate(self.root)
        self.github = FakeGitHub()

    def publish(self):
        publisher.publish(self.root, self.manifest, self.github)

    def test_publish_and_repeat_are_idempotent(self):
        self.publish()
        mutations = list(self.github.mutations)
        self.publish()
        self.assertEqual(mutations, self.github.mutations)
        self.assertTrue(self.github.release["immutable"])
        self.assertNotIn("VERIFIED", self.github.release["body"])

    def test_interrupted_upload_resumes_exact_draft(self):
        self.github.interrupt_after = 2
        with self.assertRaises(OSError):
            self.publish()
        self.assertTrue(self.github.release["draft"])
        self.github.interrupt_after = None
        self.publish()
        self.assertFalse(self.github.release["draft"])
        self.assertEqual(sum(m == "POST" for _, m in self.github.mutations), 1)

    def test_disabled_immutability_makes_no_mutations(self):
        self.github.enabled = False
        with self.assertRaises(ValueError):
            self.publish()
        self.assertEqual(self.github.mutations, [])

    def test_existing_tag_cannot_move(self):
        self.github.ref = {"object": {"type": "commit", "sha": "b" * 40}}
        with self.assertRaisesRegex(ValueError, "another source"):
            self.publish()
        self.assertEqual(self.github.mutations, [])

    def test_asset_overwrite_and_delete_are_forbidden(self):
        self.publish()
        for name in ("Packages", "extra.txt"):
            with self.subTest(name=name):
                saved = dict(self.github.files)
                self.github.files[name] = b"changed"
                with self.assertRaises(ValueError):
                    self.publish()
                self.assertEqual(self.github.files[name], b"changed")
                self.github.files = saved

    def test_anonymous_readback_failure_is_not_success(self):
        self.github.corrupt_public = True
        with self.assertRaisesRegex(ValueError, "anonymous"):
            self.publish()
        self.assertTrue(self.github.release["immutable"])

    def test_cleanup_only_retires_the_exact_transfer_artifact(self):
        with patch.object(self.github, "api") as api:
            api.return_value = {"name": TAG, "workflow_run": {"id": 42}}
            publisher.retire_transfer_artifact(self.github, 123, self.manifest)
            api.assert_called_with("actions/artifacts/123", "DELETE")
        with patch.object(self.github, "api") as api:
            api.return_value = {"name": TAG, "workflow_run": {"id": 99}}
            with self.assertRaisesRegex(ValueError, "unrelated"):
                publisher.retire_transfer_artifact(self.github, 123, self.manifest)
            self.assertEqual(api.call_count, 1)


@unittest.skipUnless(os.name == "posix" and shutil.which("usign"), "requires POSIX shell and pinned usign")
class MigrationAndSignatureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.key = self.root / "test.key"
        self.pub = self.root / "test.pub"
        subprocess.run(["usign", "-G", "-s", str(self.key), "-p", str(self.pub), "-c", "Disposable test key"], check=True)
        self.key_id, self.normalized = auto.public_key(self.pub.read_bytes())
        self.put("rom/usr/share/hh71vm-feed/release.pub", self.normalized)
        self.descriptor()
        self.put("etc/opkg.conf", "dest root /\nlists_dir ext /var/opkg-lists\noption check_signature\n")
        self.put("etc/opkg/customfeeds.conf", "# user feeds\nsrc/gz thirdparty https://example.org/feed\n")
        self.put("tmp/sysinfo/board_name", "hh71vm\n")
        self.put("usr/lib/opkg/status", f"Package: kernel\nVersion: {KERNEL}\n\n")
        self.script = self.root / "migrate.sh"
        self.script.write_bytes((ROOT / "autobuild/package/hh71vm-feed/files/reconcile.sh").read_bytes().replace(b"\r\n", b"\n"))

    def put(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data.encode() if isinstance(data, str) else data)

    def descriptor(self, tag=TAG, kernel=KERNEL):
        self.put("rom/usr/share/hh71vm-feed/release.conf",
                 f"release={tag}\nkernel={kernel}\narchitecture=mipsel_24kc\nkey_id={self.key_id}\nfeed_url={auto.feed_url(tag)}\n")

    def migrate(self, success=True):
        result = subprocess.run(["sh", str(self.script)], env=dict(os.environ, HH71VM_ROOT=str(self.root)),
                                capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        self.assertEqual((self.root / "etc/opkg/hh71vm.conf").exists(), success)
        return result

    def test_clean_install_no_manual_configuration_and_repeat(self):
        self.migrate()
        first = (self.root / "etc/opkg/hh71vm.conf").read_bytes()
        self.migrate()
        self.assertEqual(first, (self.root / "etc/opkg/hh71vm.conf").read_bytes())
        self.assertIn(auto.feed_url(TAG).encode(), first)
        self.assertEqual((self.root / ("etc/opkg/keys/" + self.key_id)).read_bytes(), self.normalized)

    def test_upgrade_changes_url_even_if_kernel_abi_is_unchanged(self):
        old = auto.feed_url(auto.identity(COMMIT, 41, 1))
        foreign = (self.root / "etc/opkg/customfeeds.conf").read_text()
        self.put("etc/opkg/customfeeds.conf", foreign + "src/gz previous " + old + "\n")
        self.put("etc/opkg/hh71vm.conf", "src/gz hh71vm " + old + "\n")
        self.put("etc/opkg/distfeeds.conf", "src/gz raw https://raw.githubusercontent.com/sowarden/hh71vm-openwrt/main/packages\n")
        for name in ("hh71vm", "previous", "raw"):
            self.put("var/opkg-lists/" + name, "stale index")
        self.put("var/opkg-lists/thirdparty", "preserved")
        self.migrate()
        self.assertEqual((self.root / "etc/opkg/customfeeds.conf").read_text(), foreign)
        self.assertEqual((self.root / "var/opkg-lists/thirdparty").read_text(), "preserved")
        self.assertFalse((self.root / "var/opkg-lists/previous").exists())
        self.assertEqual((self.root / "etc/opkg/distfeeds.conf").read_text(), "")
        self.assertTrue(list((self.root / "etc/hh71vm-feed/backups").iterdir()))

    def test_rom_descriptor_overrides_restored_overlay(self):
        self.put("usr/share/hh71vm-feed/release.conf", "old invalid overlay\n")
        self.migrate()

    def test_initramfs_without_rom_uses_its_own_descriptor(self):
        shutil.move(str(self.root / "rom/usr/share/hh71vm-feed"), str(self.root / "usr/share-hh71vm-feed"))
        (self.root / "usr/share").mkdir()
        shutil.move(str(self.root / "usr/share-hh71vm-feed"), str(self.root / "usr/share/hh71vm-feed"))
        shutil.rmtree(self.root / "rom")
        self.migrate()

    def test_abi_mismatch_disables_managed_feed(self):
        self.descriptor(kernel="4.14.275-1-other")
        self.migrate(success=False)

    def test_wrong_board_and_missing_rom_descriptor_fail_closed(self):
        self.put("tmp/sysinfo/board_name", "other\n")
        self.migrate(success=False)
        self.put("tmp/sysinfo/board_name", "hh71vm\n")
        (self.root / "rom/usr/share/hh71vm-feed/release.conf").unlink()
        self.migrate(success=False)

    def test_signature_override_and_reserved_feed_name_rejected(self):
        for override in ("option check_signature 0", "option force_signature 1", "option force_depends 1",
                         "src/gz hh71vm https://example.org/other"):
            with self.subTest(override=override):
                self.put("etc/opkg/customfeeds.conf", override + "\n")
                self.migrate(success=False)
                self.assertIn(override, (self.root / "etc/opkg/customfeeds.conf").read_text())

    def test_relocated_old_cache_is_retired_but_override_requires_review(self):
        self.put("etc/opkg/customfeeds.conf", f"lists_dir ext /tmp/old-lists\nsrc/gz previous {auto.feed_url(TAG)}\n")
        self.put("tmp/old-lists/previous", "stale")
        self.migrate(success=False)
        self.assertFalse((self.root / "tmp/old-lists/previous").exists())

    def test_symlink_config_is_replaced_without_writing_its_target(self):
        data = f"src/gz old {auto.feed_url(TAG)}\nsrc/gz thirdparty https://example.org/feed\n"
        self.put("user.conf", data)
        (self.root / "etc/opkg/customfeeds.conf").unlink()
        (self.root / "etc/opkg/customfeeds.conf").symlink_to(self.root / "user.conf")
        self.migrate()
        self.assertEqual((self.root / "user.conf").read_text(), data)
        self.assertFalse((self.root / "etc/opkg/customfeeds.conf").is_symlink())

    def test_key_rotation_preserves_third_party_key(self):
        self.put("etc/opkg/keys/oldproject", KEY)
        self.put("etc/opkg/keys/thirdparty", b"untrusted comment: third party\nexample\n")
        self.migrate()
        self.assertFalse((self.root / "etc/opkg/keys/oldproject").exists())
        self.assertTrue((self.root / "etc/opkg/keys/thirdparty").exists())

    def test_real_usign_public_fingerprint_and_tampering(self):
        actual = subprocess.check_output(["usign", "-F", "-p", str(self.pub)], text=True).strip()
        self.assertEqual(actual, self.key_id)
        message, signature = self.root / "Packages", self.root / "Packages.sig"
        for length in (46, 47, 48, 174, 175):
            data = b"x" * length
            if (64 + len(data)) % 128 in (110, 111):
                data += b"\n\n"
            message.write_bytes(data)
            subprocess.run(["usign", "-S", "-s", str(self.key), "-m", str(message), "-x", str(signature)], check=True)
            args = ["usign", "-V", "-p", str(self.pub), "-m", str(message), "-x", str(signature)]
            self.assertEqual(subprocess.run(args, capture_output=True).returncode, 0)
            message.write_bytes(data + b"changed")
            self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)

    def test_signing_handoff_uses_secret_only_for_signatures(self):
        output = self.root / "candidate"
        output.mkdir()
        manifest = candidate(output)
        (output / "hh71vm-feed.pub").write_bytes(self.normalized)
        manifest["key_id"] = self.key_id
        manifest["files"]["hh71vm-feed.pub"] = auto.sha256(output / "hh71vm-feed.pub")
        auto.write_json(output / "release.json", manifest)
        # Image inspection is exercised independently; this checks real usign
        # handoff, environment isolation and complete signed inventory.
        with patch.dict(os.environ, HH71VM_FEED_SIGNING_KEY=self.key.read_text(), GITHUB_RUN_ID="42"):
            with patch.object(publisher, "inspect_release_images") as inspect:
                inspect.side_effect = lambda *args: self.assertNotIn("HH71VM_FEED_SIGNING_KEY", os.environ)
                publisher.sign(output, TAG, COMMIT, self.normalized)
            self.assertNotIn("HH71VM_FEED_SIGNING_KEY", os.environ)
        auto.validate_candidate(output, TAG, COMMIT, signed=True)
        publisher.verify_signatures(output)
        for path in output.iterdir():
            self.assertNotIn(self.key.read_bytes(), path.read_bytes())

    def test_signing_rejects_a_candidate_from_another_run(self):
        output = self.root / "candidate"
        output.mkdir()
        candidate(output)
        with patch.dict(os.environ, HH71VM_FEED_SIGNING_KEY=self.key.read_text(), GITHUB_RUN_ID="99"):
            with self.assertRaisesRegex(ValueError, "different workflow"):
                publisher.sign(output, TAG, COMMIT, KEY)
            self.assertNotIn("HH71VM_FEED_SIGNING_KEY", os.environ)
        self.assertFalse((output / "Packages.sig").exists())


if __name__ == "__main__":
    unittest.main()
