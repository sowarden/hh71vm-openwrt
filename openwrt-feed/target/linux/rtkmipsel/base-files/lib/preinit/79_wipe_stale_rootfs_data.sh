#!/bin/sh
#
# The first boot after installing the vendor fwupg gets a section rootfs_data,
# filled with the remnants of the stock rootfs: fwupg writes only kernel and rootfs and
# doesn't erase anything. jffs2 does not accept such a section, mount_root goes to
# tmpfs, settings do not survive reboot, and the scan floods the console
# messages “Magic bitmask 0x1985 not found” for two minutes.
#
# HOW SOLVED. Before mount_root, the partition is checked and erased if necessary.
#
#   1. At the beginning of the section lies the magic jffs2 (0x1985, on the wire there are bytes 85 19) -
#      This is our overlay, don’t touch it.
#   2. Otherwise, the section is either empty (0xFF everywhere) or contains someone else’s data. To
#      distinguish, it is read entirely up to the first byte other than 0xFF.
#
# Why are the first two bytes missing (this was the case before 2026-08-24): `mtd erase`
# erases from the beginning of the section forward, so erasing interrupted by turning off
# power supply, leaves 0xFF at the beginning and other people's data further. Check by two
# bytes, such a section was taken as empty, and the next download received exactly
# that two-minute piece “Magic bitmask” for which the hook was written.
# To the user it looks like a freeze after the firmware, he turns off the power
# in the midst of erasure - and the state reproduces itself.
#
# Erasing 6 MiB takes about a minute (1536 sectors by 4 KiB, ~45 ms per
# sector is the limit of the chip itself, not the program). Previously, the console was for all this
# time fell silent. Now it is told in advance how long to wait, and every five seconds
# a mark is printed: silence per minute is the only reason why
# food is pulled at the most inopportune moment.
#
# In initramfs mode the hook is skipped: there mount_root is not called at all, and
# You cannot touch the flash when running the image from RAM.
#

wipe_stale_rootfs_data() {
	local index magic dirty pid waited

	index=$(find_mtd_index rootfs_data)
	[ -n "$index" ] || return 0
	[ -c "/dev/mtd$index" ] || return 0

	magic=$(dd if="/dev/mtd$index" bs=2 count=1 2>/dev/null | \
		hexdump -v -n 2 -e '2/1 "%02x"')
	[ "$magic" = "8519" ] && return 0

	# head -c 1 closes the channel on the first byte, so dd gets SIGPIPE
	# and the reading is interrupted: for the garbage section this is a fraction of a second, complete
	# the passage only happens when it is truly empty. For the same reason, tr is muted:
	# he manages to write to the console write error: Broken pipe before he dies, and
	# This is the only error line in the entire boot of the standard system (visible 2026-08-24).
	dirty=$(dd if="/dev/mtd$index" bs=64k 2>/dev/null | \
		tr -d '\377' 2>/dev/null | head -c 1 | wc -c)
	[ "$dirty" = "0" ] && return 0

	echo "rootfs_data: partition still holds other data, erasing it now."
	echo "rootfs_data: this takes about a minute. DO NOT switch the power off."

	mtd erase rootfs_data &
	pid=$!
	waited=0
	while kill -0 "$pid" 2>/dev/null; do
		sleep 5
		waited=$((waited + 5))
		echo "rootfs_data: erasing, ${waited}s elapsed, still working"
	done
	if wait "$pid"; then
		echo "rootfs_data: erase finished, continuing the boot"
	else
		echo "rootfs_data: erase FAILED; the overlay will fall back to tmpfs"
	fi
}

[ "$INITRAMFS" = "1" ] || boot_hook_add preinit_main wipe_stale_rootfs_data
