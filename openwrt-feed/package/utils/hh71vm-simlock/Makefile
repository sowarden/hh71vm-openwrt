# Copyright 2026 sowarden
# Licensed to the public under the Apache License 2.0.

include $(TOPDIR)/rules.mk

PKG_NAME:=hh71vm-simlock
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_LICENSE:=Apache-2.0
PKG_MAINTAINER:=sowarden <sowarden@proton.me>

include $(INCLUDE_DIR)/package.mk

# The UIM helper is an already-stripped ARM binary for the modem side, not a MIPS ELF.
RSTRIP:=:

define Package/hh71vm-simlock
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Carrier SIM lock status and removal for HH71VM
  DEPENDS:=+lua +luci-lib-nixio +luci-lib-jsonc
endef

define Package/hh71vm-simlock/description
  Reads the carrier (network) lock of the Qualcomm modem and removes it, either with an
  unlock code or, on the firmware this was verified against, without one. Part of the
  base image on purpose: a router whose SIM is refused cannot install a package first.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	cp ./src/hh71-uim.c ./files/hh71-uim-arm ./files/helpers.sha256 $(PKG_BUILD_DIR)/
endef

define Build/Compile
	cd $(PKG_BUILD_DIR) && sha256sum -c helpers.sha256
endef

define Package/hh71vm-simlock/install
	$(INSTALL_DIR) $(1)/etc/hh71vm-simlock
	$(INSTALL_DIR) $(1)/usr/libexec/hh71vm-simlock
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hh71-uim-arm $(1)/usr/libexec/hh71vm-simlock/hh71-uim-arm
	$(INSTALL_DATA) ./files/*.lua $(1)/usr/libexec/hh71vm-simlock/
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./files/rpcd-hh71vm-simlock $(1)/usr/libexec/rpcd/hh71vm-simlock
endef

define Package/hh71vm-simlock/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || /etc/init.d/rpcd reload >/dev/null 2>&1 || true
exit 0
endef

define Package/hh71vm-simlock/postrm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || /etc/init.d/rpcd reload >/dev/null 2>&1 || true
exit 0
endef

$(eval $(call BuildPackage,hh71vm-simlock))
