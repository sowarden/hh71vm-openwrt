#
# Profile of the only target device so far.
#
# The profile contains the board-specific runtime packages in addition to OpenWrt defaults.
# The rtknet Ethernet driver is built into the kernel, while the rtl8192cd Wi-Fi driver is
# packaged as kmod-rtl8192cd. Keeping that distinction explicit prevents the image recipe
# from referring to packages that do not exist and ensures the loadable Wi-Fi module is
# actually installed in the image.
#
# This is free software, licensed under the GNU General Public License v2.
#

# Ethernet: swconfig is the userspace utility for the built-in switch driver
# (rtl819x_switch.c). Without it, the status of the ports has to be viewed only through
# /proc/rtl865x/port_status.
# Wi-Fi: kmod-rtl8192cd packages the vendor 2.4/5 GHz driver. Without this package,
# rtl8192cd.ko remains in build_dir and is not included in the image. See ../../modules.mk.
# Access points are configured through the UCI/netifd handler at
# /lib/netifd/wireless/rtl8192cd.sh (the driver is not cfg80211, the handler translates sections
# wifi-device/wifi-iface to calls iwpriv). The previous /etc/init.d/rtl8192cd-ap has been removed.
# opkg: libustream-mbedtls + ca-bundle gives HTTPS in uclient-fetch/wget and therefore
# working `opkg update` and loading on https from the box. mbedtls, not wolfssl:
# 179 KB vs 479 KB is the set size, and wolfssl is not needed in the image -
# WPA-key is considered by the vendor driver, we do not install wpad.
# WireGuard dependencies: kmod-udptunnel4 and kmod-udptunnel6 provide UDP tunneling
# (`udp_tunnel.ko`, `ip6_udp_tunnel.ko`) and `CONFIG_DST_CACHE`, which pulls them along
# KCONFIG. By themselves they do nothing, but without them the kernel does not export
# `udp_tunnel_xmit_skb`, `udp_sock_create6` and `dst_cache_*`, and any external module
# tunnel does not load: checked 2026-08-24 on `kmod-wireguard`, which got up through
# opkg, but fell from `Unknown symbol udp_sock_create6`. Two modules weigh a few kilobytes,
# and without them, WireGuard would have to be repaired by rebuilding the kernel for everyone.
# WireGuard itself is deliberately NOT included in the image: it is installed in packages as desired, see.
# docs/wireguard-on-hh71vm.md.
define Profile/hh71vm
  NAME:=Alcatel LINKHUB HH71VM
  PRIORITY:=1
  PACKAGES:=swconfig kmod-rtl8192cd libustream-mbedtls ca-bundle kmod-udptunnel4 kmod-udptunnel6
endef

define Profile/hh71vm/Description
	Alcatel LINKHUB HH71VM (RTL8197FS + RTL8812FE), Realtek side
endef

$(eval $(call Profile,hh71vm))
