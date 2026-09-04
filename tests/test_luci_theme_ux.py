"""Focused contracts for theme behavior that a firmware build cannot exercise."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
THEME = ROOT / "openwrt-feed/package/luci/themes/luci-theme-hh71vm"
SCRIPT = (THEME / "htdocs/luci-static/hh71vm/hh71vm.js").read_text(encoding="utf-8")
STYLE = (THEME / "htdocs/luci-static/hh71vm/cascade.css").read_text(encoding="utf-8")


class NotificationUxContractTests(unittest.TestCase):
    def test_dynamic_notifications_are_moved_to_a_viewport_tray(self):
        self.assertIn("function collectNotifications()", SCRIPT)
        self.assertIn("function armNotification(item)", SCRIPT)
        self.assertIn("remaining = 10000", SCRIPT)
        self.assertIn("pointerenter", SCRIPT)
        self.assertIn("touchstart", SCRIPT)
        self.assertIn("resumeOutside", SCRIPT)
        self.assertIn("#maincontent > .alert-message", SCRIPT)
        self.assertIn("list[i].style.display !== 'flex'", SCRIPT)
        self.assertIn("hh-notifications", SCRIPT)

    def test_notification_tray_is_fixed_and_responsive(self):
        self.assertIn("#hh-notifications {", STYLE)
        self.assertIn("position: fixed", STYLE)
        self.assertIn("width: min(430px, calc(100vw - 40px))", STYLE)
        self.assertIn("#hh-notifications > .alert-message", STYLE)

    def test_pending_buttons_keep_their_label_and_show_busy_feedback(self):
        self.assertIn(".cbi-dropdown.spinning { gap: 0; }", STYLE)
        self.assertIn("margin-left: 10px; margin-right: 6px", STYLE)
        self.assertIn("color: inherit; padding-left: 0", STYLE)
        self.assertIn("gap: 6px", STYLE)
        self.assertIn(".btn.spinning, .cbi-button.spinning, button.spinning", STYLE)
        self.assertIn("cursor: wait", STYLE)
        self.assertIn("pointer-events: none", STYLE)


if __name__ == "__main__":
    unittest.main()
