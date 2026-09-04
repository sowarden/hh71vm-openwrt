# Packages built for this port

Everything below is written for the HH71VM port and is not part of upstream OpenWrt. The
installation and recovery material lives one directory up: [flash installation](../flash-install.md),
[RAM boot](../ram-boot.md), [Releases](../releases.md), [package feed](../package-feed.md),
[telnet access](../telnet-access.md).

## In the base firmware

| Package | What it gives you | Document |
|---|---|---|
| `luci-app-hh71vm-modem` | The **Modem** menu: signal, messages, network selection, profiles, SIM and PIN, phonebook, AT console | [Modem control](modem-control.md) |
| `luci-theme-hh71vm` | The default look, light and dark, with the modem status strip in the header | [Theme](theme.md) |

The modem pages talk to `hh71vm-modemd`, the daemon that owns the single control channel to
the Qualcomm half. It is part of the target base files rather than a separate package, and is
described in the [modem control](modem-control.md) document.

The image also mounts the storage that belongs to the Qualcomm half and can install packages
onto it, which is how anything larger than the 6 MiB overlay gets installed at all. See
[external storage](external-storage.md).

## Optional, from the signed feed

Install these with `opkg` from the feed that belongs to your exact firmware build; see
[installing optional packages](../package-feed.md).

| Package | What it gives you | Document |
|---|---|---|
| `modem-extra-tools`, `luci-app-modem-extra-tools` | Persistent TTL / Hop Limit rewriting and transactional LTE band selection | [Extra modem tools](modem-extra-tools.md) |
| `sms-to-telegram`, `luci-app-sms-to-telegram` | Forward incoming SMS to a Telegram chat | [SMS to Telegram](sms-to-telegram.md) |
| `kmod-hh71vm-ipt-ipopt` | The IP option netfilter targets, built against the unchanged release kernel | [IP option modules](ipt-ipopt.md) |
| `xray-core`, `xray`, `luci-app-hh71vm-xray` | **Experimental.** An Xray VPN client with a page to drive it, and the firewall rules that send your devices through the tunnel | [Xray VPN](xray-vpn.md) |

`kmod-hh71vm-ipt-ipopt` is pulled in automatically as a dependency of `modem-extra-tools`;
you do not normally install it by hand.

The Xray packages are an **early experiment** and are not ready for a router you depend on.
`xray-core` is 34 MB and installs onto the storage shared with the modem half rather than
into the overlay, with `hh71vm-extern-pkg install xray-core`; the other two are ordinary
`opkg` packages. Read [Xray VPN](xray-vpn.md) before installing them - in particular what
is not carried through the tunnel, and which clients can be captured at all.

## Kernel modules

[Ported and shipped kernel modules](ported-kmods.md) lists what is built into the image and
what the feed offers separately.
