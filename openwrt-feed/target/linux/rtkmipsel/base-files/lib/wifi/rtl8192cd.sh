# The radio definition for `wifi config` is to generate /etc/config/wireless if it does not exist.
# Standard mechanism: /sbin/wifi sources /lib/wifi/*.sh, collects names from $DRIVERS and calls
# detect_<driver>. mac80211 and vendor broadcom-wl are made in the same way.
# Trigger - /etc/hotplug.d/net/00-rtl8192cd-wifi-detect (modeled after broadcom-wl).
#
# The function is IDEMPOTENT: a radio already described in /etc/config/wireless is skipped and the numbering
# radioN continues from the first available number. This is important because hotplug is jerking
# `wifi config` for each interface that appears.

append DRIVERS "rtl8192cd"

# Radio drivers are fixed netdev wlan0, wlan1 (VAP interfaces wlanX-vaN and
# WDS interfaces wlanX-wdsN are not included here, they are configured together with their radio).
rtl8192cd_radios() {
	local dir
	for dir in /proc/wlan[0-9]; do
		[ -d "$dir" ] || continue
		echo "${dir##*/}"
	done
}

# The band is determined by the driver itself: in mib_rf the 5-gigahertz radio has fields
# 5 GHz calibrations (pwrlevel5GHT40_*, pwrdiff5GOFDM), 2,4-gigahertz do not have them.
# [UNTESTED for other instances] 5 GHz on this board is the RTL8812FE card, that is 11ac,
# therefore, the default for it is VHT80. If one day you come across 5 GHz without 11ac, you will need it here
# separate check.
rtl8192cd_is_5g() {
	grep -q 5GHT40 "/proc/$1/mib_rf" 2>/dev/null
}

# A free number is searched AGAIN before each entry, and not once and then
# incrementally. The difference is visible when there is a hole in the numbering: with the existing radio0 and radio2
# and free radio1 the previous code stopped at the first free number (1),
# wrote the first radio there, then simply increased the counter - and the second radio
# overwrote the live section radio2 along with its SSID and key.
#
# $seen_idx - numbers already occupied in this pass: they are not yet in the config on disk,
# because config_load is done once before the loop.
rtl8192cd_idx_taken() {
	local idx="$1" cfgtype

	config_get cfgtype "radio$idx" type
	[ -n "$cfgtype" ] && return 0
	case " $seen_idx " in
		*" $idx "*) return 0 ;;
	esac
	return 1
}

rtl8192cd_next_free_idx() {
	local idx=0

	while rtl8192cd_idx_taken "$idx"; do
		idx=$((idx + 1))
	done
	echo "$idx"
}

detect_rtl8192cd() {
	local devidx
	local seen_idx=""
	local dev type known ifname channel hwmode htmode ssid

	config_load wireless

	for dev in $(rtl8192cd_radios); do
		known=0
		config_foreach rtl8192cd_check_device wifi-device "$dev"
		[ "$known" -gt 0 ] && continue

		devidx="$(rtl8192cd_next_free_idx)"

		if rtl8192cd_is_5g "$dev"; then
			channel=36          # lower UNII-1, outside DFS: broadcast without radar pause
			hwmode=11a
			htmode=VHT80
			ssid=HH71VM-5G
		else
			channel=6
			hwmode=11g
			htmode=HT20
			ssid=HH71VM
		fi

		# ⚠️ The default password is the same for all copies and is in the image. This is conscious
		# compromise of the test image: a completely open point on the device that distributes
		# Internet from SIM card is worse. Changes through LuCI/UCI (wireless.default_radioN.key).
		# The right decision for the future is to take the factory key from the config section in the flash
		# (format parsed into docs/mib-config-format.md).
		uci -q batch <<-EOF
			set wireless.radio${devidx}=wifi-device
			set wireless.radio${devidx}.type=rtl8192cd
			set wireless.radio${devidx}.ifname=${dev}
			set wireless.radio${devidx}.channel=${channel}
			set wireless.radio${devidx}.hwmode=${hwmode}
			set wireless.radio${devidx}.htmode=${htmode}
			set wireless.radio${devidx}.disabled=0

			set wireless.default_radio${devidx}=wifi-iface
			set wireless.default_radio${devidx}.device=radio${devidx}
			set wireless.default_radio${devidx}.network=lan
			set wireless.default_radio${devidx}.mode=ap
			set wireless.default_radio${devidx}.ssid=${ssid}
			set wireless.default_radio${devidx}.encryption=psk2+ccmp
			set wireless.default_radio${devidx}.key=hh71vm12345
		EOF
		uci -q commit wireless

		seen_idx="$seen_idx $devidx"
	done
}

rtl8192cd_check_device() {
	local cfg="$1" dev="$2"
	local cfgtype cfgif

	config_get cfgtype "$cfg" type
	[ "$cfgtype" = rtl8192cd ] || return 0

	config_get cfgif "$cfg" ifname
	# The section without ifname is assigned to a radio named: radio0 -> wlan0.
	[ -n "$cfgif" ] || cfgif="wlan${cfg#radio}"

	[ "$cfgif" = "$dev" ] && known=1
}
