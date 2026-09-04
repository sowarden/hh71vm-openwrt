# Copyright 2026 sowarden
# Licensed to the public under the Apache License 2.0.

include $(TOPDIR)/rules.mk

# luci.mk works the name out from the directory, so this line changes nothing about the
# build. It is here so the local build launcher can find this package by name: its
# resolver greps the Makefiles for `PKG_NAME` or `define Package/<name>`, and a LuCI
# application that declares neither cannot be rebuilt with `-Mode Package`.
PKG_NAME:=luci-app-hh71vm-xray

LUCI_TITLE:=Xray VPN control for the Alcatel LINKHUB HH71VM
LUCI_DESCRIPTION:=Profiles, a Connect button that says precisely why it failed, and \
the two switches that decide what happens when nobody is watching. Talks to the xray \
package over ubus.
LUCI_DEPENDS:=+luci-base +xray
LUCI_PKGARCH:=all

PKG_LICENSE:=Apache-2.0
PKG_MAINTAINER:=sowarden <sowarden@proton.me>

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
