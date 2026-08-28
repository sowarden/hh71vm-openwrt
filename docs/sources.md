# Source provenance and build instructions

This repository includes the HH71VM source delta and build configuration used for the
2026-08-28 firmware files.

## Firmware artifacts

| Item | Value |
|---|---|
| Flash installer image | `firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-fwupg.bin` — `286e057c3b5b934c24f04d3dcf3edff5e7136c4aec3f726b9e55ea6a8d5db066` |
| Sysupgrade image | `firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-sysupgrade.bin` — `3e4527d32a30d76bf42a713280e65b531ca7cb149356b02d4dfe83535b1315e8` |
| RAM image | `firmware/openwrt-rtkmipsel-rtl8197f-hh71vm-nfjrom.bin` — `0dd334f2c05076498bea51668f8ba45ac3fb5651faadfd685c06939d22d8ca52` |
| Package manifest | `firmware/openwrt-rtkmipsel-rtl8197f-hh71vm.manifest` |
| Build config | `openwrt-feed/build.config` |
| Build-config SHA-256 | `048cdd8899f554b0821b3365c22e4e1e6a2d8da4ed1a1ae8b29ced28e49d2c4d` |

## Pinned upstream revisions

| Tree | Revision |
|---|---|
| OpenWrt | `1da2e82c1182a3fd681da5760be96821213afadd` |
| LuCI feed | `f25285a6c26e8776f153994704710cb8e51fad91` |
| packages feed | `6df6880800397ab3821572c6c5ad18e300374d9e` |

The build is based on OpenWrt 19.07 and Linux 4.14.275. The target architecture is
`mipsel_24kc`.

## HH71VM delta

`openwrt-feed/` contains:

- the `rtkmipsel/rtl8197f` target and HH71VM board support;
- RTL8197F BSP, UART, PCIe, GPIO, USB-mux, and SPI-NOR integration;
- the Realtek `rtknet` Ethernet/switch port and external-PHY setup;
- the Realtek `rtl8192cd` Wi-Fi port for RTL8197FS and RTL8812FE;
- UCI/netifd integration for both Wi-Fi radios;
- Qualcomm RNDIS WAN and modem-control integration;
- LuCI modem pages, HH71VM theme, and iwinfo compatibility patches;
- image construction scripts and the captured build configuration.

## Vendor-source provenance

The main donor sources were obtained from manufacturer-published GPL source archives:

| Component | Donor source |
|---|---|
| RTL8197F BSP and `rtknet` Ethernet/switch code | TP-Link Archer AX12 Realtek SDK v3.6.0 archive |
| `rtl8192cd` Wi-Fi code with PHYDM/HALMAC | D-Link DIR-842E Realtek SDK v3.4.14b archive |
| OpenWrt integration references | Community RTL8197F/OpenWrt trees, adapted and reviewed for this target |

The radio firmware data files are included in the vendor source trees in the form supplied
by the vendor and were not modified.

## Rebuild the images

The original environment was Ubuntu 22.04 under WSL. OpenWrt 19.07 build scripts still rely
on Python 2 in parts of the configuration flow, so newer distributions may need additional
compatibility work.

Do not inherit Windows search paths into the build. The configuration fixes the kernel
identity to `openwrt@build`.
The CA bundle and mbedTLS transport are explicitly selected to preserve the previous
image's HTTPS dependencies rather than relying on files left in an incremental rootfs.
WireGuard is selected as a separate kernel package (`=m`), not added to the base image.

Compare final SHA-256 values with the published checksums before distributing a build.

```sh
git clone https://git.openwrt.org/openwrt/openwrt.git
cd openwrt
git checkout 1da2e82c1182a3fd681da5760be96821213afadd

./scripts/feeds update packages luci
git -C feeds/packages checkout 6df6880800397ab3821572c6c5ad18e300374d9e
git -C feeds/luci checkout f25285a6c26e8776f153994704710cb8e51fad91
./scripts/feeds install -a
```

From the parent directory containing this repository and the OpenWrt checkout:

```sh
rsync -a --delete \
  hh71vm-openwrt/openwrt-feed/target/linux/rtkmipsel/ \
  openwrt/target/linux/rtkmipsel/

rsync -a \
  hh71vm-openwrt/openwrt-feed/package/ \
  openwrt/package/

sh hh71vm-openwrt/openwrt-feed/scripts/prepare-build-host.sh openwrt
cp hh71vm-openwrt/openwrt-feed/build.config openwrt/.config
cd openwrt
make defconfig
env -i HOME="$HOME" PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  SHELL=/bin/bash LANG=C.UTF-8 make -j"$(nproc)"
```

The build produces all three artifacts under `bin/targets/rtkmipsel/rtl8197f/`:

| Suffix | Used for |
|---|---|
| `-fwupg.bin` | Installing into flash from stock |
| `-sysupgrade.bin` | Updating an already installed system |
| `-nfjrom.bin` | The optional RAM dry run |

Verify them:

The installed `build_dir/target-mipsel_24kc_musl/root-rtkmipsel/usr/sbin/xtables-legacy-multi`
must be a MIPS ELF executable, never a libtool shell wrapper. For an incremental package
repair, run `make package/install` before `make target/install` to refresh the image rootfs.
Inspect the executable again inside both SquashFS images and the RAM initramfs.

```sh
sha256sum bin/targets/rtkmipsel/rtl8197f/*.bin
```

The published RAM-image SHA-256 is:

```text
0dd334f2c05076498bea51668f8ba45ac3fb5651faadfd685c06939d22d8ca52
```

Compare all generated files with [`firmware/SHA256SUMS`](../firmware/SHA256SUMS), not only
the RAM image.

If a clean build differs, preserve both manifests, the final `.config`, tool versions, and
the complete build log before changing source. Do not claim a reproduced build until the
cause of the difference is understood.

## Build and use the kernel packages

The full `make -j"$(nproc)"` command above builds the target's kernel modules and their
dependencies as installable `.ipk` files. They are written to:

```text
bin/targets/rtkmipsel/rtl8197f/packages/
```

After a kernel or `rtl8192cd` source change, rebuild the target before collecting packages:

```sh
make target/linux/compile -j"$(nproc)" V=s
make package/index
```

Keep the entire dependency set together. OpenWrt kernel packages embed an exact kernel ABI
dependency, so a `kmod-*.ipk` from another build must not be mixed with this image. The
published [`packages/`](../packages/) directory contains the matching set and package index.

The optional modem controls are selected as modules in `build.config`, so a normal build also
produces their backend, LuCI application, and kernel-dependent netfilter package. Their source
directories are:

```text
package/utils/modem-extra-tools/
package/luci/applications/luci-app-modem-extra-tools/
package/utils/hh71vm-ipt-ipopt/
```

Actions builds the extras from source. See the [available bundles](../extras/README.md).

For local packaging of the explicitly pinned, published IPKs without duplicating them in Git:

```sh
python3 tools/release/build-package-index.py packages
python3 tools/release/build-package-bundle.py extras/modem-extra-tools
```

The resulting ZIP is written under the ignored `dist/` directory and is intended to be uploaded
as a release asset. See the [bundle README](../extras/modem-extra-tools/README.md) for use and
compatibility boundaries.

When updating that local package snapshot, copy its IPKs to `packages/`, then update the exact
filenames, hashes, architecture, firmware build and kernel dependency in `bundle.json`.
Actions instead derives these fields from its fresh build output. The bundle builder
generates installer variables and refuses mismatched hashes or kernel dependencies;
it never silently picks a package with a similar name. The index builder includes the virtual
`kernel` and `libc` packages already shipped in this repository.

The ARM helper executables are included with their C sources and verified by
`files/helpers.sha256` during the OpenWrt package build. To regenerate them, run
`sh build-helper.sh` in `package/utils/modem-extra-tools/` with
`arm-linux-gnueabi-gcc`, `arm-linux-gnueabi-strip` and a host C compiler available.
Run the backend's mocked tests with `lua5.1 tests/unit.lua files` in that directory.

The supported deployment is described in the [flash installation guide](flash-install.md).
The exact 2026-08-28 binaries still require a final installation and boot check on the
reference device; see [known issues](known-issues.md).

## License map

Files retain their upstream notices and licenses. See [LICENSING.md](../LICENSING.md) for the
repository-level map. This document records provenance and build state; it is not legal
advice.
