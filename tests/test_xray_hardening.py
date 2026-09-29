"""Hardening of the Xray service scripts: clock, hotplug, DNS restarts, the HTTP API."""

import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
FILES = ROOT / "openwrt-feed/package/net/xray-core/files"
LUA = shutil.which("lua5.1") or shutil.which("lua")

PROBE = r"""
package.preload["luci.jsonc"] = function() return { parse = function() end, stringify = function() return "" end } end
package.preload["nixio"] = function() return { fs = {} } end
local X = dofile(arg[1])
local built = 1788000000
local function yes(v) io.write(v and "1" or "0") end
yes(X.plausible_time(built + 86400 * 30, built, built))           -- behind: move forward
yes(X.plausible_time(built - 86400, built + 86400, built))         -- before the image
yes(X.plausible_time(built + 86400 * 366 * 11, built, built))      -- decades ahead
yes(X.plausible_time(built + 86400 * 5, built + 86400 * 10, built)) -- five days backwards
yes(X.plausible_time(built + 86400 * 10 - 600, built + 86400 * 10, built)) -- ten minutes back
yes(X.plausible_time(built + 60, built, 0))                         -- no build time known
"""


class XrayHardening(unittest.TestCase):
    @unittest.skipUnless(LUA, "requires a Lua interpreter")
    def test_the_clock_only_moves_to_a_plausible_time(self):
        with tempfile.NamedTemporaryFile("w", suffix=".lua") as probe:
            probe.write(PROBE)
            probe.flush()
            out = subprocess.run([LUA, probe.name, str(FILES / "xray-lib.lua")],
                                 capture_output=True, text=True, check=True)
        self.assertEqual(out.stdout, "100011", out.stderr)

    def test_sync_clock_checks_plausibility_before_setting_the_clock(self):
        lib = (FILES / "xray-lib.lua").read_text()
        body = lib.split("function M.sync_clock(date_header)", 1)[1].split("\nend", 1)[0]
        self.assertLess(body.index("M.plausible_time("), body.index("/bin/date -u -s"))

    def test_hotplug_acts_on_the_last_event_of_a_burst(self):
        hook = (FILES / "xray.hotplug").read_text()
        self.assertNotIn("-lt 5 ] && exit 0", hook)
        self.assertIn('[ "$(cat "$STAMP" 2>/dev/null)" = "$token" ] || exit 0', hook)
        self.assertIn(") </dev/null >/dev/null 2>&1 &", hook)

    def test_dnsmasq_is_restarted_only_when_the_override_changes(self):
        fw = (FILES / "hh71vm-xray-fw").read_text()
        body = fw.split("dns_capture() {", 1)[1].split("\n}", 1)[0]
        self.assertIn('= "$want" ] && return 0', body)
        self.assertLess(body.index("return 0"), body.index("dnsmasq restart"))

    def test_api_token_check_and_activate(self):
        api = (FILES / "xray-api.cgi").read_text()
        self.assertIn("if not same(token, s.api_token) then", api)
        self.assertNotIn("if token ~= s.api_token then", api)
        self.assertIn("if X.bool(s.enabled) and X.pid() then", api)


if __name__ == "__main__":
    unittest.main()
