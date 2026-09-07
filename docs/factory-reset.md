# Restoring factory settings

Two ways to put every setting back to the state of a freshly installed image. The
firmware itself is not reinstalled or changed by either of them: the installed
version, and the packages that came with it, stay exactly as they are.

Afterwards the router is on the [default network settings](../README.md#default-network-settings):
`192.168.1.1`, no root password, and the default Wi-Fi names and keys.

## Reinstalling the firmware does not do this

This surprises people, so it is worth stating plainly.

Installation writes the kernel and the root filesystem, and both of them end below
`0x600000`. The settings live in a separate `rootfs_data` partition that starts there,
and nothing in the installation touches it. On the first boot the system also keeps an
existing settings partition on purpose, so that an update does not throw away a working
configuration.

The result is that a configuration which locks you out of the router survives
reinstalling the same firmware over it. A broken `/etc/config/network` takes SSH, LuCI
and Ethernet away at the same time, and the file comes back from the settings partition
on the next boot. Use one of the two methods below instead.

## 1. The RESET button

The small recessed button on the bottom of the case.

1. With the router running, press and hold RESET.
2. Keep holding until **the whole front panel starts flashing**.
3. Let go.

The flashing panel means the reset has already been decided. It is not a request to
keep holding, and releasing on it does not cancel anything. The router erases its
settings and restarts by itself; it is back about 45 seconds later.

A short press does nothing at all, so an accidental tap cannot reset or reboot the
router. The button is only read while the system is running: if the router does not
boot, use the second method.

## 2. Over TFTP, from the bootloader

Works when the system does not boot at all, because the bootloader needs nothing but
power and a cable.

```sh
python tools/flash/reset_openwrt.py --dry-run   # show the plan, send nothing
python tools/flash/reset_openwrt.py             # erase the settings
```

Connect the router directly to the computer and give the computer an address on
`192.168.1.0/24`, for example `192.168.1.50/24`. Do not run this through another
router: the bootloader can only answer hosts on its own subnet, and TFTP replies
arrive from a fresh port, which network address translation drops without a helper.

The tool prints its plan before sending anything and stops if the plan would reach
outside the settings partition. It writes only `rootfs_data`:

| Region | |
|---|---|
| `boot`, `hwsetting` | never written, and no flag can add them |
| `kernel`, `rootfs` | not written; the installed firmware stays as it is |
| `rootfs_data` | erased |
| `vendor_jffs2` | not written |

The section it writes does not restart the board. Power the router off and on when it
finishes, without holding any button. The first boot afterwards takes about a minute
longer than usual while the settings partition is cleared; that is not a hang, and
removing power during it leaves the partition half erased.

`--quick` writes one sector instead of the whole partition and lets the first boot
finish the erase. It takes seconds rather than about two minutes, but it needs a
firmware that has that first-boot step, and it has not yet been independently validated
on hardware. The default full erase is the path to use if you are recovering a router
that does not boot.

## Resetting while installing

`tools/flash/install_openwrt_lan.py --reset-settings` adds the same erase to a normal
installation, so the router starts on the defaults of the newly installed firmware
rather than keeping the settings that were there before. Without the flag the existing
settings survive the installation, which is the behaviour described at the top of this
page. This flag has not yet been independently validated on hardware either.

## What a reset removes

Everything stored on the router: the root password, the network configuration, Wi-Fi
names and keys, installed packages, and any file added after installation. Take a
backup first if you want the configuration back:

```sh
ssh root@192.168.1.1 sysupgrade -b /tmp/backup.tar.gz
scp root@192.168.1.1:/tmp/backup.tar.gz .
```
