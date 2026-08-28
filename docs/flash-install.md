# Install OpenWrt into flash

This is the normal way to use this firmware. After installation the router boots OpenWrt by
itself and keeps its settings across power cycles.

The procedure below writes only the kernel and the root filesystem. The bootloader, the
`hwsetting` area that holds your MAC addresses, the vendor configuration, and the vendor
JFFS2 partition are not written. That is what makes the rollback in this guide possible.

> [!IMPORTANT]
> Take the backup. Step 2 saves the original flash contents of your router to your computer,
> and that copy is what puts your stock firmware back if you decide to return to it. A
> Realtek dump from another HH71VM may in theory work for recovery as well, but this has not
> been verified.

## What you need

| Item | Requirement |
|---|---|
| Computer | Windows, Linux, or macOS with Python 3 |
| Network | Ethernet cable from the computer directly to a router LAN port |
| Computer address | Static `192.168.1.50`, mask `255.255.255.0` |
| Firmware image | `openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin` |
| Free disk space | About 16 MiB for the stock backup |
| UART adapter | **Not required** |

Disconnect or disable any other interface on `192.168.1.0/24` so that the transfers cannot go
to the wrong adapter.

Clone or download this repository, then open PowerShell, Command Prompt, or a terminal in its
top-level directory. All commands below are written for that directory; keep the files in their
repository subdirectories.

### Host firewall

The tools transfer files over TFTP, and the router replies from a port other than 69. Allow
**inbound UDP** for the Python interpreter you will run. On Windows this is a rule for the
specific `python.exe`; a rule created for a different interpreter will not apply.

The backup step makes the router send its flash contents to your computer, so your computer
acts as the TFTP server. Without the inbound UDP rule the backup will time out.

## 1. Verify the image

Windows PowerShell:

```powershell
Get-FileHash -Algorithm SHA256 firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin
```

Windows Command Prompt:

```bat
certutil -hashfile firmware\openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin SHA256
```

Linux or macOS:

```sh
sha256sum firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin
```

Compare the result with [`firmware/SHA256SUMS`](../firmware/SHA256SUMS). Stop if it differs.

## 2. Back up your stock firmware

The installer performs the backup itself as its first step, from the running stock firmware,
and writes it to the directory you name. Keep that directory, and copy it somewhere else as
well.

The backup is read over Telnet and TFTP from the stock Realtek system, so for this step the
router must be running its original firmware and reachable at its normal address, with Telnet
enabled. Follow [the stock Telnet access guide](telnet-access.md) before starting the backup.
The installer connects to port `2323` by default, matching the primary method in that guide.
If you used its marker-only fallback on the default Telnet port, add `--telnet-port 23` to
the dry-run and installation commands.

## 3. Install

Run the dry run first. It checks the image, builds the write plan, saves or verifies the stock
backup over Telnet, and stops before entering the bootloader or writing flash:

```text
python tools/flash/install_openwrt_lan.py --image firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin --backup-dir backup-stock --dry-run
```

To validate only the local image and write plan without contacting the router, explicitly skip
the backup in dry-run mode:

```text
python tools/flash/install_openwrt_lan.py --image firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin --dry-run --skip-backup --yes --pc-ip 192.168.1.50
```

This command does not transfer anything to the router. Do not carry `--skip-backup --yes` over
to the real installation unless you accept the warning below.

If the dry run reports no problem, run the installation. It verifies and reuses the backup
already stored in `backup-stock`; it does not download the partitions again:

```text
python tools/flash/install_openwrt_lan.py --image firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin --backup-dir backup-stock
```

The tool then:

1. verifies the image and builds the write plan;
2. saves the 16 MiB stock backup into `backup-stock`, or verifies the existing copy;
3. asks you to put the router into its bootloader console;
4. waits until the bootloader answers on the network;
5. writes the root filesystem, then the kernel;
6. lets the board restart itself after the kernel is written.

When the tool asks for the bootloader console:

1. disconnect router power;
2. press and hold the WPS button;
3. apply power while still holding WPS;
4. keep holding it for about 12 seconds, then release.

The bootloader brings Ethernet up on `192.168.1.6` by itself. The tool waits for it to answer
and stops without sending a single byte if it does not, so a missed button press is not
dangerous.

Do not interrupt the tool between the two sections. The bootloader is single-threaded: after
it accepts a section it goes away to write flash and answers nothing at all for tens of
seconds. The tool waits for that on its own.

> [!CAUTION]
> `--skip-backup` exists and requires you to type a confirmation phrase. Use it only when you
> already have a stock backup of **your own Realtek part**, or consciously accept having none:
>
> ```text
> python tools/flash/install_openwrt_lan.py --image firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin --skip-backup
> ```
>
> Skipping the backup avoids the Telnet prerequisite, but is strongly discouraged. Do not use
> a donor Realtek image: it can contain another unit's MAC addresses and regional or carrier
> settings, and may not match your hardware.

## 4. First boot

The first boot after installation takes about two minutes. During it the system erases the
settings partition, which still holds part of the old stock root filesystem. This is normal
and it is not a hang. Later boots are fast.

For the first minute or two after a fresh installation, SSH may refuse the empty root password
while the settings partition is still being built. LuCI accepts you during the same period.
It resolves by itself.

## 5. Check the result

```text
ping 192.168.1.1
ssh -o HostKeyAlgorithms=+ssh-rsa root@192.168.1.1
```

LuCI is at `http://192.168.1.1/`.

If the browser shows `Bad Request` or the old vendor page, it still has the vendor interface
cached for that address. Reload, or clear site data for `192.168.1.1`.

On the router:

```sh
cat /etc/openwrt_release
cat /proc/mtd
ubus call hh71vm-modem status
```

Set a root password and change both Wi-Fi keys before connecting anything else. The defaults
are published in the README.

Then work through the [testing guide](testing.md) and send a compatibility report.

## Updating an installed system

Use the OpenWrt updater on the device. No installation tool and no button press is needed.

The image has no SFTP server, so copy the file with legacy SCP mode. Dropbear only offers
the SHA-1 `ssh-rsa` host key, which current OpenSSH releases do not accept by default, so
`scp` needs the same option as `ssh`:

```text
scp -O -o HostKeyAlgorithms=+ssh-rsa firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-sysupgrade.bin root@192.168.1.1:/tmp/
```

Then, on the router:

```sh
sysupgrade -v /tmp/openwrt-rtkmipsel-rtl8197f-hh71vm-sysupgrade.bin
```

Settings are kept. To install a clean system instead, add `-n`:

```sh
sysupgrade -n -v /tmp/openwrt-rtkmipsel-rtl8197f-hh71vm-sysupgrade.bin
```

## Rolling back to your stock firmware

This uses the backup from step 2 and the same button-and-network path as the installation. No
UART is needed.

```text
python tools/flash/restore_stock_lan.py --backup-dir backup-stock --dry-run
python tools/flash/restore_stock_lan.py --backup-dir backup-stock
```

The tool restores the stock kernel, root filesystem, and the stock data that occupied
`rootfs_data`. The bootloader, MAC area, vendor configuration, and vendor JFFS2 were never
overwritten by OpenWrt, so the tool deliberately leaves those original bytes in place.

The restore sections do not restart the board. After the tool finishes, disconnect and reconnect
power **without holding WPS**. The router then boots the restored stock firmware. Nothing opens a
terminal for you; use the stock interface as you did before.

## If you have a UART adapter

UART is not required for anything above. These alternatives exist and were used during
development:

| Situation | Tool |
|---|---|
| Install from a running stock system, over its serial console | `flash_openwrt_vendor.py` |
| Install when no working system is left on the device | `flash_openwrt_tftp.py` |
| Roll back with the serial console available | `restore_stock.py` |

Each of them prints its plan before touching the port, and each supports `--dry-run`. The
serial console is `38400 8N1` on the **Realtek** side; see the
[RAM boot guide](installation.md) for the pinout photo and wiring rules.

Close every other program using the serial port first. Only one program can own it.

## If something goes wrong

**The tool never saw the bootloader.** The router is untouched: the tool stops before sending
anything. Power off, hold WPS for the full 12 seconds while applying power, and check that
your computer is on `192.168.1.50/24` with no other adapter on that subnet.

**The installation stopped partway.** The bootloader console is reachable the same way at any
time, because it lives in a region that is never written. Repeat the installation, or run
`tools/flash/restore_stock_lan.py` with your backup.

**The router does not boot at all.** Use the same WPS-at-power-on path with your backup and
`tools/flash/restore_stock_lan.py`. It does not need a working system on the device.

**Nothing works and there is no backup.** Do not use a dump from another device. Recovery then
requires an SPI flash programmer and a verified full-chip image from the same unit. This is why
step 2 exists.
