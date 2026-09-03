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
comes up, it is preserved across `sysupgrade`, and it is removed cleanly when the package is
removed.

```sh
modem-extra-tools ttl show
modem-extra-tools ttl set 64          # IPv4 only
modem-extra-tools ttl set 64 64       # IPv4 and the IPv6 Hop Limit
modem-extra-tools ttl set 64 off wan  # IPv4 only, on a named WAN network
modem-extra-tools ttl disable
```

The rewriting itself is done by the netfilter `TTL` and `HL` targets, which come from
[`kmod-hh71vm-ipt-ipopt`](ipt-ipopt.md) and are installed automatically as a dependency.

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
