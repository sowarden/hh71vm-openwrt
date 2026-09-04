# Known issues

Do not file a new bug for an item already listed here unless your result adds materially
different evidence.

Items are grouped by what they apply to. Artifact-level verification is kept separate from
the behavior already observed on the reference device.

## Security limitations

- **No root password is set.** Set one at the first login. Until you do, SSH and LuCI must
  not be reachable from an untrusted network.
- **Both Wi-Fi networks use the public key `hh71vm12345`.** Change both before use.
- **Some vendor procfs controls still have overly broad permissions.** Reducing permissions
  on entries that can expose key material is outstanding.
- **Logs can contain unique identifiers.** Redact MAC addresses, serial numbers, IMEI/IMSI,
  SIM data, phone numbers, messages, credentials, and keys before posting.

## Hardware coverage

The port has been validated on one HH71VM. Other HH71VM board revisions, regional
variants, RF front ends, carrier variants, and flash layouts remain unverified.

The build selects these reference-unit values:

- SoC radio: `SOC_RFE_TYPE_1`;
- PCIe 5 GHz radio: `RTL8812FE`, slot 0, `SLOT_0_RFE_TYPE_0`;
- external gigabit PHY on switch port 0, MDIO address 6.

A unit with different hardware may boot but have reduced radio performance, unstable links,
or no networking. Report marker differences.

## Installing and updating

- The installer writes the kernel and root filesystem only. The bootloader, `hwsetting`,
  vendor configuration, and vendor JFFS2 partition are not written.
- The first boot after a fresh installation takes about two minutes while the settings
  partition is erased. It is not a hang.
- For the first minute or two after a fresh installation, SSH can refuse the empty root
  password while the settings partition is still being built. LuCI accepts you during the
  same period, and the problem clears by itself.
- A `sysupgrade` that keeps settings also keeps files you changed on the running system, and
  those copies continue to take precedence over the versions in the new image. Use
  `sysupgrade -n` when you want the image contents exactly as built.
- Rollback uses the backup taken during installation. A Realtek dump from another HH71VM may
  in theory work as well, but this has not been verified.

## Modem and LuCI limitations

- Modem control-channel preparation has occasionally taken roughly 3-9 minutes.
- **SMS delivery depends on the mobile service.** Message composition, encoding, and multipart
  segmentation are implemented, but delivery confirmation depends on the modem, SIM, and
  carrier service.
- A message the network refuses takes the modem about four minutes to report, and the modem
  pages stall for that time because the single AT channel is busy with it. The channel
  survives, and the failure is written to the system log.
- Long-term reconnect behavior after a Qualcomm-side restart is not yet validated.
- Some radio/scan signal and mode fields in the UI still need audit or clearer handling.
- Settings changed through the modem pages act on the separate Qualcomm subsystem and may
  persist there. Rebooting or reinstalling the Realtek side is not guaranteed to undo them.
- An IMEI restore verifies the Qualcomm NV 550 write by reading that NV item back. A fresh
  `ATI` query refreshes the main Modem overview, but neither result proves that the running
  modem or mobile network is using the restored identity. After a successful restore, fully
  shut down the router, disconnect power, and then power it on again. The implemented
  OpenWrt reboot path resets the Realtek SoC and has no confirmed Qualcomm reset step, so
  it is not a substitute for this power cycle.
- OpenWrt and `modem-extra-tools` do not remove a carrier, SIM, subsidy, or network lock.
  IMEI restoration, TTL normalization, LTE band preferences, SIM PIN handling, and carrier
  unlocking are separate operations. This project has no verified carrier-unlock method.

## Wi-Fi encryption on older images

Older images can offer WEP for the custom `rtl8192cd` radios even though their netifd handler
rejects it. Applying WEP can leave the radio down. The supported choices are an open network
or WPA2-PSK with CCMP/AES; `wpa2` by itself means WPA2-Enterprise and is not a substitute.

Until an image containing the LuCI capability fix is installed, the following recovery can
be applied through SSH to each affected `wifi-iface`. This procedure follows the current
generator and netifd contract but has not yet been independently validated on hardware:

```sh
uci set wireless.default_radio0.encryption='psk2+ccmp'
uci set wireless.default_radio0.key='REPLACE_WITH_8_TO_63_ASCII_CHARACTERS'
uci set wireless.default_radio1.encryption='psk2+ccmp'
uci set wireless.default_radio1.key='REPLACE_WITH_8_TO_63_ASCII_CHARACTERS'
uci commit wireless
wifi reload
```

Confirm the actual section names before running the commands; `default_radio0` and
`default_radio1` are factory defaults, not a universal promise. Do not change the radio
device type, install hostapd/wpad as a workaround, or select `mac80211`/`broadcom`.

## Installing packages over the mobile link

When the only uplink is the mobile connection, some carriers rewrite plain HTTP, which breaks
`opkg` in a misleading way: the reported failure is `Signature check failed`, as though the
signing keys were wrong. Use HTTPS package feeds on such a connection.

## Network-driver quirks

`ip link` may not show Ethernet carrier correctly because the vendor driver does not update
the standard carrier state. Use:

```sh
cat /proc/rtl865x/port_status
cat /proc/eth0/link_status
```

The switch is named `switch0`:

```sh
swconfig list
swconfig dev switch0 show
```

Some vendor Wi-Fi proc counters are incomplete. In particular, `/proc/wlanX/sta_info` and
several fields under `/proc/wlanX/stats` may not reflect active traffic reliably. Prefer
`iwinfo`, DHCP leases, ARP entries, and an actual traffic test.

The two radio interfaces currently use the same default MAC behavior. Record observations,
but redact unique addresses in public reports.

## SSH and file transfer

The bundled Dropbear offers the legacy `ssh-rsa` host-key algorithm:

```text
ssh -o HostKeyAlgorithms=+ssh-rsa root@192.168.1.1
```

If a previous session used another host key:

```text
ssh-keygen -R 192.168.1.1
```

The image has no SFTP server. Use legacy SCP mode, with the same host-key option:

```text
scp -O -o HostKeyAlgorithms=+ssh-rsa local-file root@192.168.1.1:/tmp/
```

## Xray VPN (experimental)

Shipped early on purpose. Read [Xray VPN](extra/xray-vpn.md) before installing it.

- **Only clients that route through this router are captured.** If the router is wired as a
  dumb access point, with its LAN bridged to another router that hands out the addresses,
  its clients belong to that other router: their traffic never passes through here and
  nothing can redirect it. The page will say connected, and devices will still show their
  own address. A router serving its own DHCP - on the SIM or behind a WAN cable - captures
  its clients normally.
- **UDP other than DNS is not carried through the tunnel**, and capturing it is off by
  default. Xray reads the captured packet, tunnels it, the far end answers, and the reply is
  never written back on this hardware, so the traffic would vanish rather than go out
  unproxied. QUIC is rejected instead so browsers fall back to TCP, and DNS is tunnelled
  through `dnsmasq`. There is a switch to try it anyway; expect it not to work.
- **IPv6 is not tunnelled** and forwarded IPv6 to global addresses is rejected while VPN
  mode is on, so that it cannot leak around the tunnel.
- **`geoip.dat` and `geosite.dat` are not installed**, so any routing rule naming `geoip:`
  or `geosite:` fails at start. They are about 20 MB against about 21 MB free on the shared
  storage. Split routing is therefore unavailable.
- **The tunnel is CPU-bound at about 12 Mbit/s** with REALITY, about 27 for VMess over
  plain TCP. That is the processor, not the connection.
- **VMess needs the clock.** The board has no working NTP. Connecting sets the clock from
  the connection, but a router that has never connected and has a clock hours out will fail
  VMess with `invalid user`, which names the wrong thing.
- **The tunnel over the mobile connection is untested.** Everything was measured with the
  router's uplink on a cable. Nothing in the design depends on which interface the default
  route uses, but that is not the same as having run it.

## Not yet independently reproduced

The included source state, build configuration, base revision, and feed revisions were
captured from the release build. A second clean-checkout build has not yet independently
reproduced every published SHA-256. See [sources and build instructions](sources.md) for the
exact verification targets.
