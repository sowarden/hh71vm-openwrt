# Testing and log collection

Reports help identify which HH71VM hardware variants behave the same and where their board,
radio, switch, or modem configuration differs.

Submit a report after **every complete attempt**, including an uneventful success.

## Before testing

Record these values from the stock firmware or device label before you install anything:

- exact model as printed on the device label;
- stock firmware version;
- purchase country/region and carrier branding, if any;
- board revision printed on the PCB, if visible.

Do not publish the device serial number, full MAC addresses, IMEI/IMSI, SIM identifiers,
phone numbers, messages, credentials, or keys. A report normally needs only whether a MAC
matches the label, not the complete address.

## Evidence required for every report

1. Exact image SHA-256.
2. A log, if your path produces one: the installer output for a flash installation, or the
   complete UART log that `ram_boot.py` saves under `tools/ram-boot-logs/` for a RAM dry
   run. A flash installation without UART has no boot log, which is expected.
3. Result category: booted and worked, booted with differences, or failed.
4. Device model, region, stock version, and visible board revision.
5. The output collected below, for every command the system reached.

Review and sanitize logs before attaching them. Preserve line order and error messages; use
plain-text files rather than screenshots.

## 1. Boot and platform identity

Capture:

```sh
ubus call system board
cat /etc/openwrt_release
cat /proc/cpuinfo
dmesg
logread
```

If boot does not reach a shell and you have UART, report the last line. Do not trim earlier warnings just
because a later stage succeeds.

## 2. Hardware-variation markers

Radio initialization:

```sh
dmesg | grep -iE "PHY_REG_PG|Bonding|RFE type|chip_version"
```

The reference device reported:

```text
[97F] Bonding Type 97FS, PKG1
[97F] RFE type 1 PHY paratemters: GPA0+GLNA0
[GetHwReg88XX][PHY_REG_PG_8197Fmp_Type1]
chip_version=0x100a
```

Any difference is important. Do not describe a different value as a defect by itself; it is
evidence of a potentially different hardware configuration.

Switch mapping:

```sh
dmesg | grep -E "eth[01] added"
cat /proc/rtl865x/port_status
swconfig list
swconfig dev switch0 show
```

The reference build used member masks `0x10f` for `eth0` and `0x110` for `eth1`.

MAC retrieval:

```sh
dmesg | grep hwsetting
ip link show eth0
```

State whether the address matches the device label, but redact the unique portion before
posting. `00:12:34:56:78:96` is the vendor fallback and indicates that the `H601` read did
not supply a device address.

## 3. Ethernet

Check:

- DHCP from the router and access to `192.168.1.1`;
- negotiated link speed;
- ping and sustained traffic;
- unplug/replug behavior;
- `cat /proc/rtl865x/port_status` before and after a cable change.

Compare with stock under the same cable and computer when possible.

## 4. Wi-Fi on both bands

The expected default networks are `HH71VM` on 2.4 GHz and `HH71VM-5G` on 5 GHz, both using
`hh71vm12345`.

For each band, report:

- whether the SSID is visible;
- association and WPA2-AES success;
- DHCP and ping to `192.168.1.1`;
- active traffic for at least 10–15 minutes;
- approximate throughput and stability;
- more than one client, if available;
- any material range difference from stock.

Capture:

```sh
wifi status
iwinfo
dmesg | grep -iE "wlan|rtl8192|8812|97F|RFE"
```

Phones often leave networks that appear to have no Internet. Distinguish client policy from
an actual link drop by keeping traffic active and checking another client when possible.

## 5. Qualcomm modem and mobile Internet

First test without changing modem settings.

Check:

- whether WAN appears as `eth2`;
- whether LuCI reports the modem-control channel as ready;
- whether mobile status is populated;
- Internet access from an Ethernet client;
- Internet access from both Wi-Fi bands.

Capture:

```sh
ip addr show eth2
ip route
logread | grep -iE "hh71vm|modem|rndis|usb"
dmesg | grep -iE "rndis|usb 1-1|eth2"
```

Some modem-control functions change persistent Qualcomm-side settings. Do not change APN,
SIM, or network-mode values unless that is the explicit test, and record the original value.
Never include SMS content, phonebook data, IMEI/IMSI, or SIM identifiers in a public log.

## 6. LuCI

Open `http://192.168.1.1/` and check:

- dashboard and basic status pages;
- network and wireless pages;
- the HH71VM theme in light and dark mode;
- modem overview and read-only status;
- Wi-Fi client list.

Record browser name/version for UI defects. Attach a screenshot only for visual layout bugs;
all console and system logs should remain text.

## 7. Stability

If the basic checks pass, leave traffic active for several hours and then collect:

```sh
uptime
free
dmesg
logread
```

Report kernel oops, memory growth, modem-control disconnects, interface disappearance, and
repeatable timing rather than only saying that something "sometimes fails."

## Submit the report

Use the repository's [issue chooser](https://github.com/sowarden/hh71vm-openwrt/issues/new/choose):

- **Compatibility report** for every hardware test, including success;
- **Bug report** for a reproducible software defect after compatibility data has already
  been provided.

If one run exposes both hardware differences and a bug, start with the compatibility report
and link a separate bug report only when the defect can be described independently.
