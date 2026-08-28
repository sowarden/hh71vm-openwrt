# SPDX-License-Identifier: GPL-2.0-only
# Build upstream in-tree modules externally against the UNCHANGED release kernel.
# Enabling their KCONFIG in the release config would change its package ABI hash.
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=hh71vm-ipt-ipopt
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only
PKG_MAINTAINER:=sowarden <sowarden@proton.me>
include $(INCLUDE_DIR)/package.mk

IPOPT_MODULES:=xt_dscp xt_DSCP xt_length xt_statistic xt_tcpmss xt_CLASSIFY ipt_ECN xt_ecn xt_hl xt_HL

define KernelPackage/hh71vm-ipt-ipopt
  SUBMENU:=Netfilter Extensions
  TITLE:=IP option modules for the unchanged HH71VM release kernel
  DEPENDS:=@TARGET_rtkmipsel +kmod-ipt-core
  PROVIDES:=kmod-ipt-ipopt
  FILES:=$(foreach module,$(IPOPT_MODULES),$(PKG_BUILD_DIR)/$(module).ko)
  AUTOLOAD:=$(call AutoProbe,$(IPOPT_MODULES))
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(foreach module,$(filter-out ipt_ECN,$(IPOPT_MODULES)),cp $(LINUX_DIR)/net/netfilter/$(module).c $(PKG_BUILD_DIR)/;)
	cp $(LINUX_DIR)/net/ipv4/netfilter/ipt_ECN.c $(PKG_BUILD_DIR)/
	cp ./src/Makefile $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(KERNEL_MAKE) M=$(PKG_BUILD_DIR) modules
endef

$(eval $(call KernelPackage,hh71vm-ipt-ipopt))
