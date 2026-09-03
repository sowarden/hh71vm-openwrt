# OpenWrt for the Alcatel LINKHUB HH71VM

This repository publishes an OpenWrt port for the Realtek subsystem of the Alcatel LINKHUB
HH71VM (`RTL8197FS` + `RTL8812FE`), together with the source delta it is built from.

The supported deployment is a **permanent installation into the Realtek SPI flash**. The
router then boots OpenWrt on its own and keeps its settings across power cycles.

Loading the build into RAM is still available as an **optional dry run** for owners who have
a UART adapter and want to see the port work on their unit before writing flash. It is not
the normal way to use this firmware.

> [!IMPORTANT]
> This port was developed and verified on one physical HH71VM. Other HH71VM board
> revisions, regional variants, and carrier variants are unverified. Read
> [known issues](docs/known-issues.md) before installing.

## Start here

| You want to | Read |
|---|---|
| Download firmware and flashing tools | [Latest flash bundle](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/hh71vm-openwrt-flash-bundle.zip) |
| Install OpenWrt permanently | [Flash installation guide](docs/flash-install.md) |
| Add optional modem controls or WireGuard | [Signed package feed](docs/package-feed.md) |
| See what the packages of this port do | [Packages built for this port](docs/extra/custom-packages.md) |
| Try the build first, without writing flash (needs UART) | [RAM boot guide](docs/ram-boot.md) |
| Check what works and what does not | [Known issues](docs/known-issues.md) |
| Test a subsystem and report results | [Testing guide](docs/testing.md) |
| Rebuild from source | [Sources and build instructions](docs/sources.md) |

After any installation attempt, successful or not, please submit a
[compatibility report](https://github.com/sowarden/hh71vm-openwrt/issues/new/choose).

## Firmware files

Builds are distributed as [immutable Releases](https://github.com/sowarden/hh71vm-openwrt/releases),
each with matching checksums, source archives, and a signed package feed. The
[latest flash bundle](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/hh71vm-openwrt-flash-bundle.zip)
contains the firmware, flashing tools, and installation guides. No generated firmware or IPK
files are stored in the Git repository; cloning the repository is only needed for development.

See [firmware Releases](docs/releases.md) for the bundle checksum and individual image links.
The floating `latest` links are only for downloading new firmware. An installed image uses
the exact immutable Release tag embedded at build time for its package feed. See
[package installation](docs/package-feed.md) for that ABI boundary.

## Before you install anything

Read this section in full. It is short and it is the difference between a recoverable and an
unrecoverable mistake.

- **Take your own stock backup first.** The installer reads the original flash contents of
  your router over the network and saves them. That copy is what restores your stock
  firmware if you decide to go back. Do not use a Realtek dump from another HH71VM for
  recovery: it can contain device-specific settings and may not match your hardware.
- **The bootloader is never overwritten.** Installation writes only the kernel and root
  filesystem. The bootloader, the `hwsetting` area holding the MAC addresses, the vendor
  configuration, and the vendor JFFS2 partition are left alone. This is what makes recovery
  possible without opening the case.
- **Recovery does not need UART.** The stock bootloader drops into its own console when the
  WPS button is held while power is applied, and brings Ethernet up by itself. The rollback
  tool uses that path.
- **A hardware safety net is still worth having.** An SPI flash programmer and a full-chip
  dump recover a device in any state. Nothing in this repository requires one.
- **The Qualcomm modem subsystem is not touched.** It is a separate system with its own
  firmware and settings, and installing OpenWrt on the Realtek side does not reset it.

## What works on the reference device

| Subsystem | Status |
|---|---|
| OpenWrt 19.07 / Linux 4.14.275 | Boots from flash; settings persist |
| Ethernet and external gigabit PHY | Working |
| 2.4 GHz Wi-Fi (`RTL8197FS`) | Working through UCI/netifd |
| 5 GHz Wi-Fi (`RTL8812FE`) | Working through UCI/netifd |
| Qualcomm USB mux and RNDIS WAN (`eth2`) | Working |
| Mobile Internet through the Qualcomm modem | Working |
| LuCI, HH71VM theme, modem-control pages | Working |
| SMS reading, including multipart messages | Working |
| Firmware update with `sysupgrade` | Working |
| Rollback to your own stock backup | Working |

## RAM image

[Download the latest `nfjrom` image](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/openwrt-rtkmipsel-rtl8197f-hh71vm-nfjrom.bin)
and verify it against the Release
[`SHA256SUMS`](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/SHA256SUMS).

RAM boot is only a compatibility dry run. It does not exercise flash installation,
persistence, `sysupgrade`, or rollback, and all OpenWrt-side changes disappear at power-off.

## Default network settings

| Setting | Value |
|---|---|
| LAN address | `192.168.1.1` |
| Root password | not set |
| 2.4 GHz SSID / key | `HH71VM` / `hh71vm12345` |
| 5 GHz SSID / key | `HH71VM-5G` / `hh71vm12345` |

These credentials are public. Set a root password and change both Wi-Fi keys immediately
after the first boot.

## Reporting safely

Both positive and negative results are valuable. Reports should identify the visible board
revision, device model, region, stock firmware version, and exact image hash, then include
the requested diagnostic output.

Review every log before publishing it. Redact device-unique data you do not want public,
including full MAC addresses, serial numbers, IMEI/IMSI values, phone numbers, SMS content,
credentials, and keys. Attach searchable text, not screenshots of text.

## Repository map

| Path | Purpose |
|---|---|
| [`docs/flash-install.md`](docs/flash-install.md) | Backup, permanent installation, update, rollback |
| [`docs/ram-boot.md`](docs/ram-boot.md) | Optional RAM dry run over UART |
| [`docs/known-issues.md`](docs/known-issues.md) | Current limitations and expected quirks |
| [`docs/testing.md`](docs/testing.md) | Test matrix, log collection, and reporting |
| [`docs/sources.md`](docs/sources.md) | Source provenance and build instructions |
| [`docs/releases.md`](docs/releases.md) | Latest downloads and immutable Release identity |
| [`docs/driver-reuse.md`](docs/driver-reuse.md) | Advanced port-reuse guidance and coupling |
| [`docs/extra/`](docs/extra/custom-packages.md) | The packages and kernel modules built for this port |
| [`openwrt-feed/`](openwrt-feed/) | HH71VM source delta and build config |
| [`autobuild/`](autobuild/) | Unified firmware build and immutable signed feed |
| [`tools/flash/`](tools/flash/) | Backup, installation, update, and rollback utilities |
| [`tools/ram_boot.py`](tools/ram_boot.py) | RAM-only loader and UART capture tool |
| [`CHANGELOG.md`](CHANGELOG.md) | Published snapshot history |

## Source and licenses

The source delta used for the published image is included and its OpenWrt and feed revisions
are pinned in [docs/sources.md](docs/sources.md).

The source tree contains components under their respective upstream licenses. Project
documentation and the RAM boot tool use a separate license. See
[LICENSING.md](LICENSING.md) for the repository map.
