"""Static integration contracts around the serialized SMS pipeline.

The parser and LuCI renderer have executable fixture harnesses.  These checks cover
the thin boundaries between them so a later packaging change cannot silently turn a
daemon failure into an empty inbox or make the indicator read a different source.
"""

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files"
APP = ROOT / "openwrt-feed/package/luci/applications/luci-app-hh71vm-modem"
DAEMON = BASE / "usr/sbin/hh71vm-modemd"
RPCD = BASE / "usr/libexec/rpcd/hh71vm-modem"
MODEM_JS = APP / "htdocs/luci-static/resources/hh71vm/modem.js"
SMS_JS = APP / "htdocs/luci-static/resources/view/hh71vm/sms.js"
ACL = APP / "root/usr/share/rpcd/acl.d/luci-app-hh71vm-modem.json"
THEME_JS = (ROOT / "openwrt-feed/package/luci/themes/luci-theme-hh71vm/htdocs/"
            "luci-static/hh71vm/hh71vm.js")
FORWARDER = (ROOT / "openwrt-feed/package/utils/sms-to-telegram/files/"
             "sms_to_telegram.lua")


def text(path):
    return path.read_text(encoding="utf-8")


class SmsPipelineContractTests(unittest.TestCase):
    def test_rpcd_relays_daemon_json_and_has_a_bounded_timeout(self):
        rpcd = text(RPCD)
        daemon = text(DAEMON)
        timeout = int(re.search(r"local TIMEOUT\s*=\s*(\d+)", rpcd).group(1))
        cmgl_timeout = int(re.search(
            r'cmd = "AT\+CMGL=4", parse = parse, timeout = (\d+)', daemon).group(1))
        self.assertLess(cmgl_timeout, timeout)
        self.assertLess(timeout, 30)
        self.assertIn('io.write(call(method, params), "\\n")', rpcd)
        self.assertIn('fail("no answer from modem daemon")', rpcd)

    def test_rpcd_and_acl_expose_every_sms_read_boundary(self):
        rpcd = text(RPCD)
        acl = json.loads(text(ACL))["luci-app-hh71vm-modem"]
        readable = acl["read"]["ubus"]["hh71vm-modem"]
        for method in ("sms_list", "sms_snapshot", "sms_settings", "sms_read"):
            self.assertRegex(rpcd, rf"\b{method}\s*=")
            self.assertIn(method, readable)

    def test_browser_uses_long_list_call_and_cache_fallback(self):
        modem = text(MODEM_JS)
        page = text(SMS_JS)
        self.assertIn("smsList:      function ()", modem)
        self.assertIn("callLong('sms_list', {}, 28)", modem)
        self.assertIn("smsSnapshot:     decl('sms_snapshot')", modem)
        self.assertIn("L.resolveDefault(m.api.smsSnapshot(), {})", page)
        self.assertIn("list.ok === true && !list.error", page)
        self.assertIn("No message rows can be shown until", page)

    def test_indicator_and_page_share_daemon_status(self):
        theme = text(THEME_JS)
        self.assertIn("var unread = sms.unread || 0", theme)
        self.assertIn("var count = (sms.count != null) ? sms.count : null", theme)
        self.assertIn("HH.url('admin/modem/sms')", theme)

    def test_forwarder_rejects_failed_or_stale_snapshot(self):
        forwarder = text(FORWARDER)
        self.assertIn("snapshot.ok ~= true", forwarder)
        self.assertIn("type(snapshot.messages) ~= 'table'", forwarder)


if __name__ == "__main__":
    unittest.main()
