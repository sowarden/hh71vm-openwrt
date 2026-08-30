# Copyright 2026 sowarden
# Licensed to the public under the Apache License 2.0.

include $(TOPDIR)/rules.mk

LUCI_TITLE:=Extra modem controls for HH71VM
LUCI_DESCRIPTION:=A separate Modem page for modem-extra-tools.
LUCI_DEPENDS:=+luci-base +luci-app-hh71vm-modem +modem-extra-tools
LUCI_PKGARCH:=all
PKG_VERSION:=1.1.2
PKG_RELEASE:=1

PKG_LICENSE:=Apache-2.0
PKG_MAINTAINER:=sowarden <sowarden@proton.me>

define Package/luci-app-modem-extra-tools/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	rm -f /tmp/luci-indexcache /tmp/luci-indexcache.*.json /tmp/luci-modulecache/6C7563692E636F6E74726F6C6C65722E6D6F64656D5F65787472615F746F6F6C73
	/etc/init.d/rpcd reload >/dev/null 2>&1 || true
}
exit 0
endef

define Package/luci-app-modem-extra-tools/postrm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	rm -f /tmp/luci-indexcache /tmp/luci-indexcache.*.json /tmp/luci-modulecache/6C7563692E636F6E74726F6C6C65722E6D6F64656D5F65787472615F746F6F6C73
	/etc/init.d/rpcd reload >/dev/null 2>&1 || true
}
exit 0
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
