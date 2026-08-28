# Copyright 2026 sowarden
# Licensed to the public under the Apache License 2.0.

include $(TOPDIR)/rules.mk

LUCI_TITLE:=Modem control for the Alcatel LINKHUB HH71VM
LUCI_DESCRIPTION:=Signal, SIM and PIN, messages, connection profiles, network selection \
and a raw AT console for the Qualcomm modem half of the router. Talks to hh71vm-modemd \
over ubus.
LUCI_DEPENDS:=+luci-base
LUCI_PKGARCH:=all

PKG_LICENSE:=Apache-2.0
PKG_MAINTAINER:=sowarden <sowarden@proton.me>

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
