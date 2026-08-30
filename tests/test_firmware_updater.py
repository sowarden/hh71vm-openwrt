"""Mocked host tests for the router firmware updater; no network or flash access."""
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UPDATER = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files/usr/sbin/autosysupgrade"
ASSET = "openwrt-rtkmipsel-rtl8197f-hh71vm-sysupgrade.bin"
CURRENT = "hh71vm-aaaaaaaaaaaa-r100-a1"
LATEST = "hh71vm-r00000000000000000101-a000001-bbbbbbbbbbbb"


@unittest.skipUnless(os.name == "posix" and shutil.which("sh"), "requires a POSIX shell")
class FirmwareUpdaterTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.bin = self.root / "bin"
        self.fixtures = self.root / "fixtures"
        self.bin.mkdir()
        self.fixtures.mkdir()
        self.fetch_log = self.root / "fetch.log"
        (self.root / "tmp/sysinfo").mkdir(parents=True)
        (self.root / "rom/usr/share/hh71vm-feed").mkdir(parents=True)
        (self.root / "tmp/sysinfo/board_name").write_text("hh71vm\n")
        (self.root / "rom/usr/share/hh71vm-feed/release.pub").write_text("synthetic public key\n")
        (self.root / "rom/usr/share/hh71vm-feed/release.conf").write_text(f"release={CURRENT}\n")
        self.image = b"synthetic firmware image\n"
        (self.fixtures / "image.bin").write_bytes(self.image)
        self.write_manifest("latest.json", LATEST, ["Add the LuCI updater."])
        self.write_manifest("current.json", CURRENT, ["Previous synthetic change."])
        (self.fixtures / "latest.sig").write_text("good\n")
        (self.fixtures / "current.sig").write_text("good\n")
        (self.fixtures / "releases.json").write_text(json.dumps([
            {"tag_name": CURRENT, "published_at": "2026-08-29T12:00:00Z", "prerelease": False},
            {"tag_name": LATEST, "published_at": "2026-08-30T12:00:00Z", "prerelease": False},
        ]))
        self.install_mocks()

    def tearDown(self):
        self.temporary.cleanup()

    def write_manifest(self, name, tag, changes):
        value = {
            "schema": 1,
            "tag": tag,
            "architecture": "mipsel_24kc",
            "key_id": "0011223344556677",
            "feed_url": f"https://github.com/sowarden/hh71vm-openwrt/releases/download/{tag}",
            "changelog": changes,
            "files": {ASSET: hashlib.sha256(self.image).hexdigest()},
        }
        (self.fixtures / name).write_text(json.dumps(value, indent=2) + "\n")

    def executable(self, name, text):
        path = self.bin / name
        path.write_text(text)
        path.chmod(0o755)

    def install_mocks(self):
        self.executable("id", "#!/bin/sh\n[ \"$1\" = -u ] && echo 0\n")
        self.executable("usign", """#!/bin/sh
if [ "$1" = -F ]; then echo 0011223344556677; exit 0; fi
while [ "$#" -gt 0 ]; do [ "$1" = -x ] && { shift; signature=$1; }; shift; done
[ "$(cat "$signature")" = good ]
""")
        self.executable("jsonfilter", """#!/usr/bin/env python3
import json, re, sys
args=sys.argv[1:]; path=args[args.index('-i')+1]; expr=args[args.index('-e')+1]
value=json.load(open(path, encoding='utf-8'))
match=re.fullmatch(r'@\[(\d+)\]\.([A-Za-z0-9_]+)', expr)
if match:
    index, key=int(match.group(1)), match.group(2)
    if index >= len(value): sys.exit(1)
    item=value[index].get(key, '')
    print(str(item).lower() if isinstance(item, bool) else item); sys.exit()
match=re.fullmatch(r'@\.([A-Za-z0-9_]+)\[\*\]', expr)
if match:
    for item in value.get(match.group(1), []): print(item)
    sys.exit()
match=re.fullmatch(r'@\.([A-Za-z0-9_]+)', expr)
if not match or match.group(1) not in value: sys.exit(1)
item=value[match.group(1)]
print(str(item).lower() if isinstance(item, bool) else item)
""")
        self.executable("uclient-fetch", """#!/usr/bin/env python3
import json, os, shutil, sys, time
args=sys.argv[1:]; output=args[args.index('-O')+1]; url=args[-1]
with open(os.environ['UPDATER_FETCH_LOG'], 'a', encoding='utf-8') as stream:
    stream.write(url + '\\n')
for suffix in json.loads(os.environ.get('UPDATER_MOCK_FAILURES', '[]')):
    if url.endswith(suffix): sys.exit(1)
timeout=int(args[args.index('-T')+1])
for suffix, delay in json.loads(os.environ.get('UPDATER_MOCK_DELAYS', '{}')).items():
    if url.endswith(suffix):
        time.sleep(min(float(delay), timeout))
        if float(delay) >= timeout: sys.exit(1)
mapping=json.loads(os.environ['UPDATER_MOCK_MAP'])
for needle, source in mapping.items():
    if needle in url: shutil.copyfile(source, output); sys.exit()
sys.exit(1)
""")
        self.executable("sysupgrade", """#!/bin/sh
printf '%s\n' "$*" >> "$UPDATER_SYSUPGRADE_LOG"
exit 0
""")
        jshn = self.root / "jshn.sh"
        jshn.write_text("""if [ -n "${temporary:-}" ]; then jshn_log="$temporary/jshn.log"; else jshn_log="$UPDATER_JSHN_LOG"; fi
json_init() { jshn_probe="${JSON_PREFIX}${JSON_UNSET}"; unset JSON_UNSET; : > "$jshn_log"; }
json_add_string() { jshn_probe="${JSON_PREFIX}${JSON_UNSET}"; printf 'S\\t%s\\t%s\\n' "$1" "$2" >> "$jshn_log"; }
json_add_boolean() { jshn_probe="${JSON_PREFIX}${JSON_UNSET}"; printf 'B\\t%s\\t%s\\n' "$1" "$2" >> "$jshn_log"; }
json_add_array() { jshn_probe="${JSON_PREFIX}${JSON_UNSET}"; printf 'A\\t%s\\t\\n' "$1" >> "$jshn_log"; }
json_add_object() { jshn_probe="${JSON_PREFIX}${JSON_UNSET}"; printf 'O\\t%s\\t\\n' "$1" >> "$jshn_log"; }
json_close_array() { printf 'X\\t\\t\\n' >> "$jshn_log"; }
json_close_object() { printf 'X\\t\\t\\n' >> "$jshn_log"; }
json_dump() { python3 "$UPDATER_JSHN_RENDER" "$jshn_log"; }
""")
        renderer = self.root / "render_jshn.py"
        renderer.write_text("""import json, sys
root={}; stack=[root]
def attach(key, value):
    if isinstance(stack[-1], list): stack[-1].append(value)
    else: stack[-1][key]=value
for line in open(sys.argv[1], encoding='utf-8'):
    kind, key, value=line.rstrip('\\n').split('\\t', 2)
    if kind in 'AO':
        child=[] if kind == 'A' else {}; attach(key, child); stack.append(child)
    elif kind == 'X': stack.pop()
    elif kind == 'B': attach(key, value == '1')
    else: attach(key, value)
print(json.dumps(root))
""")
        source = UPDATER.read_text()
        source = source.replace(
            "data=/rom/usr/share/hh71vm-feed\n"
            "[ -f \"$data/release.pub\" ] && [ -f \"$data/release.conf\" ] || data=/usr/share/hh71vm-feed\n",
            f"data={self.root}/rom/usr/share/hh71vm-feed\n")
        replacements = {
            "/tmp/sysinfo/board_name": str(self.root / "tmp/sysinfo/board_name"),
            "/usr/share/libubox/jshn.sh": str(jshn),
            "lock=/tmp/autosysupgrade.lock": f"lock={self.root}/tmp/autosysupgrade.lock",
            "mktemp -d /tmp/autosysupgrade.XXXXXX": f"mktemp -d {self.root}/tmp/autosysupgrade.XXXXXX",
        }
        for old, new in replacements.items():
            source = source.replace(old, new)
        self.script = self.root / "autosysupgrade"
        self.script.write_text(source)
        self.script.chmod(0o755)
        self.log = self.root / "sysupgrade.log"
        self.environment = dict(os.environ,
            PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
            UPDATER_SYSUPGRADE_LOG=str(self.log), UPDATER_JSHN_RENDER=str(renderer),
            UPDATER_JSHN_LOG=str(self.root / "jshn.log"),
            UPDATER_FETCH_LOG=str(self.fetch_log))
        self.set_download_map()

    def set_download_map(self):
        mapping = {
            "/releases/latest/download/release.json.sig": str(self.fixtures / "latest.sig"),
            "/releases/latest/download/release.json": str(self.fixtures / "latest.json"),
            "api.github.com/repos/sowarden/hh71vm-openwrt/releases?": str(self.fixtures / "releases.json"),
            f"/download/{LATEST}/release.json.sig": str(self.fixtures / "latest.sig"),
            f"/download/{LATEST}/release.json": str(self.fixtures / "latest.json"),
            f"/download/{CURRENT}/release.json.sig": str(self.fixtures / "current.sig"),
            f"/download/{CURRENT}/release.json": str(self.fixtures / "current.json"),
            f"/download/{LATEST}/{ASSET}": str(self.fixtures / "image.bin"),
        }
        self.environment["UPDATER_MOCK_MAP"] = json.dumps(mapping)

    def execute(self, *args):
        return subprocess.run(["sh", str(self.script), *args], text=True, capture_output=True,
                              env=self.environment, timeout=20)

    def replace_script(self, old, new):
        source = self.script.read_text()
        self.assertIn(old, source)
        self.script.write_text(source.replace(old, new))

    def test_check_json_reports_newer_signed_release_without_flashing(self):
        result = self.execute("--check-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        status = json.loads(result.stdout)
        self.assertTrue(status["update_available"])
        self.assertFalse(status["installed_newer"])
        self.assertEqual(status["latest"], LATEST)
        self.assertEqual([item["tag"] for item in status["releases"]], [LATEST, CURRENT],
                         (status, result.stderr))
        self.assertEqual(status["releases"][0]["changes"], ["Add the LuCI updater."])
        self.assertFalse(self.log.exists())

    def test_local_status_needs_no_network_or_flash(self):
        environment = self.environment.copy()
        environment.pop("JSON_PREFIX", None)
        result = subprocess.run(
            ["sh", "-u", str(self.script), "--status-json"], text=True, capture_output=True,
            env=environment, timeout=20,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        status = json.loads(result.stdout)
        self.assertEqual(status["current"], CURRENT)
        self.assertFalse(status["checked"])
        self.assertFalse(self.fetch_log.exists())
        self.assertFalse(self.log.exists())

    def test_check_json_has_a_bounded_total_wall_clock_budget(self):
        self.replace_script("check_budget_seconds=20", "check_budget_seconds=4")
        self.replace_script("optional_request_timeout=3", "optional_request_timeout=2")
        self.environment["UPDATER_MOCK_DELAYS"] = json.dumps({
            "/repos/sowarden/hh71vm-openwrt/releases?per_page=20": 20,
        })
        started = time.monotonic()
        result = self.execute("--check-json")
        elapsed = time.monotonic() - started
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertLess(elapsed, 6, elapsed)
        status = json.loads(result.stdout)
        self.assertTrue(status["update_available"])
        self.assertFalse(status["history_complete"])

    def test_slow_required_descriptor_fails_inside_total_budget(self):
        self.replace_script("check_budget_seconds=20", "check_budget_seconds=4")
        self.replace_script("required_request_timeout=8", "required_request_timeout=3")
        self.environment["UPDATER_MOCK_DELAYS"] = json.dumps({
            "/releases/latest/download/release.json": 20,
        })
        started = time.monotonic()
        result = self.execute("--check-json")
        elapsed = time.monotonic() - started
        self.assertNotEqual(result.returncode, 0)
        self.assertLess(elapsed, 6, elapsed)
        self.assertRegex(result.stderr, r"signed release metadata|check timed out")
        self.assertFalse(self.log.exists())

    def test_unavailable_github_api_does_not_block_latest_result(self):
        self.environment["UPDATER_MOCK_FAILURES"] = json.dumps([
            "/repos/sowarden/hh71vm-openwrt/releases?per_page=20",
        ])
        result = self.execute("--check-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        status = json.loads(result.stdout)
        self.assertTrue(status["update_available"])
        self.assertFalse(status["history_complete"])
        self.assertEqual([item["tag"] for item in status["releases"]], [LATEST])

    def test_unavailable_history_descriptor_keeps_latest_successful(self):
        self.environment["UPDATER_MOCK_FAILURES"] = json.dumps([
            f"/download/{CURRENT}/release.json",
            f"/download/{CURRENT}/release.json.sig",
        ])
        result = self.execute("--check-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        status = json.loads(result.stdout)
        self.assertTrue(status["update_available"])
        self.assertFalse(status["history_complete"])
        self.assertEqual([item["tag"] for item in status["releases"]], [LATEST])

    def test_malformed_history_is_ignored_without_weakening_latest(self):
        (self.fixtures / "releases.json").write_text("{not json\n")
        result = self.execute("--check-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        status = json.loads(result.stdout)
        self.assertTrue(status["update_available"])
        self.assertFalse(status["history_complete"])
        self.assertEqual([item["tag"] for item in status["releases"]], [LATEST])

    def test_stale_lock_is_cleaned_before_check(self):
        lock = self.root / "tmp/autosysupgrade.lock"
        lock.mkdir()
        result = self.execute("--check-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(lock.exists())

    def test_live_lock_is_never_removed(self):
        lock = self.root / "tmp/autosysupgrade.lock"
        lock.mkdir()
        (lock / "pid").write_text(f"{os.getpid()}\n")
        result = self.execute("--check-json")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("another autosysupgrade process", result.stderr)
        self.assertTrue(lock.exists())

    def test_bad_signature_fails_before_image_or_sysupgrade(self):
        (self.fixtures / "latest.sig").write_text("bad\n")
        result = self.execute("--check-json")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("descriptor verification failed", result.stderr)
        self.assertFalse(self.log.exists())

    def test_same_release_is_not_offered_again(self):
        (self.root / "rom/usr/share/hh71vm-feed/release.conf").write_text(f"release={LATEST}\n")
        result = self.execute("--check-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        status = json.loads(result.stdout)
        self.assertFalse(status["update_available"])
        self.assertFalse(status["installed_newer"])

    def test_newer_installed_build_is_never_offered_as_a_downgrade(self):
        newer = "hh71vm-r00000000000000000102-a000001-cccccccccccc"
        (self.root / "rom/usr/share/hh71vm-feed/release.conf").write_text(f"release={newer}\n")
        result = self.execute("--check-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        status = json.loads(result.stdout)
        self.assertFalse(status["update_available"])
        self.assertTrue(status["installed_newer"])

    def test_history_rejects_invalid_dates(self):
        (self.fixtures / "releases.json").write_text(json.dumps([
            {"tag_name": CURRENT, "published_at": "not-a-date", "prerelease": False},
            {"tag_name": LATEST, "published_at": "2026-08-30T12:00:00Z", "prerelease": False},
        ]))
        result = self.execute("--check-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        status = json.loads(result.stdout)
        self.assertFalse(status["history_complete"])
        self.assertEqual([item["tag"] for item in status["releases"]], [LATEST])

    def test_unverified_history_entry_is_omitted_without_weakening_latest(self):
        (self.fixtures / "current.sig").write_text("bad\n")
        result = self.execute("--check-json")
        self.assertEqual(result.returncode, 0, result.stderr)
        status = json.loads(result.stdout)
        self.assertFalse(status["history_complete"])
        self.assertEqual([item["tag"] for item in status["releases"]], [LATEST])
        self.assertTrue(status["update_available"])

    def test_expected_release_blocks_check_to_upgrade_race(self):
        result = self.execute("--yes", "--expected", CURRENT)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("changed after the update check", result.stderr)
        self.assertFalse(self.log.exists())

    def test_confirmed_upgrade_tests_exact_image_then_flashes(self):
        result = self.execute("--yes", "--expected", LATEST)
        self.assertEqual(result.returncode, 0, result.stderr)
        calls = self.log.read_text().splitlines()
        self.assertEqual(len(calls), 2)
        self.assertTrue(calls[0].startswith("-T "))
        self.assertTrue(calls[1].startswith("-v "))

    def test_bad_image_checksum_never_reaches_sysupgrade(self):
        (self.fixtures / "image.bin").write_bytes(b"tampered\n")
        result = self.execute("--yes", "--expected", LATEST)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("SHA-256 mismatch", result.stderr)
        self.assertFalse(self.log.exists())


if __name__ == "__main__":
    unittest.main()
