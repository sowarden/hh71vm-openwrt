# Extra modem tools

`modem-extra-tools` and `luci-app-modem-extra-tools` are optional packages. They are not in
the base firmware. Install them from the signed feed that belongs to your exact firmware
build; do not mix packages from another Release.

```sh
opkg update
opkg install luci-app-modem-extra-tools
```

Log out of LuCI and sign in again, then open **Modem > Extra tools**.

They are kept out of the base image on purpose: the LTE band and IMEI work is done by helper
binaries that talk to the modem's own QMI services, and a router that does not need them
should not be carrying them.

## TTL and Hop Limit

Some mobile operators count how many devices are behind a router by looking at how far the
IP time-to-live has already been decremented. Setting a fixed TTL on traffic leaving the
mobile interface removes that signal.

The setting is persistent: it is reapplied by a hotplug handler whenever the mobile interface
comes up, by the firewall include on every `fw3` reload, and by a reconciler that checks once
a minute that the rules are still in the kernel. It is preserved across `sysupgrade`, and it
is removed cleanly when the package is removed.

The reconciler is what makes it reliable rather than merely persistent. The two event-driven
paths can both miss: the mobile `wan` is `proto static` on `eth2`, so it comes up once per
boot however often the data session behind it drops and returns, and any `fw3` reload after
that flushes `mangle POSTROUTING` — taking the jump with it — and re-runs the include at a
moment the WAN device may not yet resolve. Before the reconciler existed, either miss left
the fix reading as enabled with no rule behind it, and the only way back was switching it off
and on by hand.

To check or force it:

```sh
modem-extra-tools ttl show       # ipv4_active / ipv6_active are the kernel's answer
modem-extra-tools ttl reconcile  # reapply now if the rules are missing
```

```sh
modem-extra-tools ttl show
modem-extra-tools ttl set 64          # IPv4 only
modem-extra-tools ttl set 64 64       # IPv4 and the IPv6 Hop Limit
modem-extra-tools ttl set 64 off wan  # IPv4 only, on a named WAN network
modem-extra-tools ttl disable
```

The rewriting itself is done by the netfilter `TTL` and `HL` targets, which come from
[`kmod-hh71vm-ipt-ipopt`](ipt-ipopt.md) and are installed automatically as a dependency.

### Choosing the value

The default of 65 assumes the Qualcomm side of this router decrements TTL by one hop on its
way to the mobile interface, the same way a second router behind this one would -- so 65
arrives at the operator already looking like 64, an ordinary single device. That assumption
is `[UNPROVEN]` on this port: one report (2026-09-14) had 65 flagged by the operator and 64
accepted, on the same router. If your operator flags 65, try 64 (`modem-extra-tools ttl set
64`) before assuming the feature does not work at all; either value is a one-command change
and neither has been shown to be correct in general.

### Rules without this package

TTL/Hop Limit rewriting through the ordinary OpenWrt firewall (`iptables -t mangle ...`
directly, or a `firewall.user`/`nftables` rule) needs the netfilter targets and the matching
`iptables` extensions. **Both now ship in the image** — `iptables-mod-ipopt` and
`kmod-hh71vm-ipt-ipopt`, about 18 KB together — so a hand-written rule works out of the box:

```sh
iptables -t mangle -A POSTROUTING -o eth2 -j TTL --ttl-set 64
```

They used to be feed-only, pulled in as a dependency of this package. That left anyone who
wrote their own rules *without* installing `modem-extra-tools` facing
`iptables: unknown option "--ttl-set"`, which names neither the missing piece nor the fix.
Nothing can install a package on demand when `iptables` meets a target it does not know, so
the only way for the rule to work unprompted is for the targets to be present already.

## LTE band selection

Restricting the modem to particular LTE bands is useful when the nearest cell on one band is
congested and a weaker band is faster in practice. The capability list is read from the modem
rather than guessed from an operator table, so only bands this modem actually supports can be
selected.

```sh
modem-extra-tools bands show
modem-extra-tools bands backup
modem-extra-tools bands set 3,7
modem-extra-tools bands undo       # back to the previous selection
modem-extra-tools bands restore    # back to the original, from the backup
modem-extra-tools bands recover    # finish an interrupted change
```

The change is transactional. The original mask is saved before the first change, the desired
and previous masks are kept on disk, and `recover` completes or rolls back a change that was
interrupted — by a reboot in the middle, for instance. This matters because a selection that
excludes every band the local cells use leaves the router with no mobile service at all;
`restore` is the way back.

## IMEI

The IMEI helper is restore-only. It reads the modem's NV item, keeps a backup before writing,
and will only write a value that passes the standard IMEI check digit:

```sh
modem-extra-tools imei show
modem-extra-tools imei restore <the device's own 15-digit IMEI> --confirm-original-imei
modem-extra-tools imei recover
```

The confirmation flag is deliberately long and unpleasant to type. It exists so this cannot
be run by accident, and the intended use is restoring the value printed on your own device
after it has been lost — for example by a failed modem firmware operation. Check your local
law before changing it to anything else.

## Status

```sh
modem-extra-tools status --json
```

The same information is on the LuCI page. The package also exposes an rpcd backend, so a
script on the router can read the same state through ubus instead of parsing CLI output.
