#!/bin/sh
#
# Determination of the board using line machine in /proc/cpuinfo.
#
# The string is printed by arch/mips/kernel/proc.c from the name registered by the macro
# MIPS_MACHINE - for us it is "Alcatel LINKHUB HH71 series"
# (arch/mips/rtl8197f/mach-hh71vm.c).
#
# The name of the family, not a specific model: Realtek - half does not store the model anywhere,
# and the same image will become HH71 without VM. The Qualcomm side knows the exact line,
# and hh71vm-modemd clarifies /tmp/sysinfo/model as soon as she can ask her.
#
# ⚠️ THIS IS AN UPDATE CONTRACT, NOT JUST A NAME. With 2026-08-25, images carry metadata, and
# `fwtool_check_image` checks them `supported_devices` exactly with /tmp/sysinfo/board_name
# (base-files/lib/upgrade/fwtool.sh). The value is set in image/Makefile as
# SUPPORTED_DEVICES. It must ONLY be changed simultaneously in both places.
#
# Why family? The device has undocumented carrier and
# regional versions, and which one the user has - we don’t know. The line `machine` prints
# this kernel (the same in all copies of the firmware), so `board_name` is always
# `hh71vm` regardless of model, and the update works for everyone. If we break this name down into
# specific models, updates will break for everyone whose model is not guessed. TO FIRST
# this does not affect the installation at all: it goes through the bootloader and `fwupg`, where the metadata is not
# are being checked.
#
# Future trap: if the board variant ever registers `machine` without
# substrings `HH71`, `name` will become `unknown`, and `sysupgrade` will begin to fail. Then
# the correct answer is to expand `case` below, not the list of models in the metadata.
#

RTKMIPSEL_BOARD_NAME=
RTKMIPSEL_MODEL=

rtkmipsel_board_detect() {
	local machine
	local name

	machine=$(awk 'BEGIN{FS="[ \t]+:[ \t]"} /machine/ {print $2}' /proc/cpuinfo)

	case "$machine" in
	*"HH71"*)
		name="hh71vm"
		;;
	esac

	[ -z "$name" ] && name="unknown"

	[ -z "$RTKMIPSEL_BOARD_NAME" ] && RTKMIPSEL_BOARD_NAME="$name"
	[ -z "$RTKMIPSEL_MODEL" ] && RTKMIPSEL_MODEL="$machine"

	[ -e "/tmp/sysinfo/" ] || mkdir -p "/tmp/sysinfo/"

	echo "$RTKMIPSEL_BOARD_NAME" > /tmp/sysinfo/board_name
	echo "$RTKMIPSEL_MODEL" > /tmp/sysinfo/model
}
