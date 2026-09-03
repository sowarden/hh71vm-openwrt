"""Host regressions for the USSD path.

USSD is not a request/response pair.  AT+CUSD returns OK almost at once and the text
comes back later as a +CUSD URC, on the network's schedule.  The first implementation
waited on it inside one RPC with a 25 s budget that started when the request was
QUEUED, so anything already in the AT queue -- an operator scan is up to 180 s, an SMS
submit up to 330 s -- burned the whole window before the code was ever sent, and the
user got "USSD timeout" from a network that had not been asked anything.

Measured on the device 2026-09-02: with the old daemon an idle modem answered *100# in
1 s, and the same code sent while an operator scan was running failed with
"USSD timeout" after exactly 25 s.  With the job API the same sequence waited 63 s in
the queue, spent 8 s on the network and returned the answer.
"""

from pathlib import Path
import json
import unittest


ROOT = Path(__file__).resolve().parents[1]
DAEMON = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files/usr/sbin/hh71vm-modemd"
RPCD = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files/usr/libexec/rpcd/hh71vm-modem"
APP = ROOT / "openwrt-feed/package/luci/applications/luci-app-hh71vm-modem"
MODEM_JS = APP / "htdocs/luci-static/resources/hh71vm/modem.js"
NETWORK_JS = APP / "htdocs/luci-static/resources/view/hh71vm/network.js"
ACL = APP / "root/usr/share/rpcd/acl.d/luci-app-hh71vm-modem.json"


def text(path):
    return path.read_text(encoding="utf-8")


class UssdDaemonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = text(DAEMON)

    def test_the_job_api_exists(self):
        self.assertIn("function API.ussd_start(cli, args)", self.source)
        self.assertIn("function API.ussd_result(cli)", self.source)

    def test_the_network_budget_starts_at_dispatch_not_at_enqueue(self):
        # The whole point.  The clock is set in the on_dispatch hook, which runs when
        # the request leaves the queue, and nowhere else.
        self.assertIn("M.ussd.deadline = mono() + CFG.ussd_timeout", self.source)
        before, after = self.source.split("M.ussd.deadline = mono() + CFG.ussd_timeout", 1)
        self.assertIn("end, function()", before[-400:],
                      "the deadline must be set from the dispatch hook")
        self.assertEqual(self.source.count("M.ussd.deadline = mono()"), 1)

    def test_the_queue_has_a_dispatch_hook_and_uses_it(self):
        self.assertIn("local function request(name, steps, cb, on_dispatch)", self.source)
        self.assertIn("if r.on_dispatch then", self.source)

    def test_the_budget_is_no_longer_tied_to_any_rpc_ceiling(self):
        # A polled job may outlast the 30 s /admin/ubus ceiling, so the budget is the
        # network's, not the proxy's.
        self.assertRegex(self.source, r"ussd_timeout\s*=\s*(\d+),")
        budget = int(self.source.split("ussd_timeout")[1].split("=")[1].split(",")[0])
        self.assertGreaterEqual(budget, 60)

    def test_only_one_job_runs_at_a_time(self):
        # Nothing in a +CUSD line says which request it answers.
        self.assertIn('if M.ussd and M.ussd.state == "running" then return true end',
                      self.source)

    def test_the_blocking_call_survives_for_older_pages(self):
        self.assertIn("function API.ussd(cli, args)", self.source)
        self.assertIn("M.ussd_blocking", self.source)

    def test_the_old_enqueue_time_deadline_is_gone(self):
        self.assertNotIn("M.ussd_deadline", self.source)
        self.assertNotIn("M.ussd_client", self.source)


class UssdPlumbingTests(unittest.TestCase):
    def test_rpcd_exposes_both_job_methods(self):
        source = text(RPCD)
        self.assertIn('ussd_start      = { code = "str" }', source)
        self.assertIn("ussd_result     = {}", source)

    def test_the_acl_grants_them(self):
        acl = json.loads(text(ACL))["luci-app-hh71vm-modem"]
        self.assertIn("ussd_result", acl["read"]["ubus"]["hh71vm-modem"])
        self.assertIn("ussd_start", acl["write"]["ubus"]["hh71vm-modem"])
        # Reading a result must not need write rights, and starting one must not be
        # reachable from a read-only session.
        self.assertNotIn("ussd_start", acl["read"]["ubus"]["hh71vm-modem"])


class UssdWebUiTests(unittest.TestCase):
    def test_the_client_polls_instead_of_holding_one_call_open(self):
        source = text(MODEM_JS)
        self.assertIn("ussdStart:     decl('ussd_start', ['code'])", source)
        self.assertIn("ussdResult:    decl('ussd_result')", source)
        self.assertIn("ussdRun: function (code, onProgress)", source)
        self.assertIn("self.ussdResult()", source)

    def test_the_client_falls_back_to_the_blocking_call_on_an_older_daemon(self):
        source = text(MODEM_JS)
        self.assertIn("if (res.state == null && res.error == null) return self.ussd(code);",
                      source)

    def test_the_dialog_uses_the_job(self):
        source = text(NETWORK_JS)
        self.assertIn("m.api.ussdRun(c", source)
        self.assertNotIn("m.api.ussd(c)", source)


if __name__ == "__main__":
    unittest.main()
