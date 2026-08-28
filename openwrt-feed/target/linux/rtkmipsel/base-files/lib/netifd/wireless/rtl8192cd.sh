#!/bin/sh
# netifd driver for the vendor rtl8192cd Wi-Fi driver (RTL8197F SoC 2.4 GHz + RTL8812FE 5 GHz).
#
# WHY THIS FILE EXISTS
#
# rtl8192cd is not a cfg80211 driver: the whole nl80211 path is behind the RTK_NL80211 macro,
# which is only defined in a build with backports (8192cd_cfg.h:920). It is configured with
# private ioctls over wext, that is `iwpriv <if> set_mib <field>=<value>` (the field table is
# in 8192cd_ioctl.c, privtab[]). So the stock /lib/netifd/wireless/mac80211.sh cannot drive it
# and a handler of our own is needed - that is the official extension point of netifd, and
# mac80211.sh lives in the same place. netifd reads the option schema by calling `<this file> <name> dump`.
#
# What netifd gives us instead of the old init script:
#   - the configuration lives in /etc/config/wireless (UCI), and therefore in LuCI;
#   - `wifi up` / `wifi down` / `wifi reload`;
#   - the interface joins its network from `option network` (the br-lan bridge) by itself. A
#     separate `brctl addif` is NO longer needed: netifd does it in wireless_interface_handle_link() ->
#     interface_handle_link() (netifd/wireless.c). That used to be a manual hack in the init script.
#
# TRAPS CONFIRMED ON THE HARDWARE
#
# 1. `wpa2_cipher`/`wpa_cipher` is a BIT MASK over the suite number from the RSN element, not a
#    cipher number: 8192cd_psk.c:1654 checks BIT(suite_type - 1). CCMP is suite 4, so BIT(3) = 8;
#    TKIP is suite 2, so BIT(1) = 2. The neighbouring field `encmode` is an ENUMERATION instead
#    (wifi.h:649): TKIP = 2, CCMP = 4. Writing wpa2_cipher=4 gives the client
#    «Can't connect».
# 2. Take the 11n/11ac flag from `htmode`, NOT from `enable_ht`. netifd only sets `enable_ht`
#    for the old-style `hwmode 11ng/11na`; with a modern `hwmode 11g` plus `htmode HT20` it is
#    zero, and the radio comes up as plain b/g (band=3 instead of 11).
# 3. netifd reads the /lib/netifd/wireless directory ONCE at start (wireless_init() ->
#    netifd_open_subdir()). If the file is not there when netifd starts, `wifi` and
#    `ubus call network.wireless status` stay silent until the network is restarted.
#
# hostapd is not needed: the driver has a built-in PSK (INCLUDE_WPA_PSK, 8192cd_psk.c).

. /lib/netifd/netifd-wireless.sh
. /lib/functions.sh

init_wireless_driver "$@"

drv_rtl8192cd_init_device_config() {
	# A radio is bound to a fixed netdev of the driver (wlan0, wlan1). With the option unset,
	# the name is derived from the section name: radio0 -> wlan0.
	config_add_string ifname
}

drv_rtl8192cd_init_iface_config() {
	config_add_boolean hidden
}

# htmode -> the 11n/11ac bits for `band`, the channel width and the position of the second channel.
# use40M:      0 = 20 MHz, 1 = 40 MHz, 2 = 80 MHz (enum channel_width,
#              cmn_info_file/rtw_sta_info.h:68).
# 2ndchoffset: 0 = do not care, 1 = below the primary, 2 = above (ieee802_mib.h:826).
# At VHT80 the driver works the position out itself - a written 0 reads back as 2.
rtl_parse_htmode() {
	local ht="$1"

	ht_bit=0
	vht_bit=0
	use40m=0
	choffset=0

	case "$ht" in
		VHT*) ht_bit=8; vht_bit=64 ;;
		HT*)  ht_bit=8 ;;
	esac
	case "$ht" in
		HT40+|VHT40+) use40m=1; choffset=2 ;;
		HT40-|VHT40-) use40m=1; choffset=1 ;;
		HT40|VHT40)   use40m=1 ;;
		VHT80)        use40m=2 ;;
		VHT160)       use40m=3 ;;
	esac
}

# The band mask is the `band` field (enum NETWORK_TYPE, core/core.h:345):
# 1|2|8 = 11b|11g|11n for 2.4 GHz, 4|8|64 = 11a|11n|11ac for 5 GHz.
rtl_band_mask() {
	case "$1" in
		a) echo $((4 + ht_bit + vht_bit)) ;;
		b) echo 1 ;;
		*) echo $((3 + ht_bit)) ;;
	esac
}

# The netdev name for the n-th wifi-iface section of this radio: the first one takes the radio
# itself, the rest take the VAP interfaces of the driver, wlanX-va0...va3 (created when the module loads).
# The factory MAC addresses live in the hwsetting partition (signature H601): the first address is at
# offset 7 and the step is 6. idx0 is eth0 (...:96:c1), idx1 is eth1 (...:96:c9), and behind them a pool
# of ...:96:c1...c7 and onwards.
#
# WHY. Without this BOTH radios come up with the address compiled into the driver,
# 00:e0:4c:81:86:86 - that is, with the same BSSID on 2.4 and on 5 GHz. Clients get
# confused by that: a phone sees two access points with one address and flaps between them.
# The radios take idx3 and idx4 so as to clash with neither eth0, nor eth1, nor each other;
# the virtual access points continue along the pool. [UNVERIFIED] which entries of the pool
# stock actually used - the entries are taken from the factory block of this very unit, so they
# belong to it in any case and are globally unique.
rtl_factory_mac() {
	local idx="$1" mtd off

	[ "$idx" -le 8 ] || return 1
	mtd=$(grep -m1 '"hwsetting"' /proc/mtd | cut -d: -f1)
	[ -n "$mtd" ] || return 1
	[ "$(dd if="/dev/$mtd" bs=1 count=4 2>/dev/null)" = "H601" ] || return 1

	off=$((7 + idx * 6))
	dd if="/dev/$mtd" bs=1 skip="$off" count=6 2>/dev/null |
		hexdump -v -e '5/1 "%02x:" 1/1 "%02x"'
}

# A fallback address for when there is no factory one left. The pool in hwsetting is finite
# (idx <= 8), and the formula below runs past it at the fourth VAP of a radio: rtl_factory_mac
# returns nothing, `ip link set address` is skipped - and ALL such interfaces stay on the
# address compiled into the driver, 00:e0:4c:81:86:86. That is exactly the identical-BSSID
# scenario that was already fixed here once.
#
# We take the base address of the radio itself and raise the locally administered bit
# (o1 | 0x02): that guarantees the resulting address cannot collide with any factory
# one - those are global and have that bit clear.
#
# The VAP number goes into the FIFTH octet (`o5 ^ nth*16`) rather than being added to
# the last one. The earlier version computed `last = base_last + nth`, and that was
# caught on the live board: the radio bases are neighbours (wlan0 = ..:96:c2, wlan1 = ..:96:c3),
# so with five access points on 2.4 GHz and four on 5 GHz it came out as
# wlan0-va3 = wlan1-va2 = 02:e0:4c:81:96:c7 - the very identical BSSID on two
# radios the function was written to prevent. Editing the last octet moves the
# collision rather than removing it: the last octet is the only thing the radio bases
# differ in. Changing a DIFFERENT octet and keeping the last one makes the mapping
# (base, VAP number) -> address one-to-one for any bases.
rtl_derived_mac() {
	local base="$1" nth="$2" o1 o2 o3 o4 o5 o6 rest

	[ -n "$base" ] || return 1
	# Exactly six octets. Checking after the split does not work: once the colons
	# have run out, both ${rest%%:*} and ${rest#*:} return the same tail, so for
	# "00:e0:4c" the last octet comes out non-empty and without a colon.
	case "$base" in
		*:*:*:*:*:*:*) return 1 ;;
		*:*:*:*:*:*)   ;;
		*)             return 1 ;;
	esac
	o1="${base%%:*}"; rest="${base#*:}"
	o2="${rest%%:*}"; rest="${rest#*:}"
	o3="${rest%%:*}"; rest="${rest#*:}"
	o4="${rest%%:*}"; rest="${rest#*:}"
	o5="${rest%%:*}"; o6="${rest#*:}"

	printf '%02x:%s:%s:%s:%02x:%s\n' \
		$(( 0x$o1 | 0x02 )) "$o2" "$o3" "$o4" \
		$(( 0x$o5 ^ ((nth % 16) * 16) )) "$o6"
}

rtl_vif_ifname() {
	local base="$1" idx="$2"

	if [ "$idx" = 0 ]; then
		echo "$base"
	else
		echo "$base-va$((idx - 1))"
	fi
}

rtl_unsupported_vif() {
	wireless_setup_vif_failed UNSUPPORTED_MODE
}

# A wrapper around iwpriv: EVERY refusal is visible and stops the configuration.
#
# Confirmed on the board: the iwpriv from wireless_tools returns 255 on every
# failure - a non-existent MIB field, a non-existent interface, an unknown
# private command, a missing value - and 0 on success. There is no need to parse
# stderr, the exit status is enough.
rtl_iwpriv() {
	local ifname="$1"
	shift

	iwpriv "$ifname" set_mib "$@" || {
		echo "rtl8192cd: $ifname: iwpriv set_mib $* failed" >&2
		return 1
	}
}

# Writes the whole set of MIB fields and stops at the first refusal. The values are
# passed as separate arguments, so spaces in an SSID do not fall apart.
rtl_apply_mib() {
	local ifname="$1"
	shift

	while [ $# -gt 0 ]; do
		rtl_iwpriv "$ifname" "$1" || return 1
		shift
	done
}

rtl_setup_vif() {
	local name="$1"
	local ifname
	local ssid key encryption hidden
	local wpa auth_type wpa_cipher
	local psk_enable=0 encmode=0 cipher_mask=0

	ifname="$(rtl_vif_ifname "$dev_ifname" "$vif_idx")"
	vif_idx=$((vif_idx + 1))

	json_select config
	json_get_vars ssid key hidden macaddr
	wireless_vif_parse_encryption
	json_select ..

	[ -d "/proc/$ifname" ] || {
		wireless_setup_vif_failed NO_SUCH_INTERFACE
		return 1
	}

	case "$wpa_cipher" in
	*CCMP*TKIP*|*TKIP*CCMP*) cipher_mask=10; encmode=4 ;;
	*CCMP*)                  cipher_mask=8;  encmode=4 ;;
	*TKIP*)                  cipher_mask=2;  encmode=2 ;;
	esac
	case "$auth_type" in
		psk)  psk_enable="$wpa" ;;   # 1 = WPA, 2 = WPA2, 3 = mixed (PSK_WPA|PSK_WPA2)
		none) psk_enable=0; encmode=0; cipher_mask=0 ;;
		*)
			# WEP, EAP, SAE/WPA3, OWE: the built-in PSK of the driver cannot do them, and
			# there is no hostapd in this build (that needs the cfg80211 path).
			wireless_setup_vif_failed UNSUPPORTED_ENCRYPTION
			return 1
		;;
	esac

	# The interface is brought down BEFORE configuring and left down on any failure below.
	# That is what fail-closed means here: a half-configured access point must not transmit
	# at all - otherwise the SSID goes on the air with the old key (or with no encryption),
	# and netifd reports the radio as up while it happens.
	ifconfig "$ifname" down || {
		echo "rtl8192cd: $ifname: ifconfig down failed" >&2
		wireless_setup_vif_failed IFDOWN_FAILED
		return 1
	}

	# The address is only assigned while the interface is down: the driver copies it into the
	# MIB and uses it as the BSSID afterwards. A user `option macaddr` wins over this.
	[ -n "$macaddr" ] || macaddr="$(rtl_factory_mac $((3 + ${dev_ifname#wlan} + (vif_idx - 1) * 2)))"
	[ -n "$macaddr" ] || macaddr="$(rtl_derived_mac "$(rtl_factory_mac $((3 + ${dev_ifname#wlan})))" "$vif_idx")"
	if [ -n "$macaddr" ]; then
		ip link set dev "$ifname" address "$macaddr" || {
			wireless_setup_vif_failed MACADDR_FAILED
			return 1
		}
	else
		# Neither a factory nor a derived one: hwsetting cannot be read at all. Silently
		# leaving the interface on the driver address is not acceptable - the two radios
		# would meet on one BSSID, and that would later have to be found from client symptoms.
		echo "rtl8192cd: $ifname has no MAC address of its own (hwsetting unreadable)" >&2
	fi

	# iwpriv failures used to go unwatched, and unconditional `ifconfig up` and
	# `wireless_add_vif()` followed. The interface came up with whatever part of the
	# settings had made it in: a new SSID with the old key, for instance. Now the first
	# failure leaves the interface down.
	rtl_apply_mib "$ifname" \
		"ssid=$ssid" \
		"band=$band" \
		"channel=$channel" \
		"use40M=$use40m" \
		"2ndchoffset=$choffset" \
		"opmode=16" \
		"hiddenAP=${hidden:-0}" \
		"authtype=0" \
		"psk_enable=$psk_enable" \
		"encmode=$encmode" || {
		wireless_setup_vif_failed MIB_WRITE_FAILED
		return 1
	}
	# opmode 0x10 = AP; authtype=0 is open authentication, which is what WPA2 wants.

	[ "$psk_enable" = 0 ] || {
		rtl_apply_mib "$ifname" \
			"wpa_cipher=$cipher_mask" \
			"wpa2_cipher=$cipher_mask" \
			"passphrase=$key" || {
			wireless_setup_vif_failed KEY_WRITE_FAILED
			return 1
		}
	}

	ifconfig "$ifname" up || {
		echo "rtl8192cd: $ifname: ifconfig up failed" >&2
		wireless_setup_vif_failed IFUP_FAILED
		return 1
	}

	wireless_add_vif "$name" "$ifname"
}

drv_rtl8192cd_setup() {
	local dev="$1"
	local dev_ifname raw_htmode band vif_idx=0
	local ht_bit vht_bit use40m choffset

	json_select config
	json_get_vars ifname
	json_get_var raw_htmode htmode
	json_select ..

	dev_ifname="${ifname:-wlan${dev#radio}}"

	[ -d "/proc/$dev_ifname" ] || {
		wireless_setup_failed NO_SUCH_INTERFACE
		return 1
	}

	rtl_parse_htmode "$raw_htmode"
	# $hwmode and $channel are filled in by _wdev_prepare_channel before this function runs.
	band="$(rtl_band_mask "$hwmode")"

	# The driver is the vendor one and can only do AP here. Everything else used to simply
	# never reach the loop below: a section with `mode 'sta'` vanished silently, the radio
	# still reported itself as up, and LuCI showed neither the interface nor a reason. Now
	# every such section gets its own UNSUPPORTED and is visible as failed.
	for_each_interface "sta adhoc mesh monitor wds" rtl_unsupported_vif

	for_each_interface "ap" rtl_setup_vif

	wireless_set_up

	# The Wi-Fi indicator lives on the Qualcomm side and knows nothing about us.
	# In the background: the update travels over the network to the other half, and netifd
	# must not wait for its answer to finish configuring the radio.
	/usr/sbin/hh71vm-wifi-led >/dev/null 2>&1 &
}

drv_rtl8192cd_teardown() {
	local dev="$1"
	local dev_ifname vif

	json_select config
	json_get_vars ifname
	json_select ..

	dev_ifname="${ifname:-wlan${dev#radio}}"

	# Bring down the radio itself and all of its VAP interfaces: which of them were in use is
	# no longer known at this point, and a spare `down` on an interface that is not up is harmless.
	for vif in "$dev_ifname" "$dev_ifname"-va0 "$dev_ifname"-va1 "$dev_ifname"-va2 "$dev_ifname"-va3; do
		[ -d "/proc/$vif" ] && ifconfig "$vif" down 2>/dev/null
	done

	# After the loop rather than before it: the second radio may still be up, and the
	# decision is made from the actual interface flags.
	/usr/sbin/hh71vm-wifi-led >/dev/null 2>&1 &
}

drv_rtl8192cd_cleanup() {
	return 0
}

add_driver rtl8192cd
