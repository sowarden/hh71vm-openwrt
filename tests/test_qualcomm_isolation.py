"""LAN clients must not reach the Qualcomm half's unauthenticated services."""

import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / "openwrt-feed/target/linux/rtkmipsel/base-files/etc/uci-defaults"
          / "99-hh71vm-qualcomm-isolation")

FAKE_UCI = """#!/bin/sh
echo "$*" >> "$UCI_LOG"
case "$*" in
	"-q show firewall") cat "$UCI_SHOW" ;;
	"-q add firewall rule") echo cfg0a92bd ;;
esac
exit 0
"""


@unittest.skipUnless(os.name == "posix" and shutil.which("sh"), "requires a POSIX shell")
class QualcommIsolation(unittest.TestCase):
    def run_script(self, existing):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = pathlib.Path(tmp)
            uci = tmp / "uci"
            uci.write_text(FAKE_UCI)
            uci.chmod(0o755)
            (tmp / "show").write_text(existing)
            env = dict(os.environ, PATH=str(tmp) + os.pathsep + os.environ["PATH"],
                       UCI_LOG=str(tmp / "log"), UCI_SHOW=str(tmp / "show"))
            subprocess.run(["sh", str(SCRIPT)], env=env, check=True)
            log = tmp / "log"
            return log.read_text().splitlines() if log.exists() else []

    def test_a_fresh_install_rejects_lan_to_the_modem(self):
        calls = self.run_script("firewall.@zone[0]=zone\n")
        self.assertIn("-q add firewall rule", calls)
        for expected in ("name=Block-LAN-to-Qualcomm", "src=lan", "dest=wan",
                         "dest_ip=192.168.225.1", "proto=all", "target=REJECT"):
            self.assertIn("-q set firewall.cfg0a92bd." + expected, calls)
        self.assertEqual(calls[-1], "-q commit firewall")

    def test_an_existing_rule_is_left_as_the_owner_set_it(self):
        calls = self.run_script(
            "firewall.cfg0a92bd=rule\n"
            "firewall.cfg0a92bd.name='Block-LAN-to-Qualcomm'\n"
            "firewall.cfg0a92bd.enabled='0'\n")
        self.assertEqual(calls, ["-q show firewall"])

    def test_the_router_itself_is_not_filtered(self):
        # A forward rule; the router's own AT channel and share mount use OUTPUT.
        text = SCRIPT.read_text()
        self.assertIn(".src=lan", text)
        self.assertNotIn(".src=wan", text)
        self.assertNotIn("target=DROP", text)

    def test_the_limitation_is_documented(self):
        known = (ROOT / "docs/known-issues.md").read_text()
        self.assertIn("Block-LAN-to-Qualcomm", known)


if __name__ == "__main__":
    unittest.main()
