"""Host regressions for the RESET button factory reset.

Every property asserted here failed at least once on the device on 2026-09-07,
so none of them is decoration.
"""

from pathlib import Path
import re
import shutil
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files"
BUTTON = BASE / "usr/sbin/hh71vm-reset-button"
BLINK = BASE / "usr/sbin/hh71vm-panel-blink"
INIT = BASE / "etc/init.d/hh71vm-reset-button"


class InstallationTests(unittest.TestCase):
    """netifd, procd and the image builder all treat a non-executable script as absent."""

    def test_every_shipped_script_is_executable(self):
        # The mode git records is what reaches the image, and it is the only
        # readable answer on a Windows checkout: os.stat() there reports the
        # read-only attribute, never a POSIX execute bit, so a plain st_mode
        # check passes and fails at random depending on the build host.
        for path in (BUTTON, BLINK, INIT):
            self.assertTrue(path.is_file(), path)
        result = subprocess.run(
            ["git", "ls-files", "-s", "--", str(BUTTON), str(BLINK), str(INIT)],
            cwd=ROOT, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        entries = [line.split(None, 1) for line in result.stdout.splitlines()]
        self.assertEqual(len(entries), 3, f"not all tracked: {result.stdout!r}")
        for mode, rest in entries:
            self.assertEqual(mode, "100755", rest)

    def test_shell_scripts_parse(self):
        sh = shutil.which("sh") or shutil.which("bash")
        if not sh:
            self.skipTest("no POSIX shell on this host")
        for path in (BUTTON, INIT):
            # rc.common is an OpenWrt-only include, so only the button is fully
            # parseable here; the init script still has to be syntactically valid.
            result = subprocess.run([sh, "-n", str(path)], capture_output=True)
            self.assertEqual(result.returncode, 0, f"{path}: {result.stderr!r}")


class ResetButtonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = BUTTON.read_text(encoding="utf-8")

    def test_it_watches_the_reset_pin_and_not_wps(self):
        # GPIO 54 (G6) is RESET and 56 (H0) is WPS.  Confirmed on the device by
        # watching two held presses on 54 while 56 stayed idle.
        self.assertIn("RESET_GPIO=54", self.source)
        self.assertNotIn("RESET_GPIO=56", self.source)

    def test_it_never_writes_to_the_pin(self):
        # A blind export sweep of 0..63 killed the PCIe window and panicked the
        # kernel.  Only "export" and reads of value/direction are allowed here;
        # writing "direction" reconfigures a pad that is muxed with PCIe.
        self.assertNotRegex(self.source, r">\s*\"?\$GPIO_DIR/direction")
        self.assertNotRegex(self.source, r">\s*\"?\$GPIO_DIR/value")
        self.assertIn('direction="$(cat "$GPIO_DIR/direction"', self.source)
        self.assertIn("refusing to reconfigure the pad", self.source)

    def test_a_pin_stuck_low_cannot_wipe_the_settings(self):
        # Without this, a jammed button erases the configuration on every boot and
        # the symptom looks like the firmware losing its own settings.
        self.assertIn("armed=0", self.source)
        self.assertRegex(self.source, r'if \[ "\$armed" = 0 \]; then\s*\n\s*armed=1')
        self.assertIn("ignoring it until it goes high once", self.source)

    def test_the_threshold_is_ten_seconds_and_a_short_press_does_nothing(self):
        self.assertIn("RESET_HOLD_SECONDS=10", self.source)
        self.assertRegex(self.source, r'\[ "\$held" -ge "\$RESET_HOLD_SECONDS" \]')
        # A short press must not reboot: an accidental tap on a router is a footgun.
        # Look at the release branch itself, not at the whole file, whose header
        # comment legitimately talks about reboots.
        released = self.source[self.source.index("	1)"):]
        released = released[:released.index("	0)")]
        self.assertIn("nothing to do", released)
        self.assertNotIn("reboot", released)
        self.assertNotIn("jffs2reset", released)

    def test_it_refuses_when_there_is_no_overlay_to_erase(self):
        self.assertIn("overlay_mounted()", self.source)
        self.assertIn("grep -q ' /overlay ' /proc/mounts", self.source)
        self.assertIn("no /overlay is mounted", self.source)

    def test_an_unreadable_pin_is_not_a_pressed_pin(self):
        body = self.source[self.source.index("while :; do"):]
        self.assertIn("cannot read gpio", body)

    def test_the_indicator_can_never_delay_the_reset(self):
        # The panel lives on the other processor.  If that half is wedged, the
        # recovery still has to happen on time.
        announce = re.search(r"announce\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL)
        self.assertIsNotNone(announce)
        self.assertRegex(announce.group("body"), r"hh71vm-panel-blink 30 >/dev/null 2>&1 &")
        self.assertIn("[ -x /usr/sbin/hh71vm-panel-blink ] || return 0", announce.group("body"))

    def test_the_reset_still_runs_when_the_indicator_is_missing(self):
        trigger = self.source[self.source.index("erasing the overlay and rebooting"):]
        self.assertLess(trigger.index("announce"), trigger.index("jffs2reset"))
        self.assertIn("jffs2reset -y && reboot", trigger)


class PanelBlinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = BLINK.read_text(encoding="utf-8")

    def test_it_drives_the_leds_directly_rather_than_through_an_event(self):
        # The kcap event channel cannot do this: event 14 covers the wlan LED only
        # (ledMask=0x02), events 2 and 5 get repainted by core_app, and event 25
        # carries ledPowerOff=1 with no way to send the keyCode that distinguishes
        # "key long reset" from "key long power off".
        self.assertIn("/sys/class/leds/*/brightness", self.source)
        # The header comment explains why the event channel was rejected, so only
        # the code below it may be checked for an actual event call.
        code = self.source[self.source.index("]]"):]
        self.assertNotIn("MainLedStatus", code)
        self.assertNotIn("hh71vm-panel-led", code)

    def test_all_leds_change_in_one_process(self):
        # One echo per LED forks per file and the panel visibly tears, the first
        # and last indicator changing 10-30 ms apart.
        self.assertIn("tee /sys/class/leds/*/brightness", self.source)
        self.assertNotRegex(self.source, r"for f in /sys/class/leds")

    def test_the_loop_is_bounded_and_ends_with_the_panel_lit(self):
        # This side reboots and the Qualcomm side does not, so there is never a
        # second chance to tidy up.  "power" is in no event map, so whatever the
        # last write leaves is permanent -- ending dark leaves a working router
        # looking dead.
        command = self.source[self.source.index("local COMMAND"):]
        command = command[:command.index("local sock")]
        self.assertIn("while [ $i -lt %d ]", command)
        last_write = command.rindex("echo 255 | tee")
        self.assertGreater(last_write, command.rindex("echo 0 | tee"))
        self.assertIn("done; ", command)

    def test_it_stays_connected_until_the_far_shell_has_read_the_line(self):
        # Closing straight after the write returns 0 and runs nothing at all.
        self.assertIn("DRAIN_WAIT", self.source)
        drain = self.source.index("local drain = os.time() + DRAIN_WAIT")
        self.assertGreater(drain, self.source.index("local ok = write_all(COMMAND)"))

    def test_a_socket_timeout_is_not_treated_as_data(self):
        # nixio:read() answers false, not nil, and `#false` is a hard error that
        # took the whole helper down.
        self.assertNotIn('data == nil or data == ""', self.source)
        self.assertEqual(self.source.count('type(data) ~= "string"'), 2)


if __name__ == "__main__":
    unittest.main()
