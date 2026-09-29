"""Small robustness contracts of the modem daemon and its service script."""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files"
DAEMON = (BASE / "usr/sbin/hh71vm-modemd").read_text()
INIT = (BASE / "etc/init.d/hh71vm-modemd").read_text()


class ModemdRobustness(unittest.TestCase):
    def test_a_wan_event_does_not_restart_a_working_channel(self):
        reload_body = INIT.split("reload_service() {", 1)[1].split("\n}", 1)[0]
        self.assertIn("@.link.state", reload_body)
        self.assertRegex(reload_body, r"ready\)\s*return 0")
        self.assertIn("--call reconnect", reload_body)
        # restart only when the daemon does not answer at all
        self.assertRegex(reload_body, r'""\)\s*restart')

    def test_the_concatenation_reference_is_seeded(self):
        seed = DAEMON.index("math.randomseed(")
        first_use = DAEMON.index("local sms_concat_ref = math.random(0, 255)")
        self.assertLess(seed, first_use)
        self.assertIn('io.open("/dev/urandom", "rb")', DAEMON[seed - 400:seed])

    def test_the_saved_apn_password_is_owner_only(self):
        body = DAEMON.split("local function apn_state_set(entry)", 1)[1].split("\nend", 1)[0]
        chmod = body.index('fs.chmod(APN_STATE_FILE .. ".tmp", 600)')
        self.assertLess(chmod, body.index("f:write("))


if __name__ == "__main__":
    unittest.main()
