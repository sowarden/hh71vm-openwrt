# Changelog

Published images are identified by their immutable Release tag and the SHA-256 digest in
that Release's `SHA256SUMS` asset. Current builds are available from
[GitHub Releases](https://github.com/sowarden/hh71vm-openwrt/releases).

## 2026-08-28

SHA-256:

- `fwupg`: `286e057c3b5b934c24f04d3dcf3edff5e7136c4aec3f726b9e55ea6a8d5db066`
- `sysupgrade`: `3e4527d32a30d76bf42a713280e65b531ca7cb149356b02d4dfe83535b1315e8`
- `nfjrom`: `0dd334f2c05076498bea51668f8ba45ac3fb5651faadfd685c06939d22d8ca52`
- manifest: `ca52e8d864a23ddbc48a98a084478b3b9fa8790c1d66bfbecec2bc8e0a0768cd`

- Set the kernel build identity to `openwrt@build`.
- Add an OpenWrt 19.07 host-tool preflight so the normal libtool package-install path
  installs the MIPS `xtables-legacy-multi` executable instead of its build wrapper.
- Preserve the kernel package ABI `4.14.275-1-2709aa412f796f4f2600f70163b49915`.
- Explicitly select the CA bundle and mbedTLS transport in the image configuration.

## Optional modem tools

- Add `modem-extra-tools` 1.1.0 with a separate LuCI page, persistent TTL/Hop Limit
  controls and LTE band selection based on modem-reported capabilities.
- Include the three matching IPKs and their source packages; select the add-ons
  for package builds without including them in the base firmware image.
- Add checksum- and ABI-checked optional package bundling and publication tests. This
  historical manual path was later replaced by the unified signed per-build feed.

## 2026-08-26

SHA-256:

- `fwupg`: `953b0be76e6166d15863044e26b5fb81483b8a39d21f26f20abf17926f88c9fb`
- `sysupgrade`: `da1fe0269aa8d140a53bfdfcef9af780eba4cc8b84cf5b71e33ed47cba3b6fb9`
- `nfjrom`: `6b839ec1c2b1d7a2ac23550aa1bc24308c7fc71b4eac8a7c374f11e977d47a07`
- manifest: `ddd001d4b2270c28b056c598e99a42535219889655b92cb25affc06923f9db3f`

- Preserve the required `PAD_CTRL_1` RGMII mode bit for the external Ethernet PHY.
- Add multipart SMS reading and PDU-mode submission with GSM 03.38/UCS2 segmentation.
- Port the SPI-NOR driver and add `fwupg` installation, `sysupgrade`, and rollback tools.
- Add LuCI, the HH71VM theme, modem pages, WireGuard packages, and matching target/kernel packages.

## 2026-08-19

Historical RAM-image SHA-256:
`4d4a329edbe034e431a12f4f57aa8c46c4f4fe51a4d1d161a852b6a9134691f7`

## 2026-08-13

Historical RAM-image SHA-256:
`70fe5aeea90e3f2e4ab8a9e1148dac5a504efab8fd717f362257a30cf43acf64`
