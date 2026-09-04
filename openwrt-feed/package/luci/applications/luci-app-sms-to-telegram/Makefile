# Copyright 2026 sowarden
# Licensed to the public under the Apache License 2.0.

include $(TOPDIR)/rules.mk

LUCI_TITLE:=SMS to Telegram for HH71VM
LUCI_DESCRIPTION:=Configure the optional sms-to-telegram delivery service.
LUCI_DEPENDS:=+luci-base +luci-app-hh71vm-modem +sms-to-telegram
LUCI_PKGARCH:=all
PKG_VERSION:=1.2.0
PKG_RELEASE:=1

PKG_LICENSE:=Apache-2.0
PKG_MAINTAINER:=sowarden <sowarden@proton.me>

define Package/luci-app-sms-to-telegram/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	rm -f /tmp/luci-indexcache /tmp/luci-indexcache.*.json \
		/tmp/luci-modulecache/6C7563692E636F6E74726F6C6C65722E736D735F746F5F74656C656772616D
	/etc/init.d/rpcd reload >/dev/null 2>&1 || true
}
exit 0
endef

define Package/luci-app-sms-to-telegram/postrm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	rm -f /tmp/luci-indexcache /tmp/luci-indexcache.*.json \
		/tmp/luci-modulecache/6C7563692E636F6E74726F6C6C65722E736D735F746F5F74656C656772616D
	/etc/init.d/rpcd reload >/dev/null 2>&1 || true
}
exit 0
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
