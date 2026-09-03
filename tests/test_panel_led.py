"""Host regressions for the front panel indicator path.

The panel is on the Qualcomm half.  Writing /sys/class/leds/<name>/brightness there over
telnet looks like it works and does not: `core_app` repaints the file about 1.5 s later.
The stock firmware reports an *event* over the kcap channel instead, and that sticks.
These tests pin the channel and the event numbers so the old approach cannot come back.
"""

from pathlib import Path
import shutil
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files"
PANEL_LED = BASE / "usr/sbin/hh71vm-panel-led"
WIFI_LED = BASE / "usr/sbin/hh71vm-wifi-led"
HANDLER = BASE / "lib/netifd/wireless/rtl8192cd.sh"


class PanelLedChannelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = PANEL_LED.read_text(encoding="utf-8")

    def test_uses_the_kcap_control_channel(self):
        self.assertIn('"kcap"', self.source)
        self.assertIn("192.168.225.1", self.source)
        self.assertIn("2016", self.source)
        self.assertIn("MainLedStatus", self.source)
        self.assertIn("MainLedEvent", self.source)

    def test_does_not_write_qualcomm_sysfs_over_telnet(self):
        # The whole point of the rewrite.  A sysfs write survives ~1.5 s and is then
        # painted over by core_app, which is why the indicator looked fixed and was not.
        self.assertNotIn("/sys/class/leds", self.source.split("]]", 1)[1])
        self.assertNotIn("brightness", self.source.split("]]", 1)[1])

    def test_event_numbers_match_the_vendor_led_event_map(self):
        # /jrd-resource/resource/led-cfg/hh71/{generic,n1ru}/led_event_map.ini, both
        # agreeing, and the E_JRD_LED_EVENT_* name table in the stock core_app.
        self.assertIn('["wifi-off"] = 11', self.source)
        self.assertIn('["wifi-on"]  = 12', self.source)
        self.assertIn('["wifi-wps"] = 14', self.source)

    def test_rejects_an_unknown_event_instead_of_guessing(self):
        self.assertIn("EVENTS[arg1] or tonumber(arg1)", self.source)
        self.assertIn("usage(2)", self.source)


class WifiLedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = WIFI_LED.read_text(encoding="utf-8")

    def test_state_comes_from_interface_flags_and_only_from_the_radios(self):
        self.assertIn("/sys/class/net/wlan0 /sys/class/net/wlan1", self.source)
        self.assertIn("flags & 1", self.source)
        # The VAPs are deliberately not in the list: wlan0-va0 and friends always exist.
        self.assertNotIn("-va0", self.source.split("LOCK=")[-1])

    def test_flags_are_read_inside_the_lock(self):
        # netifd tears one radio down while bringing the other up and calls this in the
        # background from both paths.  Reading the flags before taking the lock lets the
        # loser send wifi-off after the winner has already sent wifi-on.
        lock = self.source.index("flock 9")
        first_read = self.source.index("/sys/class/net/wlan0")
        self.assertLess(lock, first_read)

    def test_passes_named_events_to_the_panel_helper(self):
        self.assertIn("/usr/sbin/hh71vm-panel-led wifi-on", self.source)
        self.assertIn("/usr/sbin/hh71vm-panel-led wifi-off", self.source)


class PanelLedPackagingTests(unittest.TestCase):
    def test_both_helpers_are_recorded_executable_in_git(self):
        # /usr/sbin gets +x from the OpenWrt build itself, so this is belt and braces
        # rather than the load-bearing check /lib/netifd/wireless needs -- but a Lua
        # script that is not executable fails in exactly the same silent way.
        git = shutil.which("git")
        if git is None:
            self.skipTest("git is not available")
        for path in (PANEL_LED, WIFI_LED):
            relative = path.relative_to(ROOT).as_posix()
            result = subprocess.run(
                [git, "ls-files", "-s", "--", relative],
                cwd=ROOT, capture_output=True, text=True, check=False,
            )
            if result.returncode != 0 or not result.stdout.strip():
                self.skipTest("the helpers are not tracked in a git checkout here")
            self.assertEqual(result.stdout.split()[0], "100755", relative)

    def test_the_handler_still_calls_the_helper_on_both_edges(self):
        source = HANDLER.read_text(encoding="utf-8")
        self.assertEqual(source.count("/usr/sbin/hh71vm-wifi-led >/dev/null 2>&1 &"), 2)


if __name__ == "__main__":
    unittest.main()
