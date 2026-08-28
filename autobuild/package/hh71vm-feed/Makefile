# SPDX-License-Identifier: Apache-2.0
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include $(TOPDIR)/hh71vm-release.mk

PKG_NAME:=hh71vm-feed
PKG_VERSION:=1
PKG_RELEASE:=$(HH71VM_RELEASE)
PKG_LICENSE:=Apache-2.0
PKG_MAINTAINER:=sowarden <sowarden@proton.me>
include $(INCLUDE_DIR)/package.mk

define Package/hh71vm-feed
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=HH71VM firmware package feed
  DEPENDS:=@TARGET_rtkmipsel +opkg +usign
  EXTRA_DEPENDS:=kernel (= $(LINUX_VERSION)-$(LINUX_RELEASE)-$(LINUX_VERMAGIC))
endef

define Build/Compile

endef

define Package/hh71vm-feed/install
	$(INSTALL_DIR) $(1)/usr/share/hh71vm-feed $(1)/usr/libexec $(1)/etc/uci-defaults
	$(INSTALL_DIR) $(1)/etc/opkg/keys
	$(INSTALL_DATA) ./files/release.pub $(1)/usr/share/hh71vm-feed/release.pub
	$(INSTALL_DATA) ./files/release.pub $(1)/etc/opkg/keys/$(HH71VM_KEY_ID)
	printf '%s\n' 'release=$(HH71VM_RELEASE)' \
	  'kernel=$(LINUX_VERSION)-$(LINUX_RELEASE)-$(LINUX_VERMAGIC)' \
	  'architecture=mipsel_24kc' 'key_id=$(HH71VM_KEY_ID)' \
	  'feed_url=$(HH71VM_FEED_URL)' > $(1)/usr/share/hh71vm-feed/release.conf
	printf '%s\n' 'src/gz hh71vm $(HH71VM_FEED_URL)' > $(1)/etc/opkg/hh71vm.conf
	$(INSTALL_BIN) ./files/reconcile.sh $(1)/usr/libexec/hh71vm-feed-reconcile
	$(INSTALL_BIN) ./files/99-hh71vm-feed $(1)/etc/uci-defaults/99-hh71vm-feed
endef

$(eval $(call BuildPackage,hh71vm-feed))
