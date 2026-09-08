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
        # The listing budget covers every store together, not each of them: the
        # browser holds one call open while all of them are read.
        budget = int(re.search(
            r"local SMS_LIST_BUDGET\s*=\s*(\d+)", daemon).group(1))
        self.assertLess(budget, timeout)
        self.assertLess(timeout, 30)
        self.assertIn("sms_list_job(function", daemon)
        self.assertIn("SMS_LIST_BUDGET))", daemon)
        self.assertIn("math.floor((budget or 20) / #stores)", daemon)
        self.assertIn('io.write(call(method, params), "\\n")', rpcd)
        self.assertIn('fail("no answer from modem daemon")', rpcd)

    def test_every_slot_numbered_call_can_name_its_message_store(self):
        """Slot numbers restart in each store, so an index alone is ambiguous.

        Measured on the stand: ME held indexes 0-7 while SM held 0-9 on the same
        modem.  A delete that cannot name a store is a delete that can destroy the
        wrong message.
        """
        rpcd = text(RPCD)
        daemon = text(DAEMON)
        modem = text(MODEM_JS)
        page = text(SMS_JS)
        forwarder = text(FORWARDER)

        for method in ("sms_read", "sms_delete", "sms_mark",
                       "sms_delete_all", "sms_list", "sms_save"):
            signature = re.search(rf"\b{method}\s*=\s*\{{(.*?)\}},?\n", rpcd, re.S)
            self.assertIsNotNone(signature, method)
            self.assertIn("storage", signature.group(1), method)

        self.assertIn("sms_settings_set = { sca = \"str\", storage_mode = \"str\" }", rpcd)
        self.assertIn("decl('sms_read',     ['index', 'storage'])", modem)
        self.assertIn("decl('sms_mark',     ['index', 'ts', 'read', 'storage'])", modem)
        self.assertIn("decl('sms_settings_set', ['sca', 'storage_mode'])", modem)

        # The page hands each message's own store back with the action.
        self.assertIn("m.api.smsMark(msg.index, msg.ts,", page)
        self.assertIn("msg.unread === true, msg.storage)", page)
        self.assertIn("msg.indexes || [msg.index],", page)
        self.assertIn("msg.storage)", page)

        # The daemon selects that store before any slot-numbered command.
        self.assertIn("local function sms_select_step(storage)", daemon)
        self.assertIn("sms_storage_name(args.storage) or sms_storage_for(wanted)", daemon)
        # and the forwarder carries it through its delete.
        self.assertIn("self.env.delete_sms(record.indexes, record.storage)", forwarder)

    def test_message_fingerprint_survives_a_32_bit_lua(self):
        """Lua 5.1 casts "%x" through a signed 32-bit integer on the target.

        A hash with the top bit set aborted the whole listing there with "bad
        argument #1 to 'format'", while the same code is harmless on a 64-bit
        development host -- so only a source check catches a revert.  A stored
        draft reaches this path every time, because it carries no timestamp.
        """
        daemon = text(DAEMON)
        self.assertIn(
            '("%04x%04x"):format(math.floor(hash / 65536) % 65536, hash % 65536)',
            daemon)
        self.assertNotIn('("%08x"):format(hash)', daemon)

    def test_the_read_store_is_never_chosen_from_the_receive_memory(self):
        """The regression this replaces.

        Both units report SM as CPMS receive memory, yet the owner's modem kept
        eight messages in ME.  Preferring the receive memory made every refresh
        read SM alone and those messages stopped being listed at all.
        """
        daemon = text(DAEMON)
        self.assertNotIn("requested_storage or sms_state.receive_storage", daemon)
        self.assertIn("local function sms_active_stores(requested)", daemon)
        self.assertIn('local SMS_STORES     = { "ME", "SM" }', daemon)
        self.assertIn('local SMS_MODE_DEFAULT = "both"', daemon)
        # A +CMTI must not narrow a refresh to the store it names.
        self.assertNotIn("end, M.sms_sync_storage))", daemon)

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
        self.assertIn("smsList:      function (store)", modem)
        self.assertIn("callLong('sms_list',", modem)
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
