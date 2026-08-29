# Installing optional packages

Autobuild images include a signed package feed for that exact build. No feed setup
or file transfer is needed on a clean installation.

```sh
opkg update
opkg install luci-app-modem-extra-tools
opkg install luci-app-sms-to-telegram
opkg install luci-proto-wireguard
```

Dependencies, including the matching WireGuard kernel module, are resolved by
opkg. HTTPS requires a working Internet connection and a correct system clock.

After installing a package that adds a LuCI page, log out of LuCI and sign in
again. The active browser session can retain its previous menu tree even after
the router-side index cache is rebuilt. A router reboot is not required.

## Offline installation over LAN

If mobile service is already restricted, use another computer to download
`packages-bundle.zip` from the immutable Release that exactly matches the installed
firmware. The complete flash bundle contains firmware and recovery tools, but currently
does not contain this optional package bundle.

Check the installed release identity before copying anything:

```sh
cat /usr/share/hh71vm-feed/release.conf
```

Extract `packages-bundle.zip` on the computer, create a temporary directory on the router,
and copy only the required IPKs over the LAN with legacy SCP mode. Keep the IPK filenames
unchanged. Then install the complete local set in one opkg transaction:

```sh
opkg install \
  /tmp/hh71vm-offline/libuci-lua_*.ipk \
  /tmp/hh71vm-offline/kmod-hh71vm-ipt-ipopt_*.ipk \
  /tmp/hh71vm-offline/iptables-mod-ipopt_*.ipk \
  /tmp/hh71vm-offline/modem-extra-tools_*.ipk \
  /tmp/hh71vm-offline/luci-app-modem-extra-tools_*.ipk
```

For SMS forwarding, install the following files from the same extracted bundle. The HTTPS,
JSON, ubus and CA dependencies are already present in the matching base image:

```sh
opkg install \
  /tmp/hh71vm-offline/libuci-lua_*.ipk \
  /tmp/hh71vm-offline/sms-to-telegram_*.ipk \
  /tmp/hh71vm-offline/luci-app-sms-to-telegram_*.ipk
```

Configuration and delivery semantics are documented in [SMS to Telegram](sms-to-telegram.md).

Stop if the Release tag does not match the installed image or opkg reports a kernel ABI or
dependency error. Do not use `--force-depends`, install IPKs from another Release, or run a
global `opkg upgrade`. Keeping the tools optional avoids placing IMEI/NV write helpers in
the base firmware for users who do not need them.

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
