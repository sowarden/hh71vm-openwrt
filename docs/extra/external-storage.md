# External storage: /mnt/extern

The writable part of the Realtek flash — the overlay that holds everything `opkg` installs —
is 6 MiB. That is enough for the packages this port ships and not much else.

The Qualcomm half of the router owns a separate 55 MiB partition and exports it over SMB to
this side only. The base firmware mounts it at `/mnt/extern` and can install packages onto
it. The stock Realtek firmware mounted the same share, so this is not a new arrangement — it
is the one the device was built with.

Nothing has to be installed. The mount is part of the base image.

## Using it

```sh
hh71vm-extern-mount status          # is it mounted, and how much space is left
hh71vm-extern-reset status          # is first-boot cleanup still pending
hh71vm-extern-pkg   status
hh71vm-extern-pkg   space
hh71vm-extern-pkg   list            # what is installed over there
hh71vm-extern-pkg   install <package|file.ipk>
hh71vm-extern-pkg   remove  <package>
```

`hh71vm-extern-pkg install` is the wrapper you want rather than `opkg install -d extern`
directly. It mounts the share first, and it checks that the package actually fits before
starting — `opkg`'s own check is against `Installed-Size`, which on OpenWrt 19.07 is the size
of the *compressed* payload and can undercount by a factor of three. The difference is
between a clean refusal and a share that fills up half way through.

It also puts the share's `bin` directories on `PATH`: `/etc/profile.d/hh71vm-extern.sh` does
that for login shells, and `/etc/init.d/hh71vm-extern-services` does it for services.

## Firmware upgrades

External packages are installation state, just like packages installed in the normal
OpenWrt overlay. A successful `sysupgrade` starts with an empty package area even though
the underlying Qualcomm storage survives the upgrade.

On the first boot of every new image, cleanup is marked as pending. Once the expected
Qualcomm CIFS share is mounted, the firmware removes these fixed OpenWrt-owned directories:

- `opkg` - external package files and their package database;
- `bin` - the legacy location used before external opkg support;
- `control` and `xray-loopback-test` - old Xray development artifacts;
- `xray` - optional Xray data files.

It then creates an empty `opkg` directory and publishes the external destination again.
Files and directories elsewhere under `/mnt/extern`, including Qualcomm data, are not
scanned or removed. Cleanup refuses unexpected mounts, nested mounts, symbolic links or
unexpected file types, and remains pending for the next mount attempt. An interrupted
cleanup is safe to repeat.

If the share is late or unavailable, package installation remains disabled until cleanup
finishes. Check it with `hh71vm-extern-reset status` and retry the mount with
`hh71vm-extern-mount mount`. Afterward, reinstall any wanted external package normally:

```sh
opkg update
hh71vm-extern-pkg install xray-core
```

A normal reboot does not clean external packages. Reinstalling the same firmware with
`sysupgrade` does, because the overlay and its package state are new again. Configuration
preserved by sysupgrade remains under the normal `/etc` paths. Files placed inside the
external `opkg` destination, including destination-local configuration files, are package
payload and are removed. `sysupgrade -n` also discards the normal saved configuration.

This cleanup is why the external area is suitable for large installable programs, rather
than files that must survive a firmware replacement.

## Configuration

`/etc/config/hh71vm-extern`:

```
config mount 'extern'
	option enabled '1'
	option share   '//192.168.225.1/shared_rom'
	option target  '/mnt/extern'
	option options 'guest,sec=none,vers=1.0,nodev,nosuid,noatime'
	option tries   '10'
	option delay   '3'
	option opkg_dest '1'
```

Two of those defaults are deliberate and should not be changed casually:

- **`vers=1.0`** is mandatory, not a preference. The other side is Samba configured with
  `security = SHARE`, which speaks SMB1 only, while Linux 4.14 defaults to SMB2.1 and above.
  Without it the mount fails with `Host is down`.
- **there is no `noexec`.** The entire point of the space is to run from it what does not fit
  in the overlay. The stock firmware did use `noexec`; this port does not.

`tries` and `delay` exist because the share only appears once the Qualcomm half's `smbd` is
up, which can be later than our boot. The mount retries in the background rather than failing.

Set `enabled` to `0` to turn the whole thing off.

## Limits

- The share is on the other half of the device. If that half is restarted or its `smbd`
  stops, the mount goes away with it and anything running from there stops.
- It is SMB1 over an internal link, not local storage. It is fine for program files and
  data; it is not the place for something latency-sensitive.
- Services installed there need an init script that waits for the share, because
  `hh71vm-extern-services` starts them after the mount helper has had its turn but the mount
  itself can still be late.
