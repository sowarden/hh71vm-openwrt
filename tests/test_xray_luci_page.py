"""Host regressions for the Xray LuCI page and the VPN-mode backend.

Three things are checked here, and each one has already caught a real defect:

* **the URI parser**, against a table of link shapes real panels emit. It rejected the
  owner's own REALITY link at first, because that link carries no port and 443 is
  implied;
* **the page itself**, rendered against a fake DOM. Nothing in the firmware build ever
  executes this JavaScript, so a typo in a dialog reaches the device and shows up as a
  blank page with an error in a console nobody has open;
* **the wiring** - that every method the page calls exists in the rpcd plugin and in the
  ACL, that both configuration files select the packages, and that the two settings the
  owner asked for by name are actually persisted.

The JavaScript checks need node and skip without it; the shell checks need a POSIX
shell and skip on Windows.
"""
import json
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
XRAY = ROOT / "openwrt-feed/package/net/xray-core"
APP = ROOT / "openwrt-feed/package/luci/applications/luci-app-hh71vm-xray"
VIEW = APP / "htdocs/luci-static/resources/view/hh71vm-xray/main.js"
MODULE = APP / "htdocs/luci-static/resources/hh71vm/xray.js"
RPCD = (XRAY / "files/rpcd-hh71vm-xray").read_text(encoding="utf-8")
WRITABLE_BLOCK = RPCD.split("local WRITABLE = {", 1)[1].split("\n}", 1)[0]
ACL = json.loads((APP / "root/usr/share/rpcd/acl.d/luci-app-hh71vm-xray.json")
                 .read_text(encoding="utf-8"))
LIB = (XRAY / "files/xray-lib.lua").read_text(encoding="utf-8")
FW = (XRAY / "files/hh71vm-xray-fw").read_text(encoding="utf-8")
INIT = (XRAY / "files/xray.init").read_text(encoding="utf-8")
SETTINGS = (XRAY / "files/xray.config").read_text(encoding="utf-8")
MAKEFILE = (XRAY / "Makefile").read_text(encoding="utf-8")
LOCK = json.loads((ROOT / "autobuild/lock.json").read_text(encoding="utf-8"))
CONFIG = (ROOT / "openwrt-feed/build.config").read_text(encoding="utf-8").splitlines()

NODE = shutil.which("node")


def run_node(harness, args, stdin=""):
    result = subprocess.run([NODE, str(ROOT / "tests/data" / harness)] + args,
                            input=stdin, text=True, encoding="utf-8",
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise AssertionError(harness + " failed:\n" + result.stdout)
    return json.loads(result.stdout)


@unittest.skipUnless(NODE, "requires node")
class UriParserTests(unittest.TestCase):
    """The parser has to read what other people's panels write, not what we would."""

    def parse(self, *uris):
        out = run_node("xray-uri-harness.js", [str(MODULE)],
                       json.dumps({"uris": list(uris)}))
        return out["results"]

    def test_a_reality_link_with_no_port_defaults_to_443(self):
        # This is the shape a panel actually emits, and rejecting it was the first
        # thing this parser got wrong.
        r = self.parse("vless://ef36fdf7-d237-4b9d-8367-082043780d6a@example.net"
                       "?type=tcp&encryption=none&security=reality"
                       "&pbk=WFTU5E8XP13sXcAJOKaswke4DRdbKTa7OjivUbcVXnI&fp=chrome"
                       "&sni=www.oracle.com&sid=1f64667a72&spx=%2F"
                       "&flow=xtls-rprx-vision#name")[0]
        self.assertTrue(r["ok"], r.get("error"))
        p = r["profile"]
        self.assertEqual(p["port"], 443)
        self.assertEqual(p["tls"], "reality")
        self.assertEqual(p["flow"], "xtls-rprx-vision")
        self.assertEqual(p["publicKey"], "WFTU5E8XP13sXcAJOKaswke4DRdbKTa7OjivUbcVXnI")
        self.assertEqual(p["sni"], "www.oracle.com")

    def test_sip002_shadowsocks_with_percent_encoded_base64(self):
        # The padding arrives as %3D often enough to matter, and the older form has
        # the whole userinfo@host:port inside one base64 blob.
        sip002, legacy = self.parse(
            "ss://YWVzLTI1Ni1nY206cGFzc3dvcmQ%3D@ss.example.com:8388#a",
            "ss://YWVzLTI1Ni1nY206cGFzc3dvcmRAc3MuZXhhbXBsZS5jb206ODM4OA#b")
        for r in (sip002, legacy):
            self.assertTrue(r["ok"], r.get("error"))
            self.assertEqual(r["profile"]["method"], "aes-256-gcm")
            self.assertEqual(r["profile"]["password"], "password")
            self.assertEqual(r["profile"]["port"], 8388)

    def test_vmess_base64_json(self):
        blob = ("eyJ2IjoiMiIsInBzIjoibXktdm1lc3MiLCJhZGQiOiJ2bWVzcy5leGFtcGxlLmNvbSIsInBvcnQi"
                "OiI0NDMiLCJpZCI6ImNiNTM2MzFkLTAzMjMtNGZlOS05NDQ4LWIwZWM1MzRjNDNjMSIsImFpZCI6"
                "IjAiLCJzY3kiOiJhdXRvIiwibmV0Ijoid3MiLCJ0eXBlIjoibm9uZSIsImhvc3QiOiJ2bWVzcy5l"
                "eGFtcGxlLmNvbSIsInBhdGgiOiIvcGF0aCIsInRscyI6InRscyJ9")
        p = self.parse("vmess://" + blob)[0]["profile"]
        self.assertEqual(p["protocol"], "vmess")
        self.assertEqual(p["transport"], "ws")
        self.assertEqual(p["path"], "/path")
        self.assertEqual(p["tls"], "tls")

    def test_trojan_and_grpc_and_ws(self):
        trojan, grpc = self.parse(
            "trojan://s3cr3t%40pass@t.example.com:8443?security=tls&type=tcp&sni=t.example.com#t",
            "vless://11111111-2222-3333-4444-555555555555@example.com:443"
            "?type=grpc&security=tls&serviceName=GunService&mode=multi#g")
        self.assertEqual(trojan["profile"]["password"], "s3cr3t@pass")
        self.assertEqual(grpc["profile"]["serviceName"], "GunService")
        self.assertEqual(grpc["profile"]["grpcMode"], "multi")

    def test_it_refuses_what_it_cannot_carry(self):
        reality_no_key, other_protocol, plugin, junk = self.parse(
            "vless://id@host.example.com:443?type=tcp&security=reality&sni=x.com#a",
            "hysteria2://x@y:443#b",
            "ss://YWVzLTI1Ni1nY206cGFzc3dvcmQ=@s.example.com:8388?plugin=obfs-local%3Bobfs%3Dhttp#c",
            "not a link")
        for r in (reality_no_key, other_protocol, plugin, junk):
            self.assertFalse(r["ok"])
        # and it says which thing is wrong, not "invalid link"
        self.assertIn("public key", reality_no_key["error"])
        self.assertIn("HYSTERIA2", other_protocol["error"])
        self.assertIn("plugin", plugin["error"])

    def test_a_profile_survives_the_round_trip(self):
        uri = ("vless://11111111-2222-3333-4444-555555555555@example.com:443"
               "?type=ws&security=tls&path=%2Fray&host=cdn.example.com&sni=cdn.example.com#n")
        first = self.parse(uri)[0]
        second = self.parse(first["uri"])[0]
        self.assertTrue(second["ok"], second.get("error"))
        for field in ("protocol", "address", "port", "uuid", "transport", "tls",
                      "path", "host", "sni"):
            self.assertEqual(first["profile"][field], second["profile"][field], field)


@unittest.skipUnless(NODE, "requires node")
class PageRenderTests(unittest.TestCase):
    """The page is never executed by the build, so it is executed here."""

    @classmethod
    def setUpClass(cls):
        cls.out = run_node("xray-view-harness.js", [str(VIEW), str(MODULE)])

    def test_it_renders_without_throwing(self):
        self.assertGreater(self.out["elements"], 100)
        self.assertGreater(self.out["text_length"], 500)

    def test_the_two_switches_the_owner_asked_for_are_on_the_page(self):
        self.assertTrue(self.out["has_autostart_switch"])
        self.assertTrue(self.out["has_reconnect_switch"])

    def test_the_connect_button_and_the_profiles_are_there(self):
        self.assertTrue(self.out["has_connect_button"])
        self.assertTrue(self.out["mentions_profiles"])

    def test_every_dialog_opens(self):
        # A dialog that throws is invisible until someone clicks it on the device.
        for label, result in self.out["dialogs"]:
            self.assertEqual(result, "opened", label + ": " + result)
        self.assertIn("Settings", self.out["modal_titles"])
        self.assertIn("Add from a link", self.out["modal_titles"])

    def test_it_says_that_udp_is_not_captured(self):
        # The page must not imply that everything goes through the tunnel when UDP
        # deliberately does not.
        self.assertTrue(self.out["warns_about_udp"])

    def test_it_shows_which_interfaces_are_captured(self):
        # The owner read "br-lan eth2" in a text box and could not tell whether Wi-Fi
        # was included. The page now answers that without being asked.
        self.assertTrue(self.out["shows_capture_set"])
        self.assertTrue(self.out["shows_uplink"])
        self.assertTrue(self.out["settings_offers_automatic_capture"])

    def test_the_api_section_shows_real_commands(self):
        self.assertTrue(self.out["api_examples"])


class PageUxContractTests(unittest.TestCase):
    """Keep the responsive dialog and status affordances from silently regressing."""

    @classmethod
    def setUpClass(cls):
        cls.view = VIEW.read_text(encoding="utf-8")

    def test_status_badge_does_not_stretch_with_its_column(self):
        self.assertIn(".xray-hero-state>.label{align-self:flex-start}", self.view)

    def test_form_dialogs_are_wide_scrollable_and_easy_to_close(self):
        for contract in (
                "max-width:1040px",
                "height:calc(100vh - 96px)",
                "grid-template-rows:auto minmax(0,1fr) auto",
                "height:100%;max-height:100%;min-height:0",
                "overflow-y:auto",
                "xray-modal-close",
                "ev.target === overlay",
                "ev.key === 'Escape'"):
            self.assertIn(contract, self.view)


class RpcWiringTests(unittest.TestCase):
    """Every method the page calls must exist in the plugin and in the ACL."""

    def methods_in_plugin(self):
        block = RPCD.split("local METHODS = {", 1)[1].split("\n}", 1)[0]
        return set(re.findall(r"^\t(\w+)\s*=", block, re.M))

    def methods_in_acl(self):
        acl = ACL["luci-app-hh71vm-xray"]
        return (set(acl["read"]["ubus"]["hh71vm-xray"]) |
                set(acl["write"]["ubus"]["hh71vm-xray"]))

    def test_the_plugin_and_the_acl_agree(self):
        self.assertEqual(self.methods_in_plugin(), self.methods_in_acl())

    def test_every_declared_method_has_a_handler(self):
        handlers = set(re.findall(r"^function handlers\.(\w+)", RPCD, re.M))
        self.assertEqual(handlers, self.methods_in_plugin())

    def test_no_method_has_an_empty_signature(self):
        # rpcd silently throws away any method whose signature is not an object, which
        # is every method that takes no arguments. Each one therefore takes one.
        block = RPCD.split("local METHODS = {", 1)[1].split("\n}", 1)[0]
        for name, sig in re.findall(r"(\w+)\s*=\s*(\{[^}]*\})", block):
            self.assertNotEqual(sig.strip(), "{}", name + " has an empty signature")

    def test_the_page_calls_only_methods_that_exist(self):
        module = MODULE.read_text(encoding="utf-8")
        called = set(re.findall(r"decl\('(\w+)'", module))
        called |= set(re.findall(r"callLong\('(\w+)'", module))
        self.assertTrue(called)
        self.assertTrue(called <= self.methods_in_plugin(),
                        "not in the plugin: %s" % (called - self.methods_in_plugin()))

    def test_settings_the_page_writes_are_all_writable(self):
        # settings_set refuses anything not in its own table, so a setting the page
        # offers but the backend rejects would fail only when someone changes it.
        writable = set(re.findall(r"^\t(\w+) = \"\^", RPCD, re.M))
        view = VIEW.read_text(encoding="utf-8")
        block = (view.split("function settingsDialog() {", 1)[1]
                     .split("function collect() {", 1)[1]
                     .split("\t\t\t}", 1)[0])
        offered = set(re.findall(r"^\t+(\w+):", block, re.M))
        self.assertTrue(offered <= writable, "not writable: %s" % (offered - writable))

    def test_settings_validation_uses_lua_patterns_and_explicit_enums(self):
        # Lua patterns do not implement regular-expression alternation. Keep the
        # character filter separate from the exact allowlist for enum settings.
        self.assertIn('mode = "^[a-z]+$"', RPCD)
        self.assertIn('loglevel = "^[a-z]+$"', RPCD)
        self.assertIn('lan_ifaces = "^[%w%.%-_ ]*$"', RPCD)
        self.assertIn("local ENUMS = {", RPCD)
        self.assertIn("(allowed and not allowed[sv])", RPCD)


class BackendContractTests(unittest.TestCase):
    def test_both_new_packages_are_selected_in_both_files(self):
        # lock.json is written over build.config, so one alone silently does nothing.
        for key in ("CONFIG_PACKAGE_luci-app-hh71vm-xray", "CONFIG_PACKAGE_kmod-ipt-tproxy",
                    "CONFIG_PACKAGE_iptables-mod-tproxy"):
            self.assertEqual(LOCK["config"].get(key), "m", key + " missing from lock.json")
            self.assertIn(key + "=m", CONFIG, key + " missing from build.config")

    def test_the_page_is_not_built_into_the_image(self):
        # The rootfs partition is full: 3.0 MiB of 3.0 MiB. It goes in the overlay.
        self.assertNotEqual(LOCK["config"].get("CONFIG_PACKAGE_luci-app-hh71vm-xray"), "y")

    def test_autostart_and_connected_are_separate_settings(self):
        # "connect now" and "connect at every boot" are two different questions, and
        # the page offers them as two switches. boot() is the only reader of autostart.
        self.assertIn("option autostart '0'", SETTINGS)
        self.assertIn("option enabled '0'", SETTINGS)
        boot = INIT.split("boot() {", 1)[1].split("\n}", 1)[0]
        self.assertIn("autostart", boot)
        start = INIT.split("start_service() {", 1)[1].split("\n}", 1)[0]
        self.assertNotIn("autostart", start)

    def test_the_watchdog_does_not_restart_its_own_service(self):
        # It runs as a second instance of the xray service; `/etc/init.d/xray restart`
        # would kill it in the middle of reconnecting. It kills the process instead and
        # lets procd bring it back.
        wd = (XRAY / "files/hh71vm-xray-watchdog").read_text(encoding="utf-8")
        # the file opens with a --[[ ]] block that names the very command it avoids
        code = wd.split("]]", 1)[1]
        self.assertNotIn("init.d/xray restart", code)
        self.assertIn("nixio.kill", code)

    def test_xray_never_proxies_its_own_connection(self):
        # Two independent guards, because one loop takes the router off the network.
        self.assertIn("sockopt", LIB)
        self.assertIn("mark = mark", LIB)
        rules = "\n".join(l for l in FW.splitlines() if not l.lstrip().startswith("#"))
        self.assertIn('--mark "$FWMARK" -j RETURN', rules)
        self.assertIn('-d "$SERVER_IP" -j RETURN', rules)

    def test_private_destinations_bypass_the_tunnel_in_both_places(self):
        # If these two lists drift apart, the router loses either its LAN or its
        # management address, and only on some path.
        lua = set(re.findall(r'"(\d+\.\d+\.\d+\.\d+/\d+)"', LIB))
        shell = set(re.findall(r"(\d+\.\d+\.\d+\.\d+/\d+)", FW))
        self.assertTrue(lua, "no private list in the library")
        self.assertEqual(lua - {"0.0.0.0/0"}, shell - {"0.0.0.0/0"})

    def test_dns_is_not_redirected_straight_into_xray(self):
        # Measured on the device: a UDP query redirected to Xray's inbound arrives and
        # is answered, and the answer never gets back - Go's dual-stack wildcard socket
        # replies from an address conntrack did not record. dnsmasq is in the path for
        # exactly that reason, and putting the Xray port back here would break DNS in a
        # way that looks like a dead server.
        rules = "\n".join(l for l in FW.splitlines() if not l.lstrip().startswith("#"))
        for line in rules.splitlines():
            if "dport 53" in line and "REDIRECT" in line:
                self.assertIn("--to-ports 53", line, line)

    def test_the_rules_are_rebuilt_when_the_uplink_moves(self):
        # "everything except the way out" has to be recomputed when the way out moves -
        # the modem coming back, or a cable making the combined port the WAN.
        hook = (XRAY / "files/xray.hotplug").read_text(encoding="utf-8")
        self.assertIn("ifup", hook)
        self.assertIn("hh71vm-xray-fw restore", hook)
        self.assertIn("hotplug.d/iface/95-xray", MAKEFILE)

    def test_the_profile_store_survives_a_sysupgrade(self):
        # The package lives in the overlay, which sysupgrade erases. The servers the
        # owner typed in must not go with it.
        self.assertIn("/etc/xray/profiles.json", MAKEFILE)
        keep = (XRAY / "files/keep").read_text(encoding="utf-8")
        self.assertIn("/etc/xray/", keep)
        conffiles = MAKEFILE.split("define Package/xray/conffiles", 1)[1].split("endef", 1)[0]
        self.assertIn("/etc/xray/profiles.json", conffiles)

    def test_the_client_interfaces_are_worked_out_not_typed(self):
        # The owner could not tell from "br-lan eth2" whether Wi-Fi was included. Empty
        # means "every interface except the way out", which is what makes this work the
        # same on the SIM and on a cable in the combined port.
        self.assertIn("option lan_ifaces ''", SETTINGS)
        self.assertIn("lan_ifaces       = \"\"", LIB)
        rules = "\n".join(l for l in FW.splitlines() if not l.lstrip().startswith("#"))
        self.assertIn("uplink_ifaces", rules)
        self.assertIn("auto_ifaces", rules)
        # busybox prints the whole table for `ip route show default`, so the filtering
        # has to be in awk or every interface looks like an uplink.
        self.assertNotIn("ip route show default", rules)
        self.assertIn('$1 == "default"', rules)

    def test_a_bridge_with_wifi_is_captured_even_when_it_is_the_uplink(self):
        # "everything except the way out" is not quite the rule. On a bench where the
        # internet arrives over the same bridge the clients are on, excluding the uplink
        # excluded the Wi-Fi clients too and captured the modem side instead. A bridge
        # carrying a wireless port is client-facing by definition; a plain interface
        # never is.
        rules = "\n".join(l for l in FW.splitlines() if not l.lstrip().startswith("#"))
        self.assertIn("has_wireless_port", rules)
        self.assertIn("is_bridge", rules)
        block = rules.split("auto_ifaces() {", 1)[1].split("\n}", 1)[0]
        self.assertIn('is_bridge "$ifn" || continue', block)
        self.assertIn('has_wireless_port "$ifn"', block)

    def test_capture_mode_is_not_set_inside_a_subshell(self):
        # `LAN_IFACES=$(resolve_ifaces ...)` ran the function in a subshell and the mode
        # never came back. It sets both variables in place instead.
        rules = "\n".join(l for l in FW.splitlines() if not l.lstrip().startswith("#"))
        self.assertNotIn("$(resolve_ifaces", rules)

    def test_teardown_does_not_depend_on_current_interface_settings(self):
        # A mode change can replace the automatically resolved interface list with an
        # empty value before teardown. Find installed jumps by target so old VPN rules
        # cannot survive after the user switches to Proxy mode.
        rules = "\n".join(l for l in FW.splitlines() if not l.lstrip().startswith("#"))
        block = rules.split("unlink_all() {", 1)[1].split("\n}", 1)[0]
        self.assertIn("delete_jumps nat PREROUTING $PRE_NAT", block)
        self.assertIn("delete_jumps filter FORWARD $FWD_FILT", block)
        self.assertNotIn("for ifn in $LAN_IFACES", block)

        controller = (XRAY / "files/hh71vm-xrayctl").read_text(encoding="utf-8")
        proxy_branch = controller.split('if s.mode == "vpn" then', 1)[1].split(
            '-- 7. does it actually carry traffic', 1)[0]
        self.assertIn('sh("/usr/sbin/hh71vm-xray-fw down")', proxy_branch)

    def test_udp_capture_is_off_by_default(self):
        # Measured: Xray captures the packet, tunnels it, and never writes the answer
        # back. Traffic that disappears is worse than traffic that goes out unproxied.
        self.assertIn("option capture_udp '0'", SETTINGS)
        self.assertIn('capture_udp      = "0"', LIB)
        rules = "\n".join(l for l in FW.splitlines() if not l.lstrip().startswith("#"))
        self.assertIn('[ "$CAPTURE_UDP" = "1" ] && have_tproxy', rules)

    def test_the_error_table_covers_a_server_that_just_closes(self):
        # That is what a rejected user looks like from the client side, and it had no
        # entry: the connect showed thirty lines of padding trace instead.
        self.assertIn("closed pipe", LIB)
        self.assertIn("unexpected EOF", LIB)
        self.assertIn("significant_log", LIB)

    def test_the_connect_job_detaches_its_standard_descriptors(self):
        # io.popen waits for every writer to close the pipe, so a forked child that
        # keeps stdout open makes the RPC last as long as the whole job. The browser
        # allows an RPC 20 seconds and reported "XHR request timed out" while the
        # connect was working fine underneath. Measured before the fix: 54 s. After: 0.
        ctl = (XRAY / "files/hh71vm-xrayctl").read_text(encoding="utf-8")
        fork = ctl.split("nixio.fork()", 1)[1].split("do_connect", 1)[0]
        # the comment explaining the fix names the call it replaced
        fork = "\n".join(l for l in fork.splitlines() if not l.lstrip().startswith("--"))
        self.assertIn("nixio.dup", fork)
        self.assertIn("/dev/null", fork)
        self.assertNotIn("io.stdout:close()", fork)

    def test_an_empty_log_explains_nothing_rather_than_something_wrong(self):
        # At `warning` level a dial that never answers writes no line at all, so the
        # last "significant" line was `core: Xray started` and the page presented that
        # as the reason the connection failed.
        self.assertIn('if #picked == 0 then return "" end', LIB)
        self.assertIn("returned nothing", LIB)

    def test_the_api_is_off_until_it_is_switched_on(self):
        api = (XRAY / "files/xray-api.cgi").read_text(encoding="utf-8")
        self.assertIn("option api_enabled '0'", SETTINGS)
        self.assertIn("api_enabled", api)
        self.assertIn("bad token", api)


class MemoryBoundingConfigTests(unittest.TestCase):
    """RAM-exhaustion fix: policy/keepalive/log/metrics additions to build_config().

    No Lua interpreter is assumed to be on the host running these tests (none of the
    existing checks against xray-lib.lua execute it either - they all read the source
    the way this class does), so these check the generator's actual code for the
    specific literal shapes the fix depends on, rather than a paraphrase of them.
    """

    def test_policy_uses_a_string_level_key(self):
        # Xray unmarshals `levels` as map[uint32]*Policy from a JSON object. A bare
        # numeric key would not survive as an object key through luci.jsonc at all.
        self.assertIn('policy = { levels = { ["0"] = level } }', LIB)

    def test_policy_defaults_match_the_documented_15_and_180(self):
        self.assertIn('policy_handshake    = "15"', LIB)
        self.assertIn('policy_conn_idle    = "180"', LIB)
        self.assertIn('if policy_hs == nil then policy_hs = 15 end', LIB)
        self.assertIn('if policy_ci == nil then policy_ci = 180 end', LIB)

    def test_a_zero_policy_value_omits_that_field_not_the_whole_block(self):
        block = LIB.split("local level = {}", 1)[1].split("local cfg = {", 1)[0]
        self.assertIn("if policy_hs ~= 0 then level.handshake = policy_hs end", block)
        self.assertIn("if policy_ci ~= 0 then level.connIdle = policy_ci end", block)

    def test_policy_never_sets_buffer_or_direction_only_fields(self):
        # This board already gets an arch-aware default for these from Xray itself;
        # overriding it here would make memory use worse, not better.
        block = LIB.split("local level = {}", 1)[1].split("local cfg = {", 1)[0]
        for forbidden in ("bufferSize", "uplinkOnly", "downlinkOnly"):
            self.assertNotIn(forbidden, block)

    def test_access_log_is_silenced(self):
        log_block = LIB.split("local cfg = {", 1)[1].split("inbounds = {}", 1)[0]
        self.assertIn('access = "none"', log_block)

    def test_proxy_sockopt_carries_keepalive_and_can_omit_each_field(self):
        sock = LIB.split("local sock = { mark = mark }", 1)[1].split("st.sockopt = sock", 1)[0]
        self.assertIn("sock.tcpKeepAliveIdle = keepalive.idle", sock)
        self.assertIn("sock.tcpKeepAliveInterval = keepalive.interval", sock)
        self.assertIn("sock.tcpUserTimeout = keepalive.user_timeout", sock)
        self.assertIn('keepalive_idle      = "90"', LIB)
        self.assertIn('keepalive_interval  = "15"', LIB)
        self.assertIn('tcp_user_timeout    = "0"', LIB)
        build = LIB.split("function M.build_config", 1)[1].split("local out, oerr", 1)[0]
        self.assertIn("if ka_idle ~= 0 then keepalive.idle = ka_idle end", build)
        self.assertIn("if ka_interval ~= 0 then keepalive.interval = ka_interval end", build)
        self.assertIn("if ka_timeout ~= 0 then keepalive.user_timeout = ka_timeout end", build)

    def test_metrics_endpoint_is_off_by_default_and_gated_on_a_non_empty_listen(self):
        self.assertIn('metrics_listen      = ""', LIB)
        self.assertIn('if trim(s.metrics_listen or "") ~= "" then', LIB)
        self.assertIn('cfg.metrics = { tag = "metrics", listen = s.metrics_listen }', LIB)

    def test_the_watchdog_runs_by_default_so_the_rss_guard_actually_runs(self):
        self.assertIn("option watchdog '1'", SETTINGS)

    def test_the_new_uci_options_ship_with_the_documented_defaults(self):
        for line in ("option policy_handshake '15'", "option policy_conn_idle '180'",
                     "option keepalive_idle '90'", "option keepalive_interval '15'",
                     "option tcp_user_timeout '0'", "option gogc '50'",
                     "option gomemlimit_mb '80'", "option mem_guard_mb '96'",
                     "option mem_guard_fails '3'", "option logfile_max_kb '256'",
                     "option metrics_listen ''"):
            self.assertIn(line, SETTINGS)

    def test_the_new_options_are_all_validated_by_the_rpcd_plugin(self):
        for key in ("policy_handshake", "policy_conn_idle", "keepalive_idle",
                    "keepalive_interval", "tcp_user_timeout", "gogc", "gomemlimit_mb",
                    "mem_guard_mb", "mem_guard_fails", "logfile_max_kb", "metrics_listen"):
            self.assertIn(key + " = ", RPCD)

    def test_metrics_listen_is_validated_without_pattern_alternation(self):
        # Lua patterns have no |; a prior bug (fixed in commit 4a75f94) came from using
        # one anyway, so the loopback:port check has to be an explicit match, not part
        # of the WRITABLE character-filter pattern.
        self.assertNotIn("|", WRITABLE_BLOCK)
        self.assertIn('metrics_listen:match("^127%.%d+%.%d+%.%d+:%d+$")', RPCD)

    def test_the_process_is_bounded_by_procd_limits_and_go_env_vars(self):
        self.assertIn('procd_set_param limits nofile="1024 4096"', INIT)
        self.assertIn("GOGC=$gogc", INIT)
        self.assertIn("GOMEMLIMIT=${gomemlimit_mb}MiB", INIT)

    def test_the_error_log_is_truncated_on_start_and_bounded_by_the_watchdog(self):
        self.assertIn("/var/log/xray.log", INIT)
        wd = (XRAY / "files/hh71vm-xray-watchdog").read_text(encoding="utf-8")
        self.assertIn("check_logfile", wd)
        self.assertIn("truncate_file", wd)

    def test_the_rss_guard_reuses_the_existing_recycle_call_and_state_file(self):
        wd = (XRAY / "files/hh71vm-xray-watchdog").read_text(encoding="utf-8")
        self.assertIn("mem_guard_mb", wd)
        self.assertIn("mem_guard_fails", wd)
        guard = wd.split("local recycled = false", 1)[1].split("if recycled then", 1)[0]
        self.assertIn("nixio.kill(pid, 15)", guard)
        self.assertIn("write_state(", guard)
        # the fields already used by the reconnect path must survive a memory-guard
        # write, and vice versa - write_state merges rather than overwrites
        self.assertIn("for k, v in pairs(patch) do cur[k] = v end", wd)

    def test_settings_are_only_re_read_when_the_uci_file_actually_changes(self):
        wd = (XRAY / "files/hh71vm-xray-watchdog").read_text(encoding="utf-8")
        self.assertIn('nixio.fs.stat(UCI_FILE, "mtime")', wd)

    def test_reality_dependency_is_pinned_in_the_compile_recipe(self):
        compile_recipe = MAKEFILE.split("define Build/Compile", 1)[1].split("endef", 1)[0]
        self.assertIn("github.com/xtls/reality@v0.0.0-20260827183302-8530a57042be",
                      compile_recipe)
        self.assertIn("go list -m github.com/xtls/reality", compile_recipe)


class StatusPollEfficiencyTests(unittest.TestCase):
    """Cause C: a 10s status poll forking ~50 processes per tick."""

    @classmethod
    def setUpClass(cls):
        cls.view = VIEW.read_text(encoding="utf-8")
        cls.ctl = (XRAY / "files/hh71vm-xrayctl").read_text(encoding="utf-8")

    def test_the_steady_poll_is_30s_and_skips_a_hidden_tab(self):
        poll_block = self.view.split("poll.add(function () {", 1)[1].split("}, 30);", 1)
        self.assertEqual(len(poll_block), 2, "poll.add(...) is not set to a 30s interval")
        self.assertIn("document.hidden", poll_block[0])

    def test_the_status_reply_uses_a_boolean_for_the_firewall_field_not_raw_text(self):
        # st.rules (a boolean) is what the page actually reads; the previous code also
        # attached the full multi-line `hh71vm-xray-fw status` output to every poll
        # reply as r.firewall, which nothing on the page consumed.
        self.assertNotIn("r.firewall = fw", self.ctl)
        self.assertIn("r.rules =", self.ctl)

    def test_fw_status_still_returns_the_full_text_on_demand(self):
        # The on-demand "Show the firewall rules" button still needs it.
        self.assertIn("function handlers.fw_status()", RPCD)
        handler = RPCD.split("function handlers.fw_status()", 1)[1].split("end", 1)[0]
        self.assertIn("hh71vm-xray-fw status", handler)

    def test_one_firewall_script_run_replaces_the_previous_two(self):
        self.assertIn("hh71vm-xray-fw brief", self.ctl)
        self.assertNotIn('sh("/usr/sbin/hh71vm-xray-fw ifaces', self.ctl)
        brief = FW.split("do_brief()", 1)[1].split("\n}", 1)[0]
        self.assertIn("ifaces=$LAN_IFACES", brief)

    def test_version_string_is_cached_instead_of_exec_d_every_poll(self):
        self.assertIn("cached_version", self.ctl)
        self.assertIn("nixio.fs.stat(bin", self.ctl)

    def test_listening_ports_are_checked_from_one_proc_net_snapshot(self):
        self.assertIn("function M.listening_many(ports)", LIB)
        self.assertIn("X.listening_many(", self.ctl)


@unittest.skipUnless(os.name == "posix" and shutil.which("sh"), "requires a POSIX shell")
class ShellSyntaxTests(unittest.TestCase):
    def test_the_shell_pieces_parse(self):
        for name in ("files/hh71vm-xray-fw", "files/xray.init", "files/xray.defaults",
                     "files/firewall.include", "files/hh71vm-xray"):
            result = subprocess.run(["sh", "-n", str(XRAY / name)], text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(result.returncode, 0, name + ": " + result.stdout)


if __name__ == "__main__":
    unittest.main()
