import copy
import importlib.util
import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/release"))
import extras_common as common
from package_metadata import sha256


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools/release" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


build = load_script("build-extras")
publish = load_script("publish-extras")


class ExtrasReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.output = self.base / "candidate"
        self.output.mkdir()
        (self.output / "tools.zip").write_bytes(b"test bundle bytes")
        self.candidate = {
            "schema": 1, "hardware_tested": False, "source_commit": "a" * 40,
            "run_id": 123, "run_attempt": 1, "run_number": 7,
            "firmware": common.read_json(ROOT / common.LOCK),
            "assets": {"tools.zip": sha256(self.output / "tools.zip")},
            "bundles": {"tools": {"version": "1.0-1", "archive": "tools.zip"}},
        }
        self.run = {"id": 123, "head_sha": "a" * 40, "run_attempt": 1,
                    "run_number": 7, "conclusion": "success", "build_succeeded": True}
        self.save_candidate()

    def save_candidate(self):
        common.write_json(self.output / "candidate.json", self.candidate)
        sums = dict(self.candidate["assets"], **{"candidate.json": sha256(self.output / "candidate.json")})
        (self.output / "SHA256SUMS").write_bytes(
            "".join(f"{digest}  {name}\n" for name, digest in sorted(sums.items())).encode())

    def test_repository_compatibility_and_default_policy(self):
        self.assertTrue(common.compatibility(ROOT)["firmware_build"])
        self.assertIs(type(common.release_policy(ROOT)), bool)
        self.assertGreaterEqual(len(build.recipes(ROOT)), 1)

    def test_policy_requires_real_boolean(self):
        for value in ("false", "true", 0, 1, None):
            with self.subTest(value=value), patch.object(common, "read_json", return_value={"auto_publish_unverified": value}):
                with self.assertRaises(ValueError):
                    common.release_policy(ROOT)
        with patch.object(common, "read_json", return_value={"auto_publish_unverified": True}):
            self.assertTrue(common.release_policy(ROOT))

    def test_kernel_source_and_firmware_image_drift_fail_closed(self):
        with patch.object(common, "kernel_inputs", return_value="0" * 64):
            with self.assertRaisesRegex(ValueError, "kernel/config"):
                common.compatibility(ROOT)
        with patch.object(common, "sha256", return_value="0" * 64):
            with self.assertRaisesRegex(ValueError, "firmware image"):
                common.compatibility(ROOT)

    def test_kernel_input_digest_normalizes_text_but_not_binary(self):
        root = self.base / "sources"
        target = root / "openwrt-feed/target/linux/test"
        target.mkdir(parents=True)
        config = root / "openwrt-feed/build.config"
        config.write_bytes(b"CONFIG_TEST=y\r\n")
        (target / "source.c").write_bytes(b"source\n")
        binary = target / "data.bin"
        binary.write_bytes(b"\0\r\n")
        first = common.kernel_inputs(root)
        config.write_bytes(b"CONFIG_TEST=y\n")
        self.assertEqual(common.kernel_inputs(root), first)
        binary.write_bytes(b"\0\n")
        self.assertNotEqual(common.kernel_inputs(root), first)

    def test_source_only_recipe_needs_no_committed_ipks(self):
        root = self.base / "source-only"
        bundle = root / "extras/sample-tools"
        bundle.mkdir(parents=True)
        (bundle / "README.md").write_text("Sample")
        source = root / "openwrt-feed/package/utils/sample-tools"
        source.mkdir(parents=True)
        (source / "Makefile").write_text("# test recipe")
        common.write_json(bundle / "bundle.json", {
            "schema": 2, "name": "sample-tools", "files": ["README.md"],
            "build": {"version_package": "sample-tools",
                      "packages": {"sample-tools": "package/utils/sample-tools"}},
        })
        entries = build.recipes(root)
        self.assertEqual(entries[0][1]["name"], "sample-tools")
        self.assertNotIn("sha256", entries[0][1])

    def test_source_overlay_normalizes_text_and_preserves_binaries(self):
        source = self.base / "overlay"
        source.mkdir()
        (source / "Kconfig").write_bytes(b"config EXAMPLE\r\n\tbool\r\n")
        (source / "helper").write_bytes(b"\x7fELF\0\r\n")
        destination = self.base / "destination"
        build.copy_overlay(source, destination)
        self.assertEqual((destination / "Kconfig").read_bytes(), b"config EXAMPLE\n\tbool\n")
        self.assertEqual((destination / "helper").read_bytes(), (source / "helper").read_bytes())

    def test_candidate_checks_exact_manifest_and_asset_bytes(self):
        digest = sha256(self.output / "candidate.json")
        self.assertEqual(common.validate_candidate(self.output, digest), self.candidate)
        self.assertEqual(publish.sha256_current(self.candidate), digest)
        with self.assertRaisesRegex(ValueError, "manifest SHA-256"):
            common.validate_candidate(self.output, "0" * 64)
        (self.output / "tools.zip").write_bytes(b"different")
        with self.assertRaisesRegex(ValueError, "asset checksum"):
            common.validate_candidate(self.output, digest)

    def test_candidate_rejects_extra_files(self):
        (self.output / "unexpected.ipk").write_bytes(b"extra")
        with self.assertRaisesRegex(ValueError, "unexpected"):
            common.validate_candidate(self.output)

    def test_candidate_cannot_assert_its_own_hardware_test(self):
        self.candidate["hardware_tested"] = True
        self.save_candidate()
        with self.assertRaisesRegex(ValueError, "self-asserted"):
            common.validate_candidate(self.output)

    def test_candidate_rejects_paths_and_bad_inventory(self):
        self.candidate["assets"] = {"../escape.zip": "0" * 64}
        common.write_json(self.output / "candidate.json", self.candidate)
        with self.assertRaisesRegex(ValueError, "asset name"):
            common.validate_candidate(self.output)
        self.candidate["assets"] = {"tools.zip": sha256(self.output / "tools.zip")}
        self.save_candidate()
        (self.output / "SHA256SUMS").write_text("bad inventory")
        with self.assertRaisesRegex(ValueError, "inventory"):
            common.validate_candidate(self.output)

    def test_manual_promotion_requires_test_and_report(self):
        for tested, notes in ((False, "passed"), (True, "  ")):
            with self.assertRaisesRegex(ValueError, "confirmation"):
                publish.authorize(self.candidate, self.run, "tested", tested, notes, False)
        publish.authorize(self.candidate, self.run, "tested", True, "CLI, LuCI and reboot passed", False)

    def test_run_identity_and_success_are_checked(self):
        for field, value in (("id", 124), ("head_sha", "b" * 40), ("run_attempt", 2), ("run_number", 8)):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "provenance"):
                publish.authorize(self.candidate, dict(self.run, **{field: value}), "tested", True, "passed", False)
        with self.assertRaisesRegex(ValueError, "successful"):
            publish.authorize(self.candidate, dict(self.run, build_succeeded=False), "tested", True, "passed", False)
        publish.authorize(self.candidate, dict(self.run, conclusion="failure"), "tested", True, "passed", False)

    def test_only_main_branch_build_workflow_candidates_are_accepted(self):
        run = dict(self.run, workflow_id=88, path=common.WORKFLOW,
                   head_repository={"full_name": "owner/repo"}, head_branch="main", event="push")
        mutations = ({"head_branch": "topic"}, {"event": "pull_request"}, {"workflow_id": 99},
                     {"head_repository": {"full_name": "fork/repo"}}, {"path": "other.yml"})
        for change in mutations:
            with self.subTest(change=change), patch.object(publish, "api", side_effect=[dict(run, **change), {"id": 88}]):
                with self.assertRaisesRegex(ValueError, "this repository"):
                    publish.check_run("owner/repo", 123, 1)
        with patch.object(publish, "api", side_effect=[run, {"id": 88}]), patch.object(
                publish, "gh", return_value=[{"jobs": [{"name": "build", "conclusion": "success"}]}]):
            self.assertTrue(publish.check_run("owner/repo", 123, 1)["build_succeeded"])

    def test_missing_build_job_is_not_success(self):
        run = dict(self.run, workflow_id=88, path=common.WORKFLOW,
                   head_repository={"full_name": "owner/repo"}, head_branch="main", event="push")
        with patch.object(publish, "api", side_effect=[run, {"id": 88}]), patch.object(
                publish, "gh", return_value=[{"jobs": [{"name": "other", "conclusion": "success"}]}]):
            self.assertFalse(publish.check_run("owner/repo", 123, 1)["build_succeeded"])

    def test_bundle_inventory_cannot_point_to_another_asset(self):
        self.candidate["bundles"]["tools"]["archive"] = "another.zip"
        self.save_candidate()
        with self.assertRaisesRegex(ValueError, "bundle inventory"):
            common.validate_candidate(self.output)

    def test_automatic_promotion_requires_opt_in_and_same_run(self):
        with self.assertRaisesRegex(ValueError, "disabled"):
            publish.authorize(self.candidate, self.run, "unverified", False, "", False)
        with patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(ValueError, "own build"):
                publish.authorize(self.candidate, self.run, "unverified", False, "", True)
        with patch.dict(os.environ, GITHUB_RUN_ID="123", GITHUB_RUN_ATTEMPT="1", GITHUB_SHA="a" * 40):
            publish.authorize(self.candidate, self.run, "unverified", False, "", True)

    def test_release_has_warning_and_firmware_identity(self):
        body = publish.release_body("owner/repo", self.candidate, False, "owner", "")
        self.assertIn(publish.WARNING, body)
        self.assertIn("NOT firmware", body)
        self.assertIn(self.candidate["firmware"]["kernel"], body)
        self.assertIn("clone the entire repository", body)
        tested = publish.release_body("owner/repo", self.candidate, True, "owner", "CLI and reboot passed")
        self.assertNotIn(publish.WARNING, tested)
        self.assertIn("Hardware test confirmed by owner", tested)

    def test_existing_release_protects_unmanaged_immutable_and_newer_builds(self):
        for release in ({"body": "unrelated"}, {"body": publish.MARKER, "immutable": True}):
            with self.assertRaises(ValueError):
                publish.check_existing(release, self.candidate, True)
        newer = dict(self.candidate, run_number=8)
        release = {"body": publish.release_body("o/r", newer, False, "o", "")}
        with self.assertRaisesRegex(ValueError, "newer release"):
            publish.check_existing(release, self.candidate, True)

    def test_confirmed_release_cannot_be_downgraded_by_retry(self):
        release = {"body": publish.release_body("o/r", self.candidate, True, "o", "passed")}
        with self.assertRaisesRegex(ValueError, "downgrade"):
            publish.check_existing(release, self.candidate, False)
        publish.check_existing(release, self.candidate, True)

    def test_release_updates_only_after_upload_and_readback(self):
        calls, uploaded = [], {}
        release = {"id": 99, "body": publish.MARKER, "assets": [{"name": "old.zip", "id": 55}]}

        def fake_api(repo, endpoint, method="GET", payload=None, missing=False):
            calls.append((method, endpoint))
            if endpoint.startswith("git/ref/"):
                return {"object": {"type": "commit"}}
            return copy.deepcopy(release)

        def fake_gh(*args, **kwargs):
            calls.append((args[1], args[2]))
            if args[:2] == ("release", "upload"):
                uploaded[Path(args[3]).name] = Path(args[3]).read_bytes()
            elif args[:2] == ("release", "download"):
                name = args[args.index("--pattern") + 1]
                dest = Path(args[args.index("--dir") + 1])
                (dest / name).write_bytes(uploaded[name])

        with patch.object(publish, "api", side_effect=fake_api), patch.object(publish, "gh", side_effect=fake_gh):
            publish.publish("o/r", self.output, self.candidate, True, "passed")
        patched = calls.index(("PATCH", "releases/99"))
        self.assertTrue(all(i < patched for i, call in enumerate(calls) if call[0] in ("upload", "download")))
        self.assertGreater(calls.index(("DELETE", "releases/assets/55")), patched)
        self.assertEqual(uploaded["tools.zip"], (self.output / "tools.zip").read_bytes())

    def test_failed_readback_preserves_previous_release(self):
        calls = []
        release = {"id": 99, "body": publish.MARKER, "assets": []}

        def fake_api(repo, endpoint, method="GET", **kwargs):
            calls.append(method)
            return release

        def fake_gh(*args, **kwargs):
            if args[:2] == ("release", "download"):
                (Path(args[args.index("--dir") + 1]) / args[args.index("--pattern") + 1]).write_bytes(b"corrupt")

        with patch.object(publish, "api", side_effect=fake_api), patch.object(publish, "gh", side_effect=fake_gh):
            with self.assertRaisesRegex(ValueError, "uploaded asset differs"):
                publish.publish("o/r", self.output, self.candidate, True, "passed")
        self.assertNotIn("PATCH", calls)
        self.assertNotIn("DELETE", calls)

    def test_collect_uses_built_versions_hashes_and_embedded_guard(self):
        tree = self.base / "build"
        packages = tree / "bin/packages"
        packages.mkdir(parents=True)
        kernel = tree / "build_dir/target-test/linux-rtkmipsel_test/linux-4.14.275/.vermagic"
        kernel.parent.mkdir(parents=True)
        kernel.write_text("2709aa412f796f4f2600f70163b49915")
        entries = [entry for entry in build.recipes(ROOT) if entry[1]["name"] == "modem-extra-tools"]
        for name in entries[0][1]["packages"]:
            shutil.copyfile(ROOT / "packages" / name, packages / name)
        entries[0][1]["sha256"] = {name: "0" * 64 for name in entries[0][1]["packages"]}
        output = self.base / "collected"
        with patch.dict(os.environ, {}, clear=True):
            build.collect(ROOT, tree, output, self.candidate["firmware"], entries, "a" * 40, 123, 1, 7)
        candidate = common.validate_candidate(output)
        self.assertEqual(candidate["bundles"]["modem-extra-tools"]["version"], "1.1.0-1")
        self.assertEqual(len(candidate["assets"]), 1)
        import zipfile
        with zipfile.ZipFile(output / next(iter(candidate["assets"]))) as archive:
            settings = archive.read("modem-extra-tools-1.1.0-1/bundle.env").decode()
            self.assertIn(self.candidate["firmware"]["kernel"], settings)
            self.assertIn("ipk_modem_extra_tools='modem-extra-tools_1.1.0-1_mipsel_24kc.ipk'", settings)
        kernel.write_text("bad")
        with self.assertRaisesRegex(ValueError, "rebuilt kernel ABI"):
            build.collect(ROOT, tree, self.base / "bad", self.candidate["firmware"], entries, "a" * 40, 123, 1, 7)


if __name__ == "__main__":
    unittest.main()
