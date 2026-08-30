# Copyright 2026 sowarden
# Licensed to the public under the Apache License 2.0.

include $(TOPDIR)/rules.mk

PKG_NAME:=sms-to-telegram
PKG_VERSION:=1.1.0
PKG_RELEASE:=1
PKG_LICENSE:=Apache-2.0 AND ISC
PKG_MAINTAINER:=sowarden <sowarden@proton.me>

include $(INCLUDE_DIR)/package.mk

define Package/sms-to-telegram
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Forward HH71VM SMS messages to Telegram
  DEPENDS:=+lua +libuci-lua +libubus-lua +luci-lib-jsonc +luci-lib-nixio \
	+libuclient +libustream-mbedtls +ca-bundle
endef

define Package/sms-to-telegram/description
  Persistent SMS-to-Telegram delivery worker for the HH71VM OpenWrt modem
  backend. It uses the daemon-owned ubus SMS API and never opens a second modem
  control channel.
endef

TARGET_CFLAGS += -Wall -Wextra -Werror -std=gnu99

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	cp ./src/http_transport.c $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-o $(PKG_BUILD_DIR)/sms-to-telegram-http $(PKG_BUILD_DIR)/http_transport.c \
		$(TARGET_LDFLAGS) -luclient -lubox -ldl
endef

define Package/sms-to-telegram/conffiles
/etc/config/sms-to-telegram
endef

define Package/sms-to-telegram/install
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d $(1)/etc/sms-to-telegram
	$(INSTALL_CONF) ./files/sms-to-telegram.config $(1)/etc/config/sms-to-telegram
	$(INSTALL_BIN) ./files/init $(1)/etc/init.d/sms-to-telegram
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/usr/libexec/sms-to-telegram $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./files/sms-to-telegram $(1)/usr/sbin/sms-to-telegram
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sms-to-telegram-http $(1)/usr/libexec/sms-to-telegram/http
	$(INSTALL_DATA) ./files/*.lua $(1)/usr/libexec/sms-to-telegram/
	$(INSTALL_BIN) ./files/rpcd-sms-to-telegram $(1)/usr/libexec/rpcd/sms-to-telegram
	$(INSTALL_DIR) $(1)/lib/upgrade/keep.d
	$(INSTALL_DATA) ./files/keep $(1)/lib/upgrade/keep.d/sms-to-telegram
endef

define Package/sms-to-telegram/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	chmod 0600 /etc/config/sms-to-telegram
	mkdir -p /etc/sms-to-telegram
	chmod 0700 /etc/sms-to-telegram
	/etc/init.d/sms-to-telegram enable
	/etc/init.d/sms-to-telegram restart >/dev/null 2>&1 || true
	/etc/init.d/rpcd reload >/dev/null 2>&1 || true
}
exit 0
endef

define Package/sms-to-telegram/prerm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	[ "$${PKG_UPGRADE}" = 1 ] || /etc/init.d/sms-to-telegram disable
	/etc/init.d/sms-to-telegram stop >/dev/null 2>&1 || true
	/etc/init.d/rpcd reload >/dev/null 2>&1 || true
}
exit 0
endef

$(eval $(call BuildPackage,sms-to-telegram))
