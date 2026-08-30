# Copyright 2026 sowarden
# Licensed to the public under the Apache License 2.0.

include $(TOPDIR)/rules.mk

PKG_NAME:=modem-extra-tools
PKG_VERSION:=1.1.2
PKG_RELEASE:=1
PKG_LICENSE:=Apache-2.0
PKG_MAINTAINER:=sowarden <sowarden@proton.me>

include $(INCLUDE_DIR)/package.mk

# The only ELF payload is the already-stripped ARM modem helper, not a MIPS ELF.
RSTRIP:=:

define Package/modem-extra-tools
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Persistent TTL and LTE band controls for HH71VM
  DEPENDS:=+lua +libuci-lua +luci-lib-nixio +luci-lib-jsonc +iptables +ip6tables \
	+iptables-mod-ipopt +kmod-ipt-ipopt
endef

define Package/modem-extra-tools/description
  Command-line and rpcd backend for persistent IPv4 TTL / IPv6 Hop Limit
  rewriting and transactional LTE band selection on the Alcatel LINKHUB HH71VM
  OpenWrt port.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	cp ./src/hh71-nas.c ./src/hh71-imei.c ./files/hh71-nas-arm ./files/hh71-imei-arm \
		./files/helpers.sha256 $(PKG_BUILD_DIR)/
endef

define Build/Compile
	cd $(PKG_BUILD_DIR) && sha256sum -c helpers.sha256
endef

define Package/modem-extra-tools/conffiles
/etc/config/modem-extra-tools
endef

define Package/modem-extra-tools/install
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/modem-extra-tools.config $(1)/etc/config/modem-extra-tools
	$(INSTALL_DIR) $(1)/etc/firewall.user.d
	$(INSTALL_DIR) $(1)/etc/modem-extra-tools
	$(INSTALL_DIR) $(1)/lib/upgrade/keep.d $(1)/etc/hotplug.d/iface
	$(INSTALL_DATA) ./files/keep $(1)/lib/upgrade/keep.d/modem-extra-tools
	$(INSTALL_BIN) ./files/95-modem-extra-tools $(1)/etc/hotplug.d/iface/95-modem-extra-tools
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/init $(1)/etc/init.d/modem-extra-tools
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/modem-extra-tools $(1)/usr/sbin/modem-extra-tools
	$(INSTALL_DIR) $(1)/usr/libexec/modem-extra-tools
	$(INSTALL_BIN) ./files/firewall $(1)/usr/libexec/modem-extra-tools/firewall
	$(INSTALL_BIN) ./files/setup $(1)/usr/libexec/modem-extra-tools/setup
	$(INSTALL_BIN) ./files/maintain $(1)/usr/libexec/modem-extra-tools/maintain
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hh71-nas-arm $(1)/usr/libexec/modem-extra-tools/hh71-nas-arm
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hh71-imei-arm $(1)/usr/libexec/modem-extra-tools/hh71-imei-arm
	$(INSTALL_DATA) ./files/*.lua $(1)/usr/libexec/modem-extra-tools/
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./files/rpcd-modem-extra-tools $(1)/usr/libexec/rpcd/modem-extra-tools
endef

define Package/modem-extra-tools/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	/usr/libexec/modem-extra-tools/setup
	/etc/init.d/rpcd reload >/dev/null 2>&1 || true
}
exit 0
endef

define Package/modem-extra-tools/prerm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	[ "$${PKG_UPGRADE}" = 1 ] && exit 0
	/usr/sbin/modem-extra-tools firewall disable --json || exit 1
	uci -q delete firewall.modem_extra_tools
	uci -q commit firewall
	/etc/init.d/rpcd reload >/dev/null 2>&1 || true
}
exit 0
endef

$(eval $(call BuildPackage,modem-extra-tools))
