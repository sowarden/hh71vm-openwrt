#
# sysupgrade for the Realtek RTL8197F (HH71VM).
#
# A *-sysupgrade.bin image is two parts back to back:
#   [0 .. KERNEL_PART_SIZE)      the kernel with its 16-byte cr6c header, padded with 0xFF;
#   [KERNEL_PART_SIZE .. end)    the squashfs root filesystem.
# The boundaries are set in target/linux/rtkmipsel/image/Makefile and in the kernel mtdparts;
# here they are repeated as numbers, because this script already runs on the device.
#
# The partitions are written separately: the boot partition (0x0-0x1FFFF) and hwsetting with
# the MAC addresses (0x20000-0x23FFF) are not touched at all - the only way to guarantee that
# a failed update does not destroy the recovery path through the bootloader
# console.
#
# The cr6c signature is the magic of the kernel image header of our device; the other
# signatures are left over from the Realtek family: the same bootloader accepts them.
#
# The squashfs part of the image already carries the length stamp and the checksum bytes that
# the bootloader checks before starting the kernel: mkrtkimg puts them in at build time
# (see image/Makefile, the stamp step). So the partition is written as it is here - there is
# no need to compute a 16-bit sum over the whole rootfs on the device itself.
#
# UPDATES AND THE OVERLAY. The rootfs_data partition (the overlay) is erased on every
# update, and the saved settings are put into it by a separate platform_copy_config step.
# It used not to be touched at all, and that produced a silent accumulation bug: overlayfs
# serves the upper layer, so any file that ever made it into the overlay - by a manual edit,
# by `opkg install` or from an earlier build - shadowed the file of the same name in the new
# firmware for ever. Settings "survived" an update not because we were saving them but
# because nobody was erasing the overlay; for the same reason `sysupgrade -n` did not reset
# the system to its factory state. Both behaviours are now real.
#
#

. /lib/functions/system.sh
. /lib/rtkmipsel.sh

KERNEL_PART_SIZE=2949120
ROOTFS_PART_SIZE=3145728
IMAGE_MAX_SIZE=$((KERNEL_PART_SIZE + ROOTFS_PART_SIZE))

# Without metadata the image is not accepted. `fwtool_check_image` (base-files/lib/upgrade/fwtool.sh)
# compares `supported_devices` from the tail of the image against /tmp/sysinfo/board_name; ours
# says `hh71vm`, and image/Makefile appends the metadata. The point of the flag is that WITHOUT
# it sysupgrade merely prints `Image metadata not found` and flashes whatever it was given:
# through the LuCI web form any unrelated image would have gone into the board.
#
# The other side of it: our older builds carry no metadata, and going back to one of them needs
# `sysupgrade -F`. That is by design - it is exactly the case -F exists for.
REQUIRE_IMAGE_METADATA=1

get_magic_str() {
	(get_image "$@" | dd bs=4 count=1) 2>/dev/null
}

platform_check_image() {
	local signature size

	[ "$#" -gt 1 ] && return 1

	signature=$(get_magic_str "$1")

	case "$signature" in
	cs6b|\
	cs6c|\
	csys|\
	cr6b|\
	cr6c|\
	csro)
		;;
	*)
		echo "Invalid image. Signature $signature not recognized."
		return 1
		;;
	esac

	size=$(get_image "$1" | wc -c)
	if [ "$size" -le "$KERNEL_PART_SIZE" ]; then
		echo "Invalid image: $size bytes, no rootfs behind the kernel partition."
		return 1
	fi

	# There was no upper bound here at all. An image longer than the two partitions was
	# not rejected but written: `mtd write` stops at the rootfs boundary, so the tail of
	# the squashfs simply never reached flash. The kernel was overwritten by then, so the
	# board went into a restart with a knowingly truncated root filesystem - refusing at
	# the check stage is incomparably cheaper.
	#
	# $size includes the fwtool metadata tail (a few hundred bytes), so the effective
	# ceiling is lower than the physical one by those bytes. The error is on the safe
	# side: a completely full image is rejected slightly early, but this check cannot
	# accept an image that would not fit.
	if [ "$size" -gt "$IMAGE_MAX_SIZE" ]; then
		echo "Invalid image: $size bytes, larger than kernel+rootfs ($IMAGE_MAX_SIZE bytes)."
		return 1
	fi

	return 0
}

# Erasing the overlay takes about a minute (1536 sectors of 4 KiB, roughly 45 ms per sector -
# the limit of the chip, not of the program). A silence that long in the middle of an update
# reads as a hang and makes people pull the power, so the same decision is taken here as in
# lib/preinit/79_wipe_stale_rootfs_data.sh: say in advance how long it will take, and print a
# mark every five seconds.
#
# An erase interrupted by a power cut is not dangerous: the next boot sees a partition with no
# jffs2 magic and foreign data in it, and the same preinit hook finishes the erase.
erase_rootfs_data() {
	local pid waited

	echo "rootfs_data: erasing the overlay so the new firmware is not shadowed."
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
		echo "rootfs_data: erase finished"
		return 0
	fi
	echo "rootfs_data: erase FAILED"
	return 1
}

platform_do_upgrade() {
	local image="$1"
	local kernel_mtd rootfs_mtd remount_err size jffs2_mnt

	# The size ceiling is checked in platform_check_image too, but `sysupgrade -F`
	# IGNORES the result of that check and goes on to flash (sysupgrade:324).
	# For the signature and the metadata that is by design - -F exists exactly for
	# rolling back to a foreign image - but an image longer than the two partitions
	# will not be written under any circumstances: `mtd write` would cut it off at the
	# rootfs boundary after the kernel has already been overwritten. Here a refusal is
	# still free: not a single write has been made.
	#
	# The restart cannot be cancelled from here: do_stage2 does not look at the exit
	# status and restarts the board anyway. But that is the safe outcome - the board
	# goes into a restart with UNTOUCHED firmware.
	size=$(get_image "$image" | wc -c)
	if [ "$size" -gt "$IMAGE_MAX_SIZE" ]; then
		echo "Upgrade refused: $size bytes, larger than kernel+rootfs ($IMAGE_MAX_SIZE bytes)."
		echo "Upgrade refused: -F cannot make it fit; the write would truncate the rootfs."
		echo "Upgrade refused: nothing was written, the current firmware is intact."
		return 1
	fi

	kernel_mtd=$(find_mtd_part kernel)
	rootfs_mtd=$(find_mtd_part rootfs)

	if [ -z "$kernel_mtd" ] || [ -z "$rootfs_mtd" ]; then
		echo "Upgrade failed: kernel/rootfs partitions not found."
		return 1
	fi

	# Every piece is written by its own mtd write: mtd erases exactly the partition it
	# writes to and never runs past its boundaries.
	#
	# The -r flag must not be here: it restarts the board immediately after the write,
	# and then neither the overlay erase nor platform_copy_config would ever run.
	# do_stage2 restarts it itself, on the line right after ours.
	get_image "$image" | dd bs=4096 count=$((KERNEL_PART_SIZE / 4096)) 2>/dev/null | \
		mtd write - kernel || return 1
	get_image "$image" | dd bs=4096 skip=$((KERNEL_PART_SIZE / 4096)) 2>/dev/null | \
		mtd write - rootfs || return 1

	# jffs2 sits in memory with its own node map and, on the umount -a at the end of
	# do_stage2, would write it back over the partition we have just erased. Switching
	# it to read-only closes that window before the erase.
	#
	# LOOK FOR THE FILESYSTEM ITSELF, NOT FOR THE PATH. The earlier version did
	# `mount -o remount,ro /overlay` and printed the failure; on the very first real
	# update the failure duly happened:
	#
	#   remount,ro /overlay failed: mount: can't find /overlay in /proc/mounts
	#
	# By the time it is called, sysupgrade has already done "Switching to ramdisk" and
	# there is no /overlay path in that root at all - which means the protection had
	# NEVER worked since the day it was written. So the mount point is looked up by
	# filesystem type instead. If no jffs2 is mounted in this namespace, that is a
	# normal outcome rather than a warning: there is nobody to write over the erased partition.
	sync
	jffs2_mnt=$(awk '$3 == "jffs2" { print $2; exit }' /proc/mounts)
	if [ -z "$jffs2_mnt" ]; then
		echo "rootfs_data: no jffs2 mount in this namespace, nothing to freeze."
	else
		remount_err=$(mount -o remount,ro "$jffs2_mnt" 2>&1)
		if [ $? -ne 0 ]; then
			echo "Upgrade warning: remount,ro $jffs2_mnt failed: ${remount_err:-no error text}"
			echo "Upgrade warning: the overlay may write its node map back after the erase."
		else
			echo "rootfs_data: $jffs2_mnt is read-only now, erasing is safe."
		fi
	fi

	erase_rootfs_data || return 1
}

# Called from do_stage2 only when sysupgrade was started without -n: procd then puts the
# settings archive into $UPGRADE_BACKUP. The file is written into the root of the fresh
# jffs2, from where the stock lib/preinit/80_mount_root picks it up on the first boot
# ("- config restore -"), and /etc/init.d/done removes it.
#
# With -n this is never entered and the partition stays erased - which is the honest reset
# to factory settings we did not have before.
platform_copy_config() {
	[ -f "$UPGRADE_BACKUP" ] || return 0
	mtd -d "" jffs2write "$UPGRADE_BACKUP" rootfs_data || {
		echo "Upgrade warning: could not store the saved configuration."
		return 1
	}
	sync
}
