"""Targeted host regressions for rtl8192cd LuCI and IMEI state handling."""
import json
import shutil
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LUCI_PATCH = ROOT / "openwrt-feed/patches/luci/100-rtl8192cd-encryption-capabilities.patch"


def added_javascript():
    lines = []
    for line in LUCI_PATCH.read_text(encoding="utf-8").splitlines():
        if line.startswith("+") and not line.startswith("+++"):
            lines.append(line[1:])
    return "\n".join(lines)


class Rtl8192cdLuciTests(unittest.TestCase):
    def test_clean_build_applies_the_pinned_luci_patch_fail_closed(self):
        build = (ROOT / "autobuild/build.py").read_text()
        self.assertIn('apply_feed_patches(source, build, "luci")', build)
        self.assertIn('"--fuzz=0"', build)
        self.assertIn('run("git", "diff", "--check", cwd=checkout)', build)

    @unittest.skipUnless(shutil.which("node"), "requires Node.js for the LuCI helper probe")
    def test_exact_modes_and_uci_round_trip(self):
        source = added_javascript()
        first = source.index("function normalizeEncryptionValue")
        last = source.index("\t] : null;\n}", first) + len("\t] : null;\n}")
        helpers = source[first:last]
        probe = helpers + r"""
function readEncryption(value) {
    value = String(value);
    return value.match(/\+/) ? value.replace(/\+.+$/, '') : value;
}
function saveNetwork(uci, ssid, key, encryption) {
    uci.ssid = ssid;
    uci.key = key;
    uci.encryption = normalizeEncryptionValue(uci.type, encryption, null, false);
    return uci;
}
var modes = rtl8192cdCryptoModes('rtl8192cd').map(function(item) { return item[0]; });
var existing = { type: 'rtl8192cd', ssid: 'Existing', key: 'preserved-passphrase', encryption: 'psk2+ccmp' };
var opened = JSON.stringify(existing);
readEncryption(existing.encryption);
var recovered = saveNetwork({ type: 'rtl8192cd', ssid: 'Broken', key: '1', encryption: 'wep-open' },
                            'Recovered', 'new-passphrase', 'psk2');
var result = {
    modes: modes,
    read: readEncryption('psk2+ccmp'),
    saved: normalizeEncryptionValue('rtl8192cd', 'psk2', null, false),
    reopened: readEncryption(normalizeEncryptionValue('rtl8192cd', 'psk2', null, false)),
    open: normalizeEncryptionValue('rtl8192cd', 'none', null, false),
    other: rtl8192cdCryptoModes('mac80211'),
    openingPreserved: opened == JSON.stringify(existing),
    recovered: recovered
};
process.stdout.write(JSON.stringify(result));
"""
        result = subprocess.run(
            [shutil.which("node"), "-e", "function _(s){return s;}\n" + probe],
            capture_output=True, text=True, check=True,
        )
        value = json.loads(result.stdout)
        self.assertEqual(value["modes"], ["psk2", "none"])
        self.assertEqual(value["read"], "psk2")
        self.assertEqual(value["saved"], "psk2+ccmp")
        self.assertEqual(value["reopened"], "psk2")
        self.assertEqual(value["open"], "none")
        self.assertIsNone(value["other"])
        self.assertTrue(value["openingPreserved"])
        self.assertEqual(value["recovered"], {
            "type": "rtl8192cd", "ssid": "Recovered", "key": "new-passphrase",
            "encryption": "psk2+ccmp",
        })

    def test_patch_hides_unsupported_choices_and_preserves_device_type(self):
        patch = LUCI_PATCH.read_text(encoding="utf-8")
        self.assertIn("if (hwtype != 'rtl8192cd')", patch)
        self.assertIn("['psk2', 'WPA2-PSK', 35]", patch)
        self.assertIn("['none',  _('No Encryption'), 0]", patch)
        self.assertNotIn("uci.set('wireless', section_id, 'type'", patch)
        self.assertNotIn("hostapd", added_javascript())
        self.assertIn("value == 'wpa2'", patch)
        self.assertNotIn("value == 'psk2')\n+\t\t\t\t\t\tuci.unset", patch)

    def test_generator_and_netifd_contract_match_luci(self):
        generator = (ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files/lib/wifi/rtl8192cd.sh").read_text()
        handler = (ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files/lib/netifd/wireless/rtl8192cd.sh").read_text()
        self.assertIn("set wireless.default_radio${devidx}.encryption=psk2+ccmp", generator)
        self.assertIn("psk_enable=0", handler)
        self.assertIn("cipher_mask=8", handler)
        self.assertIn("UNSUPPORTED_ENCRYPTION", handler)
        self.assertIn('case "$auth_type" in', handler)
        self.assertIn('psk)  psk_enable="$wpa"', handler)
        self.assertIn('none) psk_enable=0; encmode=0; cipher_mask=0', handler)


class ModemIdentityIntegrationTests(unittest.TestCase):
    def test_narrow_ati_refresh_is_wired_through_rpc_and_acl(self):
        daemon = (ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files/usr/sbin/hh71vm-modemd").read_text()
        rpcd = (ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files/usr/libexec/rpcd/hh71vm-modem").read_text()
        acl = (ROOT / "openwrt-feed/package/luci/applications/luci-app-hh71vm-modem/root/usr/share/rpcd/acl.d/luci-app-hh71vm-modem.json").read_text()
        backend = (ROOT / "openwrt-feed/package/utils/modem-extra-tools/files/common.lua").read_text()
        self.assertIn("function API.device_refresh", daemon)
        self.assertIn('{ cmd = "ATI", parse = function(lines) fresh = M.parsers.ati_lines(lines) end }', daemon)
        self.assertIn("device_refresh  = {}", rpcd)
        self.assertIn('"device_refresh"', acl)
        self.assertIn("ubus call hh71vm-modem device_refresh", backend)

    def test_ui_distinguishes_readback_cache_and_network_activation(self):
        view = (ROOT / "openwrt-feed/package/luci/applications/luci-app-modem-extra-tools/htdocs/luci-static/resources/view/modem-extra-tools/main-1-1-2.js").read_text()
        for marker in (
            "NV 550 readback completed",
            "Fully shut down the router",
            "normal OpenWrt reboot restarts the Realtek/OpenWrt side",
            "does not fully restart the separate Qualcomm modem subsystem",
            "cold power cycle",
            "main Modem overview cache could not be refreshed",
            "does not verify the identity accepted by the mobile network",
        ):
            self.assertIn(marker, view)
        self.assertNotIn("localStorage", view)
        self.assertNotIn("sessionStorage", view)

    def test_band_capability_mismatch_is_dynamic_and_explicit(self):
        view = (ROOT / "openwrt-feed/package/luci/applications/luci-app-modem-extra-tools/htdocs/luci-static/resources/view/modem-extra-tools/main-1-1-2.js").read_text()
        backend = (ROOT / "openwrt-feed/package/utils/modem-extra-tools/files/bands.lua").read_text()
        helper = (ROOT / "openwrt-feed/package/utils/modem-extra-tools/src/hh71-nas.c").read_text()
        for marker in (
            "selectable_bands",
            "unconfirmed_bands",
            "current only",
            "This explicitly removes current-only bands",
        ):
            self.assertIn(marker, view)
        self.assertIn("selectable_bands=B.list(union(mask,capability))", backend)
        self.assertIn("target LTE preference contains a band that is neither reported nor currently enabled", backend)
        self.assertIn('q.helper .. \' \' .. operation .. \' \' .. target .. \' \' .. expected', backend)
        self.assertIn('equal(argv[1],"apply")', helper)
        self.assertIn('equal(argv[1],"restore")', helper)
        self.assertIn("merge(available,cap,current)", helper)
        self.assertNotIn("B32/B38", backend + helper + view)

    def test_optional_packages_ship_the_same_backend_ui_version(self):
        backend = (ROOT / "openwrt-feed/package/utils/modem-extra-tools/Makefile").read_text()
        frontend = (ROOT / "openwrt-feed/package/luci/applications/luci-app-modem-extra-tools/Makefile").read_text()
        config = (ROOT / "openwrt-feed/build.config").read_text()
        self.assertIn("PKG_VERSION:=1.1.2", backend)
        self.assertIn("PKG_VERSION:=1.1.2", frontend)
        self.assertIn("CONFIG_PACKAGE_modem-extra-tools=m", config)
        self.assertIn("CONFIG_PACKAGE_luci-app-modem-extra-tools=m", config)


if __name__ == "__main__":
    unittest.main()
