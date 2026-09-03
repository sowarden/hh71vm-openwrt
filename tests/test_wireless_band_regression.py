"""Host regressions for the 5 GHz band trap in the LuCI wireless page.

Selecting anything but AC used to clear `hwmode` and `channel`, after which the 5 GHz
radio came up on channel 1 and 802.11ac vanished from the mode list for good. Two
independent defects had to line up; both are covered here.
"""
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IWINFO_PATCHES = ROOT / "openwrt-feed/package/network/utils/iwinfo/patches"
LUCI_PATCH = ROOT / "openwrt-feed/patches/luci/101-wifi-band-fallback-rtl8192cd.patch"


class IwinfoRadioNameTests(unittest.TestCase):
    """iwinfo has to answer for a UCI radio section, not only for a netdev."""

    def test_the_wext_backend_resolves_radio_sections(self):
        patch = (IWINFO_PATCHES / "104-wext-uci-radio-names-rtl8192cd.patch").read_text(encoding="utf-8")
        self.assertIn("static const char * wext_uci_ifname(const char *name)", patch)
        self.assertIn('iwinfo_uci_get_radio(name, "rtl8192cd")', patch)
        self.assertIn('uci_lookup_option_string(uci_ctx, s, "ifname")', patch)
        self.assertIn("iwinfo_uci_free();", patch)
        # Every wext call goes through this one helper, so hooking it covers freqlist,
        # info, assoclist and the rest at once.
        self.assertIn("static inline int wext_ioctl(const char *ifname, int cmd, struct iwreq *wrq)", patch)
        self.assertIn("(resolved = wext_uci_ifname(ifname)) != NULL", patch)

    def test_the_resolution_is_scoped_to_this_driver(self):
        patch = (IWINFO_PATCHES / "104-wext-uci-radio-names-rtl8192cd.patch").read_text(encoding="utf-8")
        self.assertNotIn('iwinfo_uci_get_radio(name, "mac80211")', patch)
        self.assertIn('strncmp(name, "radio", 5)', patch)

    def test_the_patch_series_stays_ordered(self):
        names = sorted(p.name for p in IWINFO_PATCHES.glob("*.patch"))
        self.assertEqual(names[-1], "104-wext-uci-radio-names-rtl8192cd.patch")
        self.assertEqual(len(names), len({name[:3] for name in names}))


class WirelessFormFallbackTests(unittest.TestCase):
    """An empty select is missing information, not a request to clear the radio."""

    def test_empty_band_or_channel_is_not_written_through(self):
        patch = LUCI_PATCH.read_text(encoding="utf-8")
        self.assertIn("-\t\tuci.set('wireless', section_id, 'hwmode', value[1]);", patch)
        self.assertIn("-\t\tuci.set('wireless', section_id, 'channel', value[2]);", patch)
        self.assertIn("+\t\tif (value[1])", patch)
        self.assertIn("+\t\tif (value[2])", patch)

    def test_htmode_is_still_allowed_to_be_cleared(self):
        # Legacy really does mean "no htmode", so that one write must stay unconditional.
        patch = LUCI_PATCH.read_text(encoding="utf-8")
        self.assertIn("uci.set('wireless', section_id, 'htmode', value[0] || null);", patch)
        self.assertNotIn("+\t\tif (value[0])", patch)


if __name__ == "__main__":
    unittest.main()
