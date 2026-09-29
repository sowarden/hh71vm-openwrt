# HH71VM delta for OpenWrt 19.07

This directory contains the target, package overlays, patches, vendor driver ports, and
captured build configuration used for the HH71VM firmware. It is applied on top
of a pinned OpenWrt checkout; it is not a standalone source tree.

Use the repository-level [source and build guide](../docs/sources.md) for exact revisions,
commands, artifact hashes, and provenance.

## Layout

```text
target/linux/rtkmipsel/
  files/arch/mips/rtl8197f/                    RTL8197F BSP and HH71VM board
  files/arch/mips/include/asm/mach-rtl8197f/   SoC register map
  files/drivers/net/rtl819x/                   rtknet Ethernet/switch port
  files/drivers/char/rtl_nfbi/                 external PHY MDIO/NFBI support
  files/drivers/net/wireless/realtek/rtl8192cd vendor Wi-Fi port
  files/drivers/spi/sheipa/                    SPI-NOR controller port
  base-files/                                  network, Wi-Fi, modem integration
  image/                                       RAM and development flash image builders
  patches-4.14/                                kernel integration patches
  rtl8197f/config-4.14                         target kernel configuration
package/
  luci/applications/luci-app-hh71vm-modem/     the Modem menu (base image)
  luci/applications/luci-app-modem-extra-tools/  TTL, LTE bands, IMEI, lock creation
  luci/applications/luci-app-sms-to-telegram/  SMS forwarding page
  luci/applications/luci-app-hh71vm-xray/      Xray VPN page (experimental)
  luci/themes/luci-theme-hh71vm/               the default theme (base image)
  net/xray-core/                               Xray build for this target
  utils/hh71vm-simlock/                        carrier SIM lock backend (base image)
  utils/hh71vm-ipt-ipopt/                      IP option netfilter modules (base image)
  utils/modem-extra-tools/                     modem helper daemon and CLI
  utils/sms-to-telegram/                       SMS-to-Telegram worker
  network/utils/iwinfo/patches/                iwinfo patches for the vendor radios
patches/                                       patches against upstream trees
scripts/                                       host-tool build preflight
build.config                                   captured OpenWrt build configuration
config.buildinfo, feeds.buildinfo              captured upstream build inputs
```

## Verified hardware configuration

- RTL8197FS SoC, MIPS 24Kc, 128 MiB RAM;
- external gigabit PHY on switch port 0, MDIO address 6;
- SoC 2.4 GHz radio with `SOC_RFE_TYPE_1`;
- PCIe RTL8812FE 5 GHz radio in slot 0 with `SLOT_0_RFE_TYPE_0`;
- Qualcomm connection through the GPIO24 USB mux and RNDIS `eth2`;
- SPI-NOR `w25q128` through the `spi-sheipa` controller.

These values were verified on one HH71VM. They must be treated as board-specific until
reports from other revisions prove otherwise.

Do not present the vendor Ethernet or Wi-Fi trees as generic drop-in Linux drivers. Their
current integration depends on the RTL8197F BSP, Linux 4.14, Realtek SDK interfaces, board
RFE selections, DMA exports, and HH71VM-specific userspace setup.
