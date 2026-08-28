# Optional package bundles

These add-ons extend an already installed HH71VM OpenWrt system. They are not
included in the base firmware image.

| Bundle | Purpose |
|---|---|
| [modem-extra-tools](modem-extra-tools/) | Persistent TTL/Hop Limit controls and LTE band selection through CLI and LuCI |

Download the prepared ZIP from the matching GitHub release. Do not combine a
bundle with a different firmware snapshot: kernel packages require the exact
kernel ABI for which they were built.

All bundles are published together in the
[`extra-tools` release](https://github.com/sowarden/hh71vm-openwrt/releases/tag/extra-tools).
Check the release notes for the compatible firmware build and exact kernel ABI before
installing a bundle.
