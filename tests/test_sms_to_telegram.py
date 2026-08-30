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
    def read_view(self):
        return (LUCI / "htdocs/luci-static/resources/view/sms-to-telegram/main-1-1-0.js").read_text(
            encoding="utf-8"
        )

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
        self.assertIn("uclient_http_set_ssl_ctx(client, ssl_ops, ssl_ctx, true)", source)
        self.assertIn('glob("/etc/ssl/certs/*.crt"', source)
        self.assertIn("unlink(path)", source)
        self.assertIn("(metadata.st_mode & 077) != 0", source)
        self.assertNotIn("no-check-certificate", source)
        runtime = (BACKEND / "files/runtime.lua").read_text(encoding="utf-8")
        self.assertNotIn("--post-data", runtime)
        self.assertNotIn("--post-file", runtime)
        self.assertNotIn("parse_mode", runtime)

    def test_secret_preserving_luci_and_minimal_rpc_surface(self):
        view = self.read_view()
        self.assertIn("'type': 'password'", view)
        self.assertIn("Leave this field blank to keep the currently configured token.", view)
        self.assertIn("configSet(this.form.token.value, this.form.chat.value", view)
        self.assertNotIn("innerHTML", view)
        rpc = (BACKEND / "files/rpcd-sms-to-telegram").read_text(encoding="utf-8")
        self.assertEqual(
            set(re.findall(r"^\s*([a-z_]+) = \{", rpc, re.MULTILINE)),
            {"status", "config_get", "config_set", "discover_chat"},
        )
        runtime = (BACKEND / "files/runtime.lua").read_text(encoding="utf-8")
        config_get = runtime[
            runtime.index("function R.config_get") : runtime.index("function R.discover_chat")
        ]
        self.assertNotRegex(config_get, r"\btoken\s*=")
        status = (BACKEND / "files/sms_to_telegram.lua").read_text(encoding="utf-8")
        safe = status[status.index("function M.safe_status") : status.index("function M.telegram_response")]
        self.assertNotIn("token", safe)
        self.assertNotIn("chat_id", safe)
        self.assertNotIn("sender", safe)
        self.assertNotIn("text", safe)

        acl = json.loads(
            (LUCI / "root/usr/share/rpcd/acl.d/luci-app-sms-to-telegram.json").read_text(
                encoding="utf-8"
            )
        )["luci-app-sms-to-telegram"]
        self.assertEqual(acl["read"]["ubus"]["sms-to-telegram"], ["status", "config_get"])
        self.assertEqual(acl["write"]["ubus"]["sms-to-telegram"], ["config_set", "discover_chat"])

    def test_logs_and_errors_do_not_include_sensitive_values(self):
        runtime = (BACKEND / "files/runtime.lua").read_text(encoding="utf-8")
        syslog_calls = re.findall(r"n\.syslog\([^\n]+", runtime)
        self.assertEqual(syslog_calls, ["n.syslog('err', 'sms-to-telegram: internal_error')"])
        discovery = runtime[runtime.index("function R.discover_chat") : runtime.index("function R.run")]
        self.assertNotRegex(discovery, r"sender|message\.text|description|chat_id\s*=")

    def test_page_is_a_six_step_setup_flow_with_standard_actions(self):
        view = self.read_view()
        for number, title in enumerate(
            (
                "Create a Telegram bot",
                "Message the bot",
                "Select a detected recipient",
                "Confirm or edit the recipient",
                "Optional SIM deletion",
                "Apply configuration",
            ),
            1,
        ):
            self.assertIn(f"step({number}, _('" + title + "')", view)
        self.assertIn("handleSaveApply: function()", view)
        self.assertIn("handleSave: null", view)
        self.assertIn("handleReset: function()", view)
        self.assertNotRegex(view, r"}, _\('Save'\)\)")
        self.assertIn("Save & Apply", view)
        self.assertIn("max-width:100%", view)
        self.assertIn("overflow-wrap:anywhere", view)

    def test_discovery_is_transient_single_selection_and_dom_safe(self):
        view = self.read_view()
        self.assertIn("Detect Chat IDs", view)
        self.assertIn("result.candidates.forEach", view)
        self.assertIn("'type': 'radio'", view)
        self.assertIn("chat.value = candidate.chat_id", view)
        self.assertIn("document.createTextNode", view)
        self.assertIn("E('bdi', { 'dir': 'auto' }", view)
        detection = view[view.index("var detect =") : view.index("this.busy = false")]
        self.assertNotIn("configSet", detection)
        self.assertIn("A blank field uses the saved token.", view)

    def test_manual_chat_validation_and_actionable_errors(self):
        view = self.read_view()
        self.assertIn("!/^[1-9][0-9]{4,19}$/.test(chat)", view)
        for code in (
            "invalid_token",
            "invalid_chat_id",
            "no_private_chat",
            "too_many_private_chats",
            "telegram_rate_limited",
            "telegram_http_error",
            "telegram_api_error",
            "telegram_transport_failed",
            "telegram_invalid_response",
        ):
            self.assertIn(code + ":", view)

    def test_discovery_returns_only_bounded_normalized_candidates(self):
        core = (BACKEND / "files/sms_to_telegram.lua").read_text(encoding="utf-8")
        runtime = (BACKEND / "files/runtime.lua").read_text(encoding="utf-8")
        discovery = runtime[runtime.index("function R.discover_chat") : runtime.index("function R.run")]
        self.assertIn("core.discovery_result(response)", discovery)
        self.assertNotIn("result = response.result", discovery)
        self.assertIn("#updates > 100", core)
        self.assertIn("#order >= 20", core)
        self.assertIn("chat.type == 'private'", core)
        self.assertNotIn("message.text", core[core.index("function M.private_chat_candidates") :])

    def test_discovery_and_delivery_have_bounded_network_behavior(self):
        runtime = (BACKEND / "files/runtime.lua").read_text(encoding="utf-8")
        source = (BACKEND / "src/http_transport.c").read_text(encoding="utf-8")
        core = (BACKEND / "files/sms_to_telegram.lua").read_text(encoding="utf-8")
        self.assertIn("timeout = 0", runtime)
        self.assertIn("limit = 50", runtime)
        self.assertIn("uclient_set_timeout(client, 25000)", source)
        self.assertIn("math.min(3600", core)

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
        forbidden = [
            rb"[A-Za-z]:[\\/]Users[\\/]",
            rb"/home/[^/\s]+/",
            rb"smet" + rb"mayo@",
            rb"Clau" + rb"de",
            rb"Co" + rb"dex",
            rb"Open" + rb"AI",
            rb"--no-check-certificate",
        ]
        for pattern in forbidden:
            self.assertIsNone(re.search(pattern, data, re.IGNORECASE), pattern)
        self.assertIn(b"+15550001111", data)
        self.assertIn(b"123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghi", data)


if __name__ == "__main__":
    unittest.main()
