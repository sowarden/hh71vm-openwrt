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
        self.assertIn("#maincontent > .alert-message", SCRIPT)
        self.assertIn("list[i].style.display !== 'flex'", SCRIPT)
        self.assertIn("hh-notifications", SCRIPT)

    def test_notification_tray_is_fixed_and_responsive(self):
        self.assertIn("#hh-notifications {", STYLE)
        self.assertIn("position: fixed", STYLE)
        self.assertIn("width: min(430px, calc(100vw - 40px))", STYLE)
        self.assertIn("#hh-notifications > .alert-message", STYLE)


if __name__ == "__main__":
    unittest.main()
