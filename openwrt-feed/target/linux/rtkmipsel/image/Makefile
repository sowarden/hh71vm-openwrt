#
# Building images for the Realtek RTL8197F (Alcatel LINKHUB HH71VM).
#
# Based on the image/Makefile of the openwrt-AC10 fork, cut down to one device.
# What was removed and why:
#
#   * the sdcard image - the device has no card slot.
#   * MultiProfile/SingleProfile - there is only one device.
#   * cvimg-rtl8197f from AC10, which we do not have: the headers are built by mkrtkimg.py
#     from this same directory (it reproduces the stock kernel image byte for byte).
#
# Two images are built:
#
#   1. an initramfs for running from RAM. When the ROM bootloader receives a file over
#      TFTP whose name contains the substring 'nfjrom', it does not write it to flash but
#      places it at LOADADDR and hands control there. Hence the -nfjrom.bin suffix.
#
#
#   2. a permanent image: a kernel with a cr6c header in the kernel partition and a
#      squashfs in the rootfs partition. There are two output files:
#        *-sysupgrade.bin - the kernel (padded to the partition size) plus the squashfs,
#                           written in one piece starting at 0x30000; this is also what
#                           platform.sh accepts during a sysupgrade;
#        *-fwupg.bin      - the same contents, but as two sections, cr6c plus r6cr, for
#                           the vendor /bin/fwupg: the first-installation path from a
#                           running stock system, with no UART.
#
# The flash image is linked at 0x80A00000, the same address as the stock kernel, so that it
# depends on the normal boot path exactly as stock does. The RAM image stays at
# 0x84000000: its address is set by the LOADADDR command.
#
# This is free software, licensed under the GNU General Public License v2.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

# Where the ROM bootloader places the loader and where it starts it (the LOADADDR command).
LZMA_TEXT_START := 0x84000000
# The same for the image in flash: the address comes from the startAddr field of the header,
# and we repeat the stock value so as not to depend on untested alternatives.
FLASH_TEXT_START := 0x80A00000
# Where the loader unpacks the kernel to.
KERNEL_LOADADDR := 0x80000000

# The flash layout (16 MB). It matches the mtdparts in the kernel CONFIG_CMDLINE and the
# boundaries of the stock partitions, so the vendor fwupg puts our sections in the same place:
#   0x000000 boot 128k | 0x020000 hwsetting 16k | 0x024000 config 48k
#   0x030000 kernel 2880k | 0x300000 rootfs 3072k | 0x600000 rootfs_data 6144k
#   0xC00000 vendor_jffs2 4096k - the stock JFFS2, which we do not touch at all: otherwise
#   fstools would mount somebody else's filesystem as our overlay, and stock would lose its
#   settings and its way back. Our rootfs partition is smaller than the stock one (3072k against
#   9216k), so the r6cr section fwupg puts at the start of the stock rootfs lands exactly in ours.
#
# The start of rootfs has to stay at 0x300000, because of installing FROM STOCK. The vendor
# `fwd` finds its target by the partition LABEL in /proc/mtd (`rtl_name_to_mtdblock`) and
# writes the `r6cr` section from offset zero of the partition it found, ignoring the
# `burnAddr` in the header for that branch. On stock the `rootfs` label sits at 0x300000 -
# so moving our rootfs would miss the partition.
#
#
# This does NOT apply to the path through the bootloader: there `burn_image` calls
# `flash_write(0, burnAddr, src, len)` for any signature and the address comes from the
# header. So there is exactly one constraint, and it rests on installing from a running
# stock system.
#
# The consequence: the tail of the `kernel` partition (about 1.2 MiB) cannot be taken for the
# overlay - it lies BEFORE 0x300000, and freeing it would move rootfs. Only the tail of
# rootfs was taken: 4096k -> 3072k, with the difference given to rootfs_data (5120k -> 6144k),
# and vendor_jffs2 stayed at 0xC00000.
KERNEL_PART_OFFSET := 0x30000
KERNEL_PART_SIZE := 2949120
ROOTFS_PART_OFFSET := 0x300000
ROOTFS_PART_SIZE := 3145728

# Image metadata for sysupgrade. The string has to match what rtkmipsel_board_detect()
# (base-files/lib/rtkmipsel.sh) writes into /tmp/sysinfo/board_name:
# fwtool_check_image compares supported_devices against exactly that file. Without the
# metadata, sysupgrade prints `Image metadata not found` and flashes ANY image without
# checking which device it came from - through the LuCI web form that is a sure way to brick the board.
#
# THIS IS NOT A DEVICE MODEL BUT A FAMILY, and it must not be split up by model. HH71 has
# undocumented carrier and regional versions; their hardware is practically identical and the
# firmware has to install on all of them. It works like this: `board_name` is derived from the
# `machine` string reported by this kernel (MIPS_MACHINE in
# files/arch/mips/rtl8197f/mach-hh71vm.c), which is the same in every copy of the firmware, so
# any unit ends up as `hh71vm` whatever model is printed on the case. The real model lives in
# /tmp/sysinfo/model, is reported by the Qualcomm side, and takes NO part in the check.
#
#
# The metadata has no effect at all on the first installation: that goes through the bootloader
# and `fwupg` (install_openwrt_lan.py / flash_openwrt_vendor.py), where this check does not exist.
SUPPORTED_DEVICES := hh71vm
BOARD_NAME := hh71vm

# The metadata is appended to the tail of the finished -sysupgrade.bin, so when written it
# lands in the rootfs partition right after the squashfs (the stock default_do_upgrade of
# every upstream mtd target behaves the same way). The bytes are inert: the bootloader reads
# rootfs by the length stamped inside the body, and the kernel by the squashfs superblock. The
# only danger is overflowing the partition, so the size is checked against the sum of both
# partitions AFTER the metadata has been appended.
IMAGE_MAX_SIZE := $(shell expr $(KERNEL_PART_SIZE) + $(ROOTFS_PART_SIZE))

MKRTKIMG := python3 $(TOPDIR)/target/linux/rtkmipsel/image/mkrtkimg.py

# The room between the start of RAM and the loader: 0x84000000 - 0x80000000. The board has
# 128 MB of RAM, so both addresses are inside physical memory.
RAMFS_MAX_SIZE := 67108864

# console=ttyS0,38400 is the console speed of the device (the vendor BSP_UART0_BAUD and the
# vendor CONFIG_CMDLINE say the same).
# The layout: writing to 0x0-0x1FFFF is forbidden always (the bootloader lives there), hwsetting
# (0x20000-0x23FFF) holds the MAC addresses, and our partitions start at 0x30000 - exactly
# where stock keeps its cr6c header. The string has to match CONFIG_CMDLINE in
# rtl8197f/config-4.14: the kernel has CMDLINE_OVERRIDE set and that is what applies in the
# booted system; here the same string is needed by the loader.
KERNEL_CMDLINE := board=HH71VM console=ttyS0,38400 mtdparts=m25p80:128k(boot),16k(hwsetting),48k(config),2880k(kernel),3072k(rootfs),6144k(rootfs_data),4096k(vendor_jffs2) root=/dev/mtdblock4 rootfstype=squashfs

define test
$(shell test $(1) $(2) $(3) && echo 1)
endef

define filesize
$(shell stat -L -c %s $1)
endef

LOADER_MAKE := $(NO_TRACE_MAKE) -C lzma-loader KDIR=$(KDIR) LINUX_DIR=$(LINUX_DIR)

define Build/Clean
	$(LOADER_MAKE) clean
endef

# $(1) is the name of the loader being built, $(2) the file with the compressed kernel in
# $(KDIR), $(3) the link address of the loader (the RAM variant by default).
define Image/BuildLoader
	$(LOADER_MAKE) LOADER=$(1).bin \
		KERNEL_CMDLINE="$(KERNEL_CMDLINE)" \
		LZMA_TEXT_START=$(if $(3),$(3),$(LZMA_TEXT_START)) \
		LOADADDR=$(KERNEL_LOADADDR) \
		LOADER_DATA="$(KDIR)/$(2)" \
		BOARD="hh71vm" \
		compile loader.bin
endef

define Image/Prepare/initramfs
	$(if $(call test,$(call filesize,$(KDIR)/vmlinux-initramfs),-gt,$(RAMFS_MAX_SIZE)),\
		$(error $(KDIR)/vmlinux-initramfs bigger than $(RAMFS_MAX_SIZE) bytes))
	$(STAGING_DIR_HOST)/bin/lzma e $(KDIR)/vmlinux-initramfs -lc1 -lp2 -pb2 \
		$(KDIR)/vmlinux-initramfs.bin.lzma
	$(call Image/BuildLoader,loader-$(SUBTARGET)-hh71vm-ramfs,vmlinux-initramfs.bin.lzma)
endef

# A kernel without initramfs, for flash: $(KDIR)/vmlinux is a raw binary (the ELF sits next to
# it as vmlinux.elf), and that is what the loader expects.
define Image/Prepare/flash
	$(STAGING_DIR_HOST)/bin/lzma e $(KDIR)/vmlinux -lc1 -lp2 -pb2 \
		$(KDIR)/vmlinux.bin.lzma
	$(call Image/BuildLoader,loader-$(SUBTARGET)-hh71vm-flash,vmlinux.bin.lzma,$(FLASH_TEXT_START))
endef

define Image/Prepare
	$(if $(CONFIG_TARGET_ROOTFS_INITRAMFS),$(call Image/Prepare/initramfs))
	$(if $(CONFIG_TARGET_ROOTFS_SQUASHFS),$(call Image/Prepare/flash))
endef

define Image/Build/Initramfs
	$(CP) $(KDIR)/loader-$(SUBTARGET)-hh71vm-ramfs.bin \
		$(BIN_DIR)/$(IMG_PREFIX)-hh71vm-nfjrom.bin
endef

# The permanent image. The order of the steps:
#   1. kernel -> a cr6c container (header, loader, and the checksum bytes chosen for it);
#   2. the squashfs is aligned to 4 KB (the flash sector size), then gets its length stamp
#      and checksum bytes: the bootloader checks the rootfs partition the same way
#      regardless of how it was written, so the same body goes into the sysupgrade image
#      and into the r6cr section;
#   3. both pieces are checked against the size of their partitions (better to fail the build
#      than to learn about an overflow while writing to the device);
#   4. the sysupgrade image: the kernel padded with 0xFF to 2880k, then the squashfs;
#   5. the fwupg container: the same data, but as two sections - cr6c is written together
#      with its header at burnAddr, and r6cr is placed by the vendor fwupg at the start of
#      the rootfs partition (it ignores the burnAddr field for r6cr).
define Image/Build/squashfs
	$(MKRTKIMG) build --sig cr6c --start $(FLASH_TEXT_START) --burn $(KERNEL_PART_OFFSET) \
		--body $(KDIR)/loader-$(SUBTARGET)-hh71vm-flash.bin \
		--out $(KDIR)/hh71vm-kernel-cr6c.bin
	$(MKRTKIMG) pad --file $(KDIR)/root.squashfs --align 4096 \
		--out $(KDIR)/hh71vm-root.squashfs
	$(MKRTKIMG) checksize --file $(KDIR)/hh71vm-kernel-cr6c.bin --max $(KERNEL_PART_SIZE)
	$(MKRTKIMG) stamp --file $(KDIR)/hh71vm-root.squashfs \
		--out $(KDIR)/hh71vm-root-stamped.bin
	$(MKRTKIMG) checksize --file $(KDIR)/hh71vm-root-stamped.bin --max $(ROOTFS_PART_SIZE)
	$(MKRTKIMG) pad --file $(KDIR)/hh71vm-kernel-cr6c.bin --align $(KERNEL_PART_SIZE) \
		--out $(KDIR)/hh71vm-kernel-padded.bin
	$(MKRTKIMG) concat --out $(BIN_DIR)/$(IMG_PREFIX)-hh71vm-sysupgrade.bin \
		$(KDIR)/hh71vm-kernel-padded.bin $(KDIR)/hh71vm-root-stamped.bin
	echo $(call metadata_json,$(SUPPORTED_DEVICES)) | \
		fwtool -I - $(BIN_DIR)/$(IMG_PREFIX)-hh71vm-sysupgrade.bin
	$(MKRTKIMG) checksize --file $(BIN_DIR)/$(IMG_PREFIX)-hh71vm-sysupgrade.bin \
		--max $(IMAGE_MAX_SIZE)
	$(MKRTKIMG) build --sig r6cr --start 0 --burn $(ROOTFS_PART_OFFSET) \
		--body $(KDIR)/hh71vm-root-stamped.bin --body-final \
		--out $(KDIR)/hh71vm-rootfs-r6cr.bin
	$(MKRTKIMG) concat --out $(BIN_DIR)/$(IMG_PREFIX)-hh71vm-fwupg.bin \
		$(KDIR)/hh71vm-kernel-cr6c.bin $(KDIR)/hh71vm-rootfs-r6cr.bin
endef

define Image/Build
	$(call Image/Build/$(1),$(1))
endef

$(eval $(call BuildImage))
