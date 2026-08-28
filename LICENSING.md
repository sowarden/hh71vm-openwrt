# Licensing map

This repository contains several categories of material. Their licenses are not identical.

## Source and firmware

The OpenWrt/Linux source delta and the resulting firmware contain components under their
respective upstream licenses. Much of the kernel, BSP, Ethernet, and Wi-Fi work is covered by
GPL-2.0. Individual OpenWrt and LuCI packages may carry other compatible licenses; their file
headers and package metadata remain authoritative.

The repository root [`LICENSE`](LICENSE) contains GPL-2.0. It applies to project source that
is marked or distributed under GPL-2.0; it does not replace a more specific upstream license
notice attached to an individual component.

Binary radio firmware files under the vendor driver tree are included in the form supplied
with manufacturer-published source archives. They were not modified. Their status remains
subject to the applicable vendor terms.

Source provenance, pinned revisions, and build instructions are documented in
[`docs/sources.md`](docs/sources.md).

The `modem-extra-tools` application, its LuCI package, package-bundle installer, and
the `tools/release/` utilities are licensed under Apache-2.0 as stated in their
source headers and package metadata. The `hh71vm-ipt-ipopt` module package is GPL-2.0-only.
The bundle includes the [Apache-2.0 license text](extras/modem-extra-tools/LICENSE-APACHE-2.0)
and a copy of the GPL-2.0 license text.

## Documentation and RAM boot tool

Unless a file says otherwise, the repository documentation and `tools/ram_boot.py` are
licensed under
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International](https://creativecommons.org/licenses/by-nc-sa/4.0/).

That means attribution and ShareAlike are required, and commercial use requires separate
permission from the author.

Creative Commons recommends software-specific licenses for code. The existing CC license is
retained here to preserve the repository's stated licensing policy; a future release may
move the RAM boot tool to an explicit software license after an author decision.

## Contributions

Do not submit code copied from an unknown or incompatible source. A contribution should
identify its origin and license when that is not already obvious from the surrounding tree.
By contributing to an existing licensed file, you agree that the contribution can be
distributed under that file's license.

For commercial permission covering the separately licensed documentation or RAM boot tool,
open an issue in the repository.
