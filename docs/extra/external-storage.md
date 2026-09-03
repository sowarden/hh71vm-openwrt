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

## What it is good for

Anything too large for the overlay, and anything you would rather not lose on a firmware
update — the share belongs to the other half of the device, so `sysupgrade` does not touch
it.

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
