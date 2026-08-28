# Reusing the RTL8197F hardware ports

The source release is useful to other RTL8197F projects, but the current code is a complete
board-support stack rather than a collection of drop-in drivers.

## What is available

| Area | Current source | Verified scope |
|---|---|---|
| RTL8197F BSP | `openwrt-feed/target/linux/rtkmipsel/files/arch/mips/rtl8197f/` | HH71VM on Linux 4.14.275 |
| Ethernet/switch | `files/drivers/net/rtl819x/` | HH71VM switch map and external PHY |
| External PHY support | `files/drivers/char/rtl_nfbi/` | port 0, MDIO address 6 |
| 2.4/5 GHz Wi-Fi | `files/drivers/net/wireless/realtek/rtl8192cd/` | RTL8197FS RFE1 + RTL8812FE slot 0/RFE0 |
| SPI-NOR | `files/drivers/spi/sheipa/` | HH71VM `w25q128` |
| OpenWrt integration | `base-files/`, `patches-4.14/`, `config-4.14` | the published target delta |

## Important coupling

The Ethernet port depends on Realtek SDK APIs, SoC register definitions, `rtl_nfbi`, the
vendor switch model, and board-specific port masks. The HH71VM's physical RJ45 path uses an
external PHY; another board can have a different PHY, MDIO address, port mask, or no external
PHY at all.

The Wi-Fi port depends on the RTL8197F BSP, vendor `rtl8192cd` interfaces, exported MIPS DMA
cache symbols, firmware data, and compile-time RFE selections. A different antenna/RF front
end requires evidence from that board; copying the HH71VM configuration can produce poor or
unsafe radio behavior even when the driver loads.

The Qualcomm modem integration is HH71VM-specific and should not be treated as part of
a generic RTL8197F driver package.

## Recommended reuse model

Use this repository as a pinned reference implementation and port the complete
`rtkmipsel/rtl8197f` delta into a separate board branch. Keep donor provenance and file
licenses intact. Parameterize board-specific values before claiming support for another
device:

- UART register layout and clock;
- RAM size and image addresses;
- switch port masks and external-PHY wiring;
- MAC/calibration storage layout;
- PCIe slot and radio identity;
- 2.4/5 GHz RFE selections;
- GPIO muxing and LEDs/buttons;
- flash controller, chip, erase size, and partition map.

Treat a successful build as only an offline result. Hardware support requires UART boot
evidence, link and traffic tests, radio marker checks, stability testing, and a recovery path
appropriate to the target board.

## Separate driver repository?

A dedicated reusable repository can make sense later, but splitting the trees now would hide
their BSP and board dependencies and create a misleading "ready-made driver" promise. A
better future package is one versioned **RTL8197F OpenWrt hardware-support repository** that
keeps BSP, kernel patches, Ethernet, Wi-Fi, firmware provenance, configuration examples, and
tested-board matrices together.

Before that split, complete a clean rebuild, finish the source/license audit, isolate
HH71VM-only userspace code, and document which configuration values are board parameters
rather than driver constants.
