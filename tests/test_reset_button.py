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

    def test_the_startup_line_cannot_claim_a_dry_run_it_is_not_doing(self):
        # ${DRY_RUN:+...} expands whenever the variable is set, and it is always
        # set -- to 0 in the real mode -- so the daemon announced "dry run" while
        # armed to erase the overlay for real.
        self.assertNotIn("defaults${DRY_RUN:+", self.source)
        self.assertIn('[ "$DRY_RUN" = 1 ] && mode=" (dry run)"', self.source)
        self.assertIn('hold resets to defaults$mode"', self.source)

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
        body = self.source.split("announce() {", 1)[1].split("}", 1)[0]
        self.assertIn("hh71vm-panel-blink 45 >/dev/null 2>&1 &", body)
        self.assertIn("[ -x /usr/sbin/hh71vm-panel-blink ] || return 0", body)

    def test_the_flashing_panel_means_the_button_can_be_released(self):
        # The signal must never mean "keep holding": a user who lets go on it
        # would cancel the very thing it announced. Once the threshold is
        # reached the button is not read again.
        self.assertIn("ANNOUNCE_SETTLE=4", self.source)
        self.assertNotIn("ANNOUNCE_AT", self.source)
        self.assertIn("the button can be released", self.source)
        trigger = self.source[self.source.index("reset committed"):]

        def where(text):
            return trigger.index(text)

        # Commit, then start the panel, then wait for it to actually be
        # flashing, and only then pull the floor out.
        self.assertLess(where("announce"), where('sleep "$ANNOUNCE_SETTLE"'))
        self.assertLess(where('sleep "$ANNOUNCE_SETTLE"'), where('exec "$STAGE"'))


    def test_the_reset_runs_from_tmpfs_not_from_the_file_being_deleted(self):
        # jffs2reset deletes /overlay/upper, and this script itself lives there
        # when it has been installed by hand. A shell reads its script one line
        # at a time, so the line after jffs2reset can never be fetched and the
        # reboot never happens. That wedged the board three times on 2026-09-07,
        # and testing the same commands by hand over SSH did not reproduce it,
        # because typed commands run from /rom rather than from the deleted file.
        self.assertIn("STAGE=/tmp/hh71vm-reset-finish", self.source)
        trigger = self.source[self.source.index("reset committed"):]
        self.assertIn('exec "$STAGE"', trigger)
        # Nothing destructive may be *run* from this file. A log line that
        # merely names the command is fine; a line that invokes it is not.
        for line in trigger.split(chr(10)):
            bare = line.strip()
            self.assertFalse(bare.startswith("jffs2reset"), line)
            self.assertFalse(bare.startswith("reboot"), line)

    def test_the_staged_script_needs_no_filesystem_after_the_erase(self):
        lines = self.source.split(chr(10))
        start = next(i for i, l in enumerate(lines) if l.startswith("stage_reset() {"))
        end = next(i for i in range(start + 1, len(lines)) if lines[i] == "}")
        stage = lines[start:end]

        def where(text):
            return next(i for i, l in enumerate(stage) if text in l)

        for want in ("jffs2reset -y", "echo b > /proc/sysrq-trigger", "reboot -f"):
            self.assertTrue(any(want in l for l in stage), want)
        # A shell builtin writing to procfs needs nothing looked up or executed.
        # reboot -f does have to exec, which is why it may only be the fallback.
        self.assertLess(where("jffs2reset -y"), where("/proc/sysrq-trigger"))
        self.assertLess(where("/proc/sysrq-trigger"), where("reboot -f"))

    def test_nothing_is_destroyed_unless_the_reboot_is_already_staged(self):
        # A reset that erases and then cannot reboot is the worst outcome there
        # is, and it is the one that actually kept happening.
        loop = self.source[self.source.index("while :; do"):]
        self.assertIn("elif ! stage_reset; then", loop)
        self.assertIn("refusing to erase anything", loop)


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
