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

## Not yet independently reproduced

The included source state, build configuration, base revision, and feed revisions were
captured from the release build. A second clean-checkout build has not yet independently
reproduced every published SHA-256. See [sources and build instructions](sources.md) for the
exact verification targets.
