#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

cd "$(dirname "$0")"

[ "$(id -u)" = 0 ] || {
	echo 'Run this installer as root.' >&2
	exit 1
}

[ "$(cat /tmp/sysinfo/board_name 2>/dev/null)" = 'hh71vm' ] || {
	echo 'This package set requires the HH71VM OpenWrt port.' >&2
	exit 1
}

sha256sum -c SHA256SUMS
. ./bundle.env
installed_kernel=$(opkg status kernel | sed -n 's/^Version: //p')
[ "$installed_kernel" = "$expected_kernel" ] || {
	echo "Kernel mismatch: $installed_kernel" >&2
	echo "Expected: $expected_kernel" >&2
	echo 'Install the bundle built for your firmware; do not force dependencies.' >&2
	exit 1
}

opkg update

# This is an official generic MIPS userspace extension, not a foreign kmod.
upstream='https://downloads.openwrt.org/releases/19.07.10/targets/ramips/mt7620/packages'
filename='iptables-mod-ipopt_1.8.3-1_mipsel_24kc.ipk'
checksum='5c8f819ffc49d05a6ca7acd431a279fbcf73f6e5406f3ad821a2fb93d311325d'
temporary=$(mktemp -d /tmp/modem-extra-install.XXXXXX)

cleanup() {
	rm -f "$temporary/$filename" "$temporary/Packages" \
		"$temporary/Packages.gz" "$temporary/Packages.sig"
	rmdir "$temporary" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM

wget -q "$upstream/Packages.gz" -O "$temporary/Packages.gz"
wget -q "$upstream/Packages.sig" -O "$temporary/Packages.sig"
gzip -dc "$temporary/Packages.gz" > "$temporary/Packages"
usign -V -P /etc/opkg/keys -m "$temporary/Packages" -x "$temporary/Packages.sig"

signed_checksum=$(awk -v name="$filename" 'BEGIN {RS=""; FS="\n"}
  {found=0;sum="";for(i=1;i<=NF;i++) {
    if($i=="Filename: " name) found=1;
    if($i ~ /^SHA256sum: /) sum=substr($i,12);
  } if(found) print sum;}' "$temporary/Packages")
[ "$signed_checksum" = "$checksum" ] || {
	echo 'Unexpected checksum in the signed upstream package index.' >&2
	exit 1
}

wget -q "$upstream/$filename" -O "$temporary/$filename"
printf '%s  %s\n' "$checksum" "$temporary/$filename" | sha256sum -c -
opkg install "./$ipk_kmod_hh71vm_ipt_ipopt"
opkg install "$temporary/$filename"
opkg install "./$ipk_modem_extra_tools" "./$ipk_luci_app_modem_extra_tools"

echo 'Installation completed.'
echo 'Open LuCI in a new tab, then select Modem > Extra tools.'
echo 'A fresh install leaves TTL and band restrictions disabled; existing settings are kept.'
