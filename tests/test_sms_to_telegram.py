"""Host-only package, security and feed integration checks."""

import json
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "autobuild"))
import common

BACKEND = ROOT / "openwrt-feed/package/utils/sms-to-telegram"
LUCI = ROOT / "openwrt-feed/package/luci/applications/luci-app-sms-to-telegram"


class SmsToTelegramIntegrationTests(unittest.TestCase):
    def test_optional_packages_are_feed_roots_not_base_firmware(self):
        lock = json.loads((ROOT / "autobuild/lock.json").read_text(encoding="utf-8"))
        self.assertEqual(lock["config"]["CONFIG_PACKAGE_sms-to-telegram"], "m")
        self.assertEqual(lock["config"]["CONFIG_PACKAGE_luci-app-sms-to-telegram"], "m")
        config = (ROOT / "openwrt-feed/build.config").read_text(encoding="utf-8")
        self.assertIn("CONFIG_PACKAGE_sms-to-telegram=m", config)
        self.assertIn("CONFIG_PACKAGE_luci-app-sms-to-telegram=m", config)
        self.assertIn("luci-app-sms-to-telegram", common.ROOTS)

    def test_backend_uses_existing_modem_ubus_path(self):
        runtime = (BACKEND / "files/runtime.lua").read_text(encoding="utf-8")
        self.assertIn("modem_call('sms_snapshot')", runtime)
        self.assertIn("modem_call('sms_delete', { indexes = indexes })", runtime)
        self.assertNotRegex(runtime, r"/dev/smd|AT\+|192\.168\.225\.1|telnet")

    def test_https_transport_hides_request_and_requires_ca_validation(self):
        source = (BACKEND / "src/http_transport.c").read_text(encoding="utf-8")
        self.assertIn('uclient_http_set_ssl_ctx(client, ssl_ops, ssl_ctx, true)', source)
        self.assertIn('glob("/etc/ssl/certs/*.crt"', source)
        self.assertIn("unlink(path)", source)
        self.assertIn("(metadata.st_mode & 077) != 0", source)
        self.assertNotIn("no-check-certificate", source)
        runtime = (BACKEND / "files/runtime.lua").read_text(encoding="utf-8")
        self.assertNotIn("--post-data", runtime)
        self.assertNotIn("--post-file", runtime)
        self.assertNotIn("parse_mode", runtime)

    def test_secret_preserving_luci_and_minimal_rpc_surface(self):
        view = (LUCI / "htdocs/luci-static/resources/view/sms-to-telegram/main-1-0-0.js").read_text(encoding="utf-8")
        self.assertIn("'type': 'password'", view)
        self.assertIn("leave blank to keep", view)
        self.assertIn("configSet(token.value, chat.value, remove.checked)", view)
        rpc = (BACKEND / "files/rpcd-sms-to-telegram").read_text(encoding="utf-8")
        self.assertEqual(set(re.findall(r"^\s*([a-z_]+) = \{", rpc, re.MULTILINE)),
                         {"status", "config_get", "config_set", "discover_chat"})
        runtime = (BACKEND / "files/runtime.lua").read_text(encoding="utf-8")
        config_get = runtime[runtime.index("function R.config_get"):runtime.index("function R.discover_chat")]
        self.assertNotRegex(config_get, r"\btoken\s*=")
        status = (BACKEND / "files/sms_to_telegram.lua").read_text(encoding="utf-8")
        safe = status[status.index("function M.safe_status"):status.index("function M.telegram_response")]
        self.assertNotIn("token", safe)
        self.assertNotIn("chat_id", safe)
        self.assertNotIn("sender", safe)
        self.assertNotIn("text", safe)

    def test_package_dependencies_and_offline_documentation(self):
        makefile = (BACKEND / "Makefile").read_text(encoding="utf-8")
        for dependency in ("+libuclient", "+libustream-mbedtls", "+ca-bundle", "+libubus-lua", "+libuci-lua"):
            self.assertIn(dependency, makefile)
        docs = (ROOT / "docs/package-feed.md").read_text(encoding="utf-8")
        self.assertIn("opkg install luci-app-sms-to-telegram", docs)
        self.assertIn("sms-to-telegram_*.ipk", docs)
        self.assertIn("luci-app-sms-to-telegram_*.ipk", docs)

    def test_new_delta_contains_only_synthetic_identifiers(self):
        paths = [*BACKEND.rglob("*"), *LUCI.rglob("*"), ROOT / "docs/sms-to-telegram.md"]
        data = b"\n".join(path.read_bytes() for path in paths if path.is_file())
        forbidden = [rb"[A-Za-z]:[\\/]Users[\\/]", rb"/home/[^/\s]+/", rb"smet" + rb"mayo@",
                     rb"Clau" + rb"de", rb"Co" + rb"dex", rb"Open" + rb"AI",
                     rb"--no-check-certificate"]
        for pattern in forbidden:
            self.assertIsNone(re.search(pattern, data, re.IGNORECASE), pattern)
        self.assertIn(b"+15550001111", data)
        self.assertIn(b"123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghi", data)


if __name__ == "__main__":
    unittest.main()
