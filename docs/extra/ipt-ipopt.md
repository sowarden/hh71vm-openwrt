# IP option netfilter modules

`kmod-hh71vm-ipt-ipopt` provides the netfilter targets and matches that upstream OpenWrt
ships as `kmod-ipt-ipopt`. It is an optional package in the signed feed and is installed
automatically as a dependency of [`modem-extra-tools`](modem-extra-tools.md). You do not
normally install it by hand.

```sh
opkg update
opkg install kmod-hh71vm-ipt-ipopt
```

It declares `PROVIDES:=kmod-ipt-ipopt`, so anything that depends on the upstream name is
satisfied by it.

## Why it is a separate package

The modules are upstream in-tree code. Enabling them in the release kernel configuration
would change that configuration, and with it the ABI hash every published module is checked
against — which would invalidate the whole signed feed for that build.

So they are built out of tree against the unchanged release kernel instead. The result is
identical code with a module that stays loadable by the exact firmware it was published for.

## What is in it

`xt_dscp`, `xt_DSCP`, `xt_length`, `xt_statistic`, `xt_tcpmss`, `xt_CLASSIFY`, `ipt_ECN`,
`xt_ecn`, `xt_hl`, `xt_HL`.

`xt_HL` and `xt_hl` are the ones that matter for this port: they are what the TTL and
IPv6 Hop Limit rewriting in `modem-extra-tools` uses.

The userspace half — the `iptables` extensions that let you write `-j TTL --ttl-set 64` —
comes from `iptables-mod-ipopt` in the same feed. Both halves are needed; installing only the
kernel modules leaves `iptables` unable to parse the rule.
