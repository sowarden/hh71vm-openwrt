# Firmware releases and stable download links

Firmware, package indexes, packages, checksums, and source archives are published together
as immutable [GitHub Releases](https://github.com/sowarden/hh71vm-openwrt/releases). Each
production build has a unique tag and remains available for devices that still run it.

New tags place the zero-padded workflow run and attempt before the source commit prefix. They are
designed to remain chronological when a GitHub view falls back to tag-name ordering. Firmware and
feed tools continue to accept the original commit-first tags used by existing Releases.

Production Releases are treated as permanent distribution records, not expiring GitHub Actions artifacts.
The workflow deletes only its temporary transfer artifact after successful publication; it does
not delete a production Release or any of its assets. Keep every production Release available
while its firmware may still be installed on a device.

## Latest production build

GitHub maintains these stable links. They follow the newest production Release, so the
documentation does not need a new build tag after every publication:

| File | Download |
|---|---|
| Release page and notes | [Latest Release](https://github.com/sowarden/hh71vm-openwrt/releases/latest) |
| Complete firmware and flashing tools | [`flash bundle`](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/hh71vm-openwrt-flash-bundle.zip) |
| Flash bundle checksum | [`flash bundle SHA-256`](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/hh71vm-openwrt-flash-bundle.zip.sha256) |
| Install from stock | [`fwupg`](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin) |
| Update an installed OpenWrt system | [`sysupgrade`](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/openwrt-rtkmipsel-rtl8197f-hh71vm-sysupgrade.bin) |
| Optional RAM boot | [`nfjrom`](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/openwrt-rtkmipsel-rtl8197f-hh71vm-nfjrom.bin) |
| Checksums for every Release asset | [`SHA256SUMS`](https://github.com/sowarden/hh71vm-openwrt/releases/latest/download/SHA256SUMS) |

The flash bundle is the normal download for a new installation. It contains all three images,
the supported installation and recovery tools, the relevant documentation, and its own exact
file manifest. The direct links above are intentionally floating convenience links for a person
downloading new firmware. Record the resolved Release tag and SHA-256 before installing or
reporting a result. Use the exact tagged Release when reproducing an older result.

## Package feed identity

The package feed does **not** use `latest`. Every image contains the public signing key and
the immutable Release URL for its own build. This keeps kernel modules and other ABI-bound
packages matched to the installed firmware even after a newer Release is published.

Run `opkg update` on the router and install packages normally. Do not replace the generated
feed URL with a floating link, copy packages between Releases, use `--force-depends`, or run
a global `opkg upgrade`.

## Source and reproducibility assets

Every Release also carries `release.json`, `source-lock.json`, `build.config`,
`source-delta.tar.gz`, `upstream-buildsystem.tar.gz`, `upstream-sources.tar.gz`, the complete
signed package index, and `packages-bundle.zip`. These assets belong to that exact build and
are not duplicated in the Git repository.
