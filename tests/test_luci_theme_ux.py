"""Focused contracts for theme behavior that a firmware build cannot exercise."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
THEME = ROOT / "openwrt-feed/package/luci/themes/luci-theme-hh71vm"
SCRIPT = (THEME / "htdocs/luci-static/hh71vm/hh71vm.js").read_text(encoding="utf-8")
STYLE = (THEME / "htdocs/luci-static/hh71vm/cascade.css").read_text(encoding="utf-8")
HEADER = (THEME / "luasrc/view/themes/hh71vm/header.htm").read_text(encoding="utf-8")


def phone_rules():
    """The body of the 720px drawer media query."""
    return STYLE.split("@media (max-width: 720px) {", 1)[1].split("\n@media", 1)[0]


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

    def test_problems_are_not_dismissed_by_a_timer(self):
        arm = SCRIPT.split("function armNotification(item) {", 1)[1].split("\n\t}\n", 1)[0]
        self.assertLess(arm.index("if (isProblem(item)) {"), arm.index("start();"))
        self.assertIn("'error'", SCRIPT.split("function isProblem(item) {", 1)[1][:200])

    def test_the_timer_only_ever_presses_dismiss(self):
        self.assertIn("button = last ? last.querySelector('.btn') : null", SCRIPT)
        self.assertNotIn("var button = item.querySelector('.btn');", SCRIPT)

    def test_the_tray_is_announced(self):
        self.assertIn("'aria-live': 'polite'", SCRIPT)

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


class DrawerAccessibilityTests(unittest.TestCase):
    def test_a_closed_drawer_is_hidden_not_just_moved_away(self):
        phone = phone_rules()
        closed = phone.split("\t#sidebar {", 1)[1].split("}", 1)[0]
        opened = phone.split("body.nav-open #sidebar {", 1)[1].split("}", 1)[0]
        self.assertIn("visibility: hidden", closed)
        self.assertIn("visibility: visible", opened)

    def test_focus_moves_into_the_drawer_and_is_kept_there(self):
        self.assertIn("function navFocusables()", SCRIPT)
        self.assertIn("ev.key !== 'Tab'", SCRIPT)
        self.assertIn("btn.focus()", SCRIPT)

    def test_a_skip_link_leads_past_the_menu(self):
        body = HEADER.split("<body", 1)[1]
        self.assertLess(body.index('class="skip-link" href="#maincontent"'), body.index('<aside id="sidebar">'))
        self.assertIn('<main id="maincontent" tabindex="-1">', HEADER)
        self.assertIn(".skip-link:focus", STYLE)


class DropdownClippingTests(unittest.TestCase):
    def test_scroll_boxes_stop_clipping_while_a_dropdown_is_open(self):
        self.assertIn("attributeFilter: ['open']", SCRIPT)
        self.assertIn("'.hh-tablewrap, .cbi-section, .cbi-map, .panel, .hh-dash-col > .cbi-section > div'", SCRIPT)
        self.assertIn(".hh-unclip { overflow: visible !important; }", STYLE)
        # it has to come after, and outrank, the phone rules that make cards scroll
        self.assertIn("overflow-x: auto", phone_rules())


def tokens(block_start):
    block = STYLE.split(block_start, 1)[1].split("\n}", 1)[0]
    return dict(re.findall(r"--([a-z0-9-]+):\s*(#[0-9a-fA-F]{6})\b", block))


def contrast(a, b):
    def lum(h):
        rgb = [int(h[i:i + 2], 16) / 255 for i in (1, 3, 5)]
        rgb = [c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb]
        return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]
    hi, lo = sorted((lum(a), lum(b)), reverse=True)
    return (hi + 0.05) / (lo + 0.05)


class ContrastTests(unittest.TestCase):
    """WCAG AA: 4.5:1 for text, 3:1 for the edge of a control and for signal bars."""

    PAIRS = (
        ("text-2", "surface"), ("muted", "surface"), ("muted", "surface-2"),
        ("muted", "bg"), ("faint", "surface"), ("faint", "bg"), ("faint", "surface-2"),
        ("accent", "surface"), ("accent", "accent-soft"), ("accent-ink", "accent"),
        ("ok", "ok-soft"), ("warn", "warn-soft"), ("err", "err-soft"),
    )

    def check(self, palette, name):
        for fg, bg in self.PAIRS:
            if fg in palette and bg in palette:
                ratio = contrast(palette[fg], palette[bg])
                self.assertGreaterEqual(ratio, 4.5, "%s: --%s on --%s is %.2f" % (name, fg, bg, ratio))
        self.assertGreaterEqual(contrast(palette["control-border"], palette["surface"]), 3.0)

    def test_light_theme(self):
        light = tokens(":root {")
        self.check(light, "light")
        self.assertGreaterEqual(contrast(light["sig-mid"], light["surface"]), 3.0)

    def test_dark_theme(self):
        light = tokens(":root {")
        dark = dict(light, **tokens(':root[data-theme="dark"] {'))
        self.check(dark, "dark")

    def test_form_controls_use_the_control_border(self):
        self.assertIn("border: 1px solid var(--control-border);", STYLE)
        self.assertIn("@media (forced-colors: active)", STYLE)
        focus = STYLE.split("input:focus, textarea:focus, select:focus {", 1)[1].split("}", 1)[0]
        self.assertIn("outline: 2px solid transparent", focus)


class DashboardLayoutTests(unittest.TestCase):
    def test_cards_are_dealt_into_fixed_stacks_not_css_columns(self):
        self.assertNotIn("columns: 2;", STYLE)
        self.assertIn("function dashboardColumns()", SCRIPT)
        self.assertIn("cols[i % 2].appendChild(c)", SCRIPT)
        self.assertIn(".hh-dash-col { display: contents; }", STYLE)
        self.assertIn("c.style.order = String(i)", SCRIPT)


class PhoneLayoutTests(unittest.TestCase):
    def test_a_new_message_stays_visible_in_the_phone_top_bar(self):
        phone = phone_rules()
        hide = phone.index(".mstrip > .mi:not(.mi-sig):not(.mi-net):not(.warnpill)")
        show = phone.index(".mstrip > a.mi.mi-sms.has-unread:not(.warnpill) { display: inline-flex; }")
        self.assertLess(hide, show)
        self.assertIn("'mi mi-sms' + (unread ? ' has-unread' : '')", SCRIPT)

    def test_touch_targets(self):
        phone = phone_rules()
        self.assertIn(".sb-nav .sb-head { min-height: 44px; }", phone)
        self.assertIn(".msg-acts .cbi-button, .at-bar .cbi-button { min-height: 36px; }", phone)
        copy = STYLE.split(".copy-btn {", 1)[1].split("}", 1)[0]
        self.assertIn("min-width: 24px", copy)
        self.assertIn("min-height: 24px", copy)
        self.assertIn("@media (hover: none)", STYLE)

    def test_heights_follow_the_visible_viewport(self):
        self.assertNotIn("calc(100vh", STYLE)
        self.assertIn("calc(var(--vh, 1vh) * 100", STYLE)
        self.assertIn("root.style.setProperty('--vh'", SCRIPT)


class TranslationTests(unittest.TestCase):
    def test_visible_words_go_through_the_translation_function(self):
        self.assertIn("var _ = (typeof window._ === 'function') ? window._", SCRIPT)
        for literal in ("'Copy'", "'Radio disabled'", "'not registered'", "'roaming'",
                        "'Band'", "'SMS'", "'Messages'", "'Modem'", "'Notifications'"):
            self.assertIn("_(" + literal + ")", SCRIPT, literal)
            self.assertNotIn(", " + literal + ")", SCRIPT.replace("_(" + literal + ")", ""), literal)


class CacheBustingTests(unittest.TestCase):
    def test_the_global_resource_version_covers_every_view(self):
        appver = HEADER.split("local function appver()", 1)[1].split("\n\tend\n", 1)[0]
        self.assertIn('scan("/www" .. resource .. "/view", 3)', appver)
        self.assertIn('nfs.stat("/usr/lib/opkg/status")', appver)
        self.assertNotIn('"/www" .. resource .. "/view/hh71vm" }', appver)


class PolishTests(unittest.TestCase):
    def test_the_fixed_top_bar_does_not_cover_scroll_targets(self):
        self.assertIn("scroll-padding-top: calc(var(--topbar-h) + 12px);", STYLE)

    def test_the_login_page_keeps_the_theme_switch(self):
        self.assertNotIn("body.no-nav #sidebar, body.no-nav #topbar { display: none; }", STYLE)
        self.assertIn("body.no-nav #topbar > :not(.tb-tools)", STYLE)

    def test_the_bar_explains_itself(self):
        self.assertIn("_('Modem not reachable')", SCRIPT)
        self.assertIn("_('Not updated: the modem status could not be read')", SCRIPT)
        self.assertIn('<meta name="theme-color"', HEADER)
        self.assertIn("<title><%=pcdata(striptags(", HEADER)


if __name__ == "__main__":
    unittest.main()
