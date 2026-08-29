# Installing optional packages

Autobuild images include a signed package feed for that exact build. No feed setup
or file transfer is needed on a clean installation.

```sh
opkg update
opkg install luci-app-modem-extra-tools
opkg install luci-proto-wireguard
```

Dependencies, including the matching WireGuard kernel module, are resolved by
opkg. HTTPS requires a working Internet connection and a correct system clock.

After installing a package that adds a LuCI page, log out of LuCI and sign in
again. The active browser session can retain its previous menu tree even after
the router-side index cache is rebuilt. A router reboot is not required.

## Firmware updates

After sysupgrade, the image restores its own feed URL and public key. Recognized
older HH71VM entries and cached indexes are removed; third-party feeds are kept.
Changed configuration files are backed up under `/etc/hh71vm-feed/backups/`.
This applies to both the first feed-enabled image and later firmware updates.

The URL contains an immutable release tag, never `latest`. Older images continue
using their original packages while their firmware release remains available.
A package fix is delivered with a new firmware/feed Release; it does not replace
files in an existing release.

Sysupgrade does not automatically restore optional packages. Reinstall the ones
you use with `opkg install` after updating. Firmware itself is never updated
automatically by the build service.

## If the feed is disabled

The migration refuses a kernel mismatch, an invalid key, a reserved feed-name
collision, or settings that disable signature checking. Review the message in
the system log and the saved configuration. After correcting the cause, run:

```sh
/usr/libexec/hh71vm-feed-reconcile
opkg update
```

Do not disable signature checks, force dependencies, install foreign kmods, or
run a global `opkg upgrade`. Do not copy an old `/etc/opkg/hh71vm.conf` into a newer
image. Explicitly relocating opkg's list directory requires configuration review.

Firmware downloads and the distinction between floating human-facing links and
the immutable feed URL are documented in [Firmware Releases](releases.md).
