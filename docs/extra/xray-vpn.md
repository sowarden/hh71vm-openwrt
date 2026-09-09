# Xray VPN — an early experiment

> **This is an experiment, not a finished feature.** It works, it has been used against a
> real REALITY server, and it will have rough edges. Do not put it on a router you depend
> on. Reports of what breaks are the point of shipping it this early.

Xray is a proxy client. This adds it to the router, with a page to drive it and the
firewall rules that send your devices' traffic through the tunnel without configuring
anything on the devices themselves.

**Services → VPN (Xray)** after installation.

## What it does

- **Profiles.** As many as you like, added by pasting a `vless://`, `vmess://`, `trojan://`
  or `ss://` link, or by typing the fields in. Click one to make it active.
- **Connect**, with an answer. Six steps run in order — binary, configuration, Xray's own
  validation, start, firewall, and a real request through the tunnel — and a failure names
  the step and what it usually means. Xray's own wording is misleading often enough to be
  worth translating: `invalid user` is usually the router's clock, and the two flow
  rejections mean opposite things.
- **VPN mode** (the default). The traffic of every client that uses this router as its
  gateway goes through the tunnel — LAN and both Wi-Fi bands — with nothing set on the
  client. Which interfaces are captured is worked out on the router and shown on the page.
- **Proxy mode**, if you prefer: a SOCKS inbound on 1080 and an HTTP inbound on 1081, and
  clients point at them themselves. This mode needs no captured LAN interface and installs
  no traffic-redirection rules. Switching from VPN mode removes the old capture rules
  before the proxy listeners start.
- **Connect automatically on power on**, and **reconnect automatically if the connection
  drops** — the second is a real request through the tunnel on a timer, not just a check
  that the process is alive.
- **The clock is set from the connection.** This board has no working NTP, and VMess
  refuses any handshake more than 90 seconds out.
- **A small HTTP API**, off by default, for automation: list profiles, activate one,
  connect, disconnect. The page shows the exact `curl` commands with your token in them.

## What it does not do

- **UDP other than DNS is not carried, and this is deliberate.** Xray captures it, tunnels
  it, and the answer is never written back on this hardware, so the traffic would disappear
  rather than go out unproxied. QUIC is rejected instead, which makes browsers fall back to
  TCP, and DNS is tunnelled through `dnsmasq`. There is a switch to try it anyway.
- **IPv6 is not tunnelled.** While VPN mode is on, forwarded IPv6 to global addresses is
  rejected so that it cannot leak around the tunnel.
- **Only clients that route through this router are captured.** If the router is wired as a
  dumb access point — its LAN bridged to another router that hands out the addresses — then
  its clients are that other router's clients, their traffic never passes through here, and
  nothing can be redirected. This is the normal case only in that specific wiring; a router
  serving its own DHCP, on the SIM or on a WAN cable, captures its clients.
- **No `geoip.dat` / `geosite.dat`.** They are about 20 MB and the shared storage has about
  21 MB free, so any rule naming `geoip:` or `geosite:` will fail. Split routing is
  therefore not available.
- **No subscription links, no per-client rules, no traffic statistics.**

## Speed

The tunnel is limited by this CPU, not by the connection: about **12 Mbit/s** with REALITY
and `xtls-rprx-vision`, about **12 Mbit/s** with REALITY alone, about **27 Mbit/s** for
VMess over plain TCP, and about **87 Mbit/s** for VLESS with no encryption. Four parallel
encrypted streams reach the same 14 Mbit/s in total, which is the processor.

## Memory on a 128 MiB board

Xray-core v26.3.27 bundles a version of the `reality` library with a confirmed upstream
connection leak (XTLS/Xray-core#6684): a peer that goes silent without closing cleanly is
never reclaimed. On the LTE uplink this router normally runs on, that is not a rare case.
Several settings exist to bound the damage instead of relying only on the upstream fix:

- **Connection limits.** `policy_handshake` and `policy_conn_idle` (Settings) cap how long a
  handshake or an idle connection may sit in Xray's own table before it is dropped, and TCP
  keepalive (`keepalive_idle`, `keepalive_interval`) makes the kernel notice a silent peer on
  the proxy's own socket instead of waiting for Xray to.
- **A Go memory ceiling.** `gogc` and `gomemlimit_mb` are passed to the Xray process as
  `GOGC` and `GOMEMLIMIT`, so the runtime collects sooner and has a soft limit to collect
  against, rather than only reacting after the fact on a board with no swap.
- **An RSS guard in the watchdog.** When `mem_guard_mb` is non-zero (`96` by default), the
  reconnect watchdog checks Xray's own resident memory every tick. If it stays above the
  limit for `mem_guard_fails` consecutive checks (`3` by default), the watchdog sends the
  process a `SIGTERM` the same way it does for a dead tunnel, and procd starts a fresh one.
  This shows up in the system log as `xray-watchdog: recycling: RSS stayed over ... MiB for
  ... checks`, and in `/var/run/xray.watchdog.json` as `mem_guard_last` / `mem_guard_fired`.
  It is a symptom being treated, not a fix - see [known issues](../known-issues.md).
- **The error log is bounded.** `/var/log/xray.log` lives on tmpfs and nothing else rotates
  it; the service truncates it on start and the watchdog truncates it in place above
  `logfile_max_kb` (`256` by default) so a long-running connection cannot slowly fill RAM
  with its own log. Access logging is off in the generated configuration for the same reason.
- **A loopback metrics endpoint**, off by default (`metrics_listen` empty). Set it to
  something like `127.0.0.1:18888` to expose Xray's own pprof/metrics while chasing a leak;
  it is never reachable from anything but this router, by validation on the settings API.

None of this is a substitute for the watchdog being enabled - it is what runs the RSS guard.

## Installing

Two packages, and they are deliberately separate. The binary is 34 MB and cannot live in
the router's 6 MiB overlay, so it goes onto the storage shared with the modem half; see
[external storage](external-storage.md).

```sh
opkg update
hh71vm-extern-pkg install xray-core     # 34 MB, onto /mnt/extern
opkg install luci-app-hh71vm-xray       # the page, and the service it needs
```

The second command pulls in `xray`, `kmod-ipt-tproxy` and `iptables-mod-tproxy` by itself.
Log out of LuCI and back in afterwards, or the new menu entry may not appear.

Firmware upgrades clean `xray-core` from the shared storage together with the external
package database. Reinstall it with the two commands above after each upgrade. With the
normal configuration-preserving upgrade, profiles and settings under `/etc` survive;
`sysupgrade -n` discards them too.

## If it does not connect

The page tells you which step failed. The common ones:

| What you see | What it usually is |
|---|---|
| The router's clock is wrong | VMess only. The board has no NTP; connect once and the clock is set from the connection |
| The server does not want the flow / requires it | The `Flow` field must match the server exactly, in both directions |
| The REALITY handshake failed | One of the public key, short id or server name does not match the server |
| The server never answered | The router itself has no internet, or the address or port is wrong |
| The Xray binary is not installed | `hh71vm-extern-pkg install xray-core`, or the shared storage is not mounted |

**Your devices still show your own address after connecting.** Check that they use this
router as their gateway. If this router is a bridge behind another one, they do not, and
their traffic never reaches the rules that would redirect it.

## Reporting a problem

Say what the page showed at which step, what `Diagnostics → Show the log` said, and what
the tunnel is expected to do that it did not. The output of `hh71vm-xrayctl status` on the
router, and `hh71vm-xray-fw status` if traffic is not being captured, contains almost
everything needed.
