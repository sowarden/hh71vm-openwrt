# Kernel module packages for the rtkmipsel target.
#
# There is currently one: the vendor Wi-Fi driver. Without this package, the built `rtl8192cd.ko`
# remains in build_dir and is not included in the image because OpenWrt installs only modules
# declared as packages.
#
# It remains a module (CONFIG_RTL8192CD=m in rtl8197f/config-4.14) so development iterations
# can use `scp -O` and `insmod` over SSH without rebuilding the image or power-cycling into
# RAM boot mode by holding WPS. The full rationale is in
# docs/rtl8192cd-porting-map.md.

define KernelPackage/rtl8192cd
  SUBMENU:=$(WIRELESS_MENU)
  TITLE:=Realtek RTL8192CD (vendor driver, RTL8197F 2.4 GHz SoC radio)
  DEPENDS:=@TARGET_rtkmipsel
  KCONFIG:=CONFIG_RTL8192CD
  FILES:=$(LINUX_DIR)/drivers/net/wireless/realtek/rtl8192cd/rtl8192cd.ko
  AUTOLOAD:=$(call AutoLoad,50,rtl8192cd)
endef

define KernelPackage/rtl8192cd/description
 Vendor Realtek Wi-Fi driver from SDK v3.4.14b (D-Link DIR-842E GPL archive),
 ported to Linux 4.14. This is not a cfg80211 driver: it is configured through
 private ioctls via iwpriv, so UCI/netifd use the custom handler at
 /lib/netifd/wireless/rtl8192cd.sh.
endef

$(eval $(call KernelPackage,rtl8192cd))
