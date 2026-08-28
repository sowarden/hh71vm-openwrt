# Optional package bundles

This directory preserves the manual bundle recipe for the historical firmware
snapshot. New autobuild images use the [signed opkg feed](../docs/package-feed.md).

These add-ons extend an already installed HH71VM OpenWrt system. They are not
included in the base firmware image.

| Bundle | Purpose |
|---|---|
| [modem-extra-tools](modem-extra-tools/) | Persistent TTL/Hop Limit controls and LTE band selection through CLI and LuCI |

Download the prepared ZIP from the matching GitHub release. Do not combine a
bundle with a different firmware snapshot: kernel packages require the exact
kernel ABI for which they were built.

The former rolling `extra-tools` publication workflow is retired. Existing ZIPs
remain usable only with their documented firmware build and exact kernel ABI.
