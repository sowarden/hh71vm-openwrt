"""Host regressions for the vendor rtl8192cd netifd handler."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
HANDLER = ROOT / (
    "openwrt-feed/target/linux/rtkmipsel/base-files/lib/netifd/wireless/rtl8192cd.sh"
)
DRIVER_CONFIG = ROOT / (
    "openwrt-feed/target/linux/rtkmipsel/files/drivers/net/wireless/realtek/"
    "rtl8192cd/8192cd_cfg.h"
)
DRIVER_RX = ROOT / (
    "openwrt-feed/target/linux/rtkmipsel/files/drivers/net/wireless/realtek/"
    "rtl8192cd/8192cd_rx.c"
)


class Rtl8192cdHandlerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = HANDLER.read_text(encoding="utf-8")

    def test_ampdu_is_a_default_on_device_option_with_explicit_opt_out(self):
        self.assertRegex(self.source, r"(?m)^\s*config_add_boolean ampdu\s*$")
        self.assertNotIn("config_add_boolean ampdu 1", self.source)
        self.assertRegex(self.source, r"json_get_vars ifname ampdu\b")
        self.assertIn('HT*|VHT*) ampdu="${ampdu:-1}"; amsdu="${amsdu:-1}"', self.source)
        self.assertRegex(self.source, r"\n\s*\*\)\s+ampdu=0; amsdu=0 ;;")

    def test_ampdu_is_written_in_the_fail_closed_mib_batch(self):
        setup = re.search(
            r"rtl_setup_vif\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        self.assertIsNotNone(setup)
        body = setup.group("body")

        down = body.index('ifconfig "$ifname" down')
        apply_mib = body.index('rtl_apply_mib "$ifname"')
        ampdu = body.index('"ampdu=$ampdu"')
        mib_failure = body.index("wireless_setup_vif_failed MIB_WRITE_FAILED")
        up = body.index('ifconfig "$ifname" up')

        self.assertLess(down, apply_mib)
        self.assertLess(apply_mib, ampdu)
        self.assertLess(ampdu, mib_failure)
        self.assertLess(mib_failure, up)

    def test_txpwrlmt_is_a_default_on_device_option_with_explicit_opt_out(self):
        self.assertRegex(self.source, r"(?m)^\s*config_add_boolean txpwrlmt\s*$")
        self.assertRegex(self.source, r"json_get_vars ifname ampdu amsdu txpwrlmt edca_fairness shortgi80\b")
        # The UCI option is positive, the MIB is negative; the mapping has to invert.
        self.assertIn(
            '[ "${txpwrlmt:-1}" = 0 ] && disable_txpwrlmt=1 || disable_txpwrlmt=0',
            self.source,
        )

    def test_txpwrlmt_is_written_before_the_channel_in_the_mib_batch(self):
        setup = re.search(
            r"rtl_setup_vif\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        self.assertIsNotNone(setup)
        body = setup.group("body")

        apply_mib = body.index('rtl_apply_mib "$ifname"')
        txpwrlmt = body.index('"disable_txpwrlmt=$disable_txpwrlmt"')
        channel = body.index('"channel=$channel"')
        mib_failure = body.index("wireless_setup_vif_failed MIB_WRITE_FAILED")

        self.assertLess(apply_mib, txpwrlmt)
        # The driver works the per-channel limit table out while the channel is applied,
        # so a later write would not reach that table.
        self.assertLess(txpwrlmt, channel)
        self.assertLess(channel, mib_failure)

    def test_amsdu_is_a_default_on_device_option_with_explicit_opt_out(self):
        self.assertRegex(self.source, r"(?m)^\s*config_add_boolean amsdu\s*$")
        self.assertNotIn("config_add_boolean amsdu 1", self.source)
        self.assertRegex(self.source, r"json_get_vars ifname ampdu amsdu txpwrlmt edca_fairness shortgi80\b")
        # Both the UCI option and the MIB are positive, so this one maps straight through.
        self.assertIn('"mustAmsdu=$amsdu"', self.source)

    def test_amsdu_follows_the_same_aggregation_gate_as_ampdu(self):
        setup = re.search(
            r"drv_rtl8192cd_setup\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        self.assertIsNotNone(setup)
        body = setup.group("body")
        # A-MSDU only exists inside an A-MPDU, so a legacy-rate radio must get neither.
        self.assertIn('HT*|VHT*) ampdu="${ampdu:-1}"; amsdu="${amsdu:-1}" ;;', body)
        self.assertIn("*)        ampdu=0; amsdu=0 ;;", body)

    def test_amsdu_is_written_in_the_fail_closed_mib_batch(self):
        setup = re.search(
            r"rtl_setup_vif\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        self.assertIsNotNone(setup)
        body = setup.group("body")

        apply_mib = body.index('rtl_apply_mib "$ifname"')
        amsdu = body.index('"mustAmsdu=$amsdu"')
        mib_failure = body.index("wireless_setup_vif_failed MIB_WRITE_FAILED")
        up = body.index('ifconfig "$ifname" up')

        self.assertLess(apply_mib, amsdu)
        self.assertLess(amsdu, mib_failure)
        self.assertLess(mib_failure, up)

    def test_edca_fairness_is_a_device_option_defaulting_on_for_5ghz_only(self):
        self.assertRegex(self.source, r"(?m)^\s*config_add_boolean edca_fairness\s*$")
        setup = re.search(
            r"drv_rtl8192cd_setup\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        self.assertIsNotNone(setup)
        body = setup.group("body")
        # 5 GHz is where the airtime split was measured; a 3 ms TXOP is a much larger
        # slice of the medium at 2.4 GHz rates, so that band keeps the vendor behaviour.
        self.assertIn('a) edca_fairness="${edca_fairness:-1}" ;;', body)
        self.assertIn('*) edca_fairness="${edca_fairness:-0}" ;;', body)

    def test_edca_fairness_resolves_both_ways_to_explicit_values(self):
        setup = re.search(
            r"drv_rtl8192cd_setup\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        body = setup.group("body")
        self.assertIn("edca_manual=1; edca_sta_cwmin=6; edca_ap_txop=94", body)
        # Turning the option off must write the vendor values rather than leave whatever
        # the previous setup put in the MIB.
        self.assertIn("edca_manual=0; edca_sta_cwmin=4; edca_ap_txop=0", body)

    def test_edca_fields_are_written_in_the_fail_closed_mib_batch(self):
        setup = re.search(
            r"rtl_setup_vif\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        self.assertIsNotNone(setup)
        body = setup.group("body")

        apply_mib = body.index('rtl_apply_mib "$ifname"')
        mib_failure = body.index("wireless_setup_vif_failed MIB_WRITE_FAILED")
        for field in ('"manual_edca=$edca_manual"',
                      '"sta_beq_cwmin=$edca_sta_cwmin"',
                      '"ap_beq_txoplimit=$edca_ap_txop"'):
            at = body.index(field)
            self.assertLess(apply_mib, at, field)
            self.assertLess(at, mib_failure, field)
        # The two per-queue values are only read by the driver while manual mode is on,
        # so the switch has to be written with them, not in a later batch.
        self.assertLess(body.index('"manual_edca=$edca_manual"'),
                        body.index('"sta_beq_cwmin=$edca_sta_cwmin"'))

    def test_shortgi80_is_a_device_option_gated_on_the_only_width_it_applies_to(self):
        self.assertRegex(self.source, r"(?m)^\s*config_add_boolean shortgi80\s*$")
        setup = re.search(
            r"drv_rtl8192cd_setup\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        self.assertIsNotNone(setup)
        body = setup.group("body")
        # The MIB field governs 80 MHz only; at any narrower width it must stay off rather
        # than be written speculatively.
        self.assertIn('VHT80) shortgi80="${shortgi80:-1}" ;;', body)
        self.assertIn("*)     shortgi80=0 ;;", body)

    def test_shortgi80_is_written_in_the_fail_closed_mib_batch(self):
        setup = re.search(
            r"rtl_setup_vif\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        body = setup.group("body")
        apply_mib = body.index('rtl_apply_mib "$ifname"')
        at = body.index('"shortGI80M=$shortgi80"')
        mib_failure = body.index("wireless_setup_vif_failed MIB_WRITE_FAILED")
        self.assertLess(apply_mib, at)
        self.assertLess(at, mib_failure)

    def test_ampdu_is_not_silently_forced_on_for_legacy_modes(self):
        setup = re.search(
            r"drv_rtl8192cd_setup\(\) \{(?P<body>.*?)\n\}", self.source, re.DOTALL
        )
        self.assertIsNotNone(setup)
        body = setup.group("body")
        mode_gate = body.index('case "$raw_htmode" in')
        interface_loop = body.index('for_each_interface "ap" rtl_setup_vif')
        self.assertLess(mode_gate, interface_loop)
        self.assertIn("*)        ampdu=0; amsdu=0 ;;", body)


class Rtl8192cdDriverConfigTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = DRIVER_CONFIG.read_text(encoding="utf-8", errors="replace")

    def test_rtl8197f_module_build_keeps_bridge_shortcut(self):
        self.assertIn(
            "#if !defined(RTK_NL80211) && !defined(CONFIG_RTL_8197F)\n"
            "#undef BR_SHORTCUT\n"
            "#endif",
            self.source,
        )

    def test_optional_vlan_shortcut_counter_is_consistently_guarded(self):
        source = DRIVER_RX.read_text(encoding="utf-8", errors="replace")
        self.assertRegex(
            source,
            r"#if defined\(CONFIG_RTL_BRSHORTCUT_LINUX_VLAN_CTL\)\s+"
            r"statistic_brsc_wlan_xmit_to_eth\+\+;\s+#endif",
        )


if __name__ == "__main__":
    unittest.main()
