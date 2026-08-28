# Modem extra tools

Optional CLI and LuCI controls for TTL/Hop Limit rewriting and LTE band
selection. The package adds a separate **Modem > Extra tools** page. A fresh
installation leaves both functions disabled; upgrades keep existing settings.

## Requirements

This release is built for the published HH71VM OpenWrt snapshot with:

- board identity `hh71vm`;
- architecture `mipsel_24kc`;
- the exact firmware build and kernel package version listed in the archive's
  `COMPATIBILITY.txt` and the release notes;
- Internet access during installation.

The installer checks the OpenWrt board identity and exact kernel package version.
Never use `--force-depends` or install the included kernel module on another
kernel.

## Installation

Download the `modem-extra-tools-*.zip` bundle from the matching
[release](https://github.com/sowarden/hh71vm-openwrt/releases) and extract it. On Windows,
copy the entire extracted folder to the router; replace the example local path and router
address below:

```text
scp -O -r -o HostKeyAlgorithms=+ssh-rsa "C:\path\to\modem-extra-tools-1.1.0" root@192.168.1.1:/tmp/
ssh -o HostKeyAlgorithms=+ssh-rsa root@192.168.1.1
```

The uploaded folder will be `/tmp/modem-extra-tools-1.1.0`. `-O` uses the legacy SCP
protocol; the host-key option supports the Dropbear server shipped in this build. Check the
host key when prompted. Then run these commands on the router:

```sh
cd /tmp/modem-extra-tools-1.1.0
sh install.sh
```

The installer verifies the board identity, exact kernel ABI, release checksums
and the signature and checksum of the required upstream OpenWrt package. After
installation, open LuCI in a new tab or sign out and in again.

## TTL and Hop Limit

`wan` below is the logical OpenWrt network leading to the Qualcomm mobile modem,
normally device `eth2`. It does not mean an Ethernet WAN socket.

In LuCI, enable TTL Fix, enter your outgoing TTL (and optionally IPv6 Hop Limit),
select the mobile WAN network, then apply the settings.

```sh
modem-extra-tools ttl show
modem-extra-tools ttl set 65 off wan
modem-extra-tools ttl set 65 65 wan
modem-extra-tools ttl disable
```

Values are literal integers from 1 to 255. IPv4 uses TTL and IPv6 uses Hop
Limit. There is no universal value suitable for every provider. The package
refuses to enable rewriting while software or hardware flow offloading is
enabled.

## LTE band selection

In LuCI, click **Read bands**, select the allowed bands and click **Apply bands**.
Use **Restore original** to return to the preference saved before the first edit.
Check the band actually in use under **Modem > Overview**.

```sh
modem-extra-tools bands show
modem-extra-tools bands set 3,7
modem-extra-tools bands restore
modem-extra-tools bands undo
modem-extra-tools bands recover
```

The available list is queried from the modem rather than hard-coded for one
operator or region. Band selection is not a cell lock, EARFCN lock or guaranteed
speed improvement. Keep a LAN connection available when changing bands. If a
selection prevents registration, run `modem-extra-tools bands restore`.

The selected mask is stored by OpenWrt and periodically reapplied if the stock
modem software overwrites it. Do not run another band-management tool at the
same time.

## Persistence and sysupgrade

Configuration is stored in `/etc/config/modem-extra-tools`; transaction state is
kept under `/etc/modem-extra-tools/`. Both are included in the sysupgrade keep
list when **Keep settings** is enabled.

Installed packages are not guaranteed to survive sysupgrade. Reinstall a bundle
built for the new firmware after upgrading. If the kernel ABI changes, the old
kmod must not be reused.

## Troubleshooting

**The Extra tools page is missing:** open LuCI in a new tab or sign out and in
again. LuCI 19.07 may retain its menu in browser session storage.

**TTL reports that offloading is enabled:** disable software and hardware flow
offloading under Network > Firewall, save the settings and retry.

**Mobile registration disappeared after changing bands:** connect over LAN and
run `modem-extra-tools bands restore`.

**A band operation was interrupted:** do not delete pending files manually. Run
`modem-extra-tools bands recover`.

**The modem is unavailable:** the band backend requires the stock local control
channel at `192.168.225.1:23`. It uses Telnet rather than Qualcomm SSH
credentials.

**The installer reports a kernel mismatch:** obtain a bundle built for that
firmware. Do not bypass the check.

## Removal

Restore the normal state first, then remove the application packages:

```sh
modem-extra-tools bands restore
modem-extra-tools ttl disable
opkg remove luci-app-modem-extra-tools modem-extra-tools
```

The additional netfilter packages may remain installed if another application
uses them.

Application and installer: Apache-2.0. Kernel modules: GPL-2.0.
