#
# Target OpenWrt for Realtek RTL8197F (HH71VM, Realtek-side).
#
# The skeleton is taken from the form in which the target is described in the fork vladisslav2011/openwrt-AC10
# (OpenWrt 18.06 / core 3.18) and reduced to our database: OpenWrt 19.07 / core 4.14.
#
# Differences from the AC10 version:
#   KERNEL_PATCHVER 3.18 -> 4.14 is our base (see ../../../README.md, “Solutions”)
#   FEATURES += ramdisk — feature ramdisk includes TARGET_ROOTFS_INITRAMFS
#                                    default (config/Config-images.in). Needed
#                                    for RAM-boot: one assembly also provides an image for
#                                    RAM, and a permanent squashfs image for flash boot.
#
# This is free software, licensed under the GNU General Public License v2.
#
include $(TOPDIR)/rules.mk

ARCH:=mipsel
BOARD:=rtkmipsel
BOARDNAME:=Realtek mipsel SOC
SUBTARGETS:=rtl8197f
FEATURES:=ramdisk squashfs usb
KERNEL_PATCHVER:=4.14

include $(INCLUDE_DIR)/target.mk

define Target/Description
	Build firmware images for Realtek RTL8197F based boards
endef

$(eval $(call BuildTarget))
