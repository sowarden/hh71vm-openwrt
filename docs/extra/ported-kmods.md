# Kernel modules

The list is taken from `openwrt-feed/build.config`, the captured build configuration for this
port. Names only; for what a module does, see the upstream OpenWrt package of the same name.

## Built into the image

Present on a fresh installation, nothing to install.

Wireless and Ethernet:

- `kmod-rtl8192cd`
- `kmod-lib-crc-ccitt`

Netfilter and routing:

- `kmod-ipt-core`
- `kmod-ipt-conntrack`
- `kmod-ipt-nat`
- `kmod-ipt-offload`
- `kmod-ip6tables`
- `kmod-nf-conntrack`
- `kmod-nf-conntrack6`
- `kmod-nf-flow`
- `kmod-nf-ipt`
- `kmod-nf-ipt6`
- `kmod-nf-nat`
- `kmod-nf-reject`
- `kmod-nf-reject6`

PPP:

- `kmod-ppp`
- `kmod-pppoe`
- `kmod-pppox`
- `kmod-slhc`

Filesystem and crypto, for the [external storage](external-storage.md) mount:

- `kmod-fs-cifs`
- `kmod-nls-base`
- `kmod-crypto-aead`
- `kmod-crypto-des`
- `kmod-crypto-ecb`
- `kmod-crypto-hash`
- `kmod-crypto-hmac`
- `kmod-crypto-manager`
- `kmod-crypto-md4`
- `kmod-crypto-md5`
- `kmod-crypto-null`
- `kmod-crypto-pcompress`
- `kmod-crypto-sha256`

## In the signed feed

Install with `opkg` from the feed belonging to your exact firmware build; see
[installing optional packages](../package-feed.md).

- `kmod-hh71vm-ipt-ipopt` — see [IP option modules](ipt-ipopt.md)
- `kmod-wireguard`
- `kmod-fuse`
- `kmod-fs-nfs`
- `kmod-fs-nfs-v3`
- `kmod-fs-nfs-v4`
- `kmod-gre`
- `kmod-ipip`
- `kmod-l2tp`
- `kmod-l2tp-eth`
- `kmod-l2tp-ip`
- `kmod-pppol2tp`
- `kmod-pptp`
- `kmod-mppe`

A module from another Release will not load: it is checked against the kernel ABI of the
firmware it was built for. Do not use `--force-depends` or foreign kmods.
