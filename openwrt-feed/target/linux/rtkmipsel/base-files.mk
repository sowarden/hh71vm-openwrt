#
# Target hook in package/base-files: it does `-include $(PLATFORM_DIR)/base-files.mk`
# before `BuildPackage`, so here you can override macros that
# base-files collects the image. The file is included only when building base-files.
#
# One macro is redefined here - `FeedSourcesAppend` from include/feeds.mk, which
# writes `/etc/opkg/distfeeds.conf`. The standard option is not suitable for two reasons:
#
#   1. The first line he always writes is core-feed `%U/targets/rtkmipsel/rtl8197f/packages`.
#      There is no such catalog and there never will be: `rtkmipsel/rtl8197f` is our own target,
#      its packages exist only in the built image. The line gave 404 on each
#      `opkg update`.
#   2. `%U` is taken from `VERSION_REPO`, and by default it is equal to
#      `.../releases/19.07-SNAPSHOT` - directory with the same name on downloads.openwrt.org
#      no either. We need a specific release.
#
# Why 19.07.10: versions of basic packages in our tree (branch `openwrt-19.07`,
# commit 1da2e82) match 19.07.10 byte-by-byte by version lines -
# `libubox20191228 2020-05-25-66195aee-1`, `libubus20210603 2022-02-21-b32a0e17-1`,
# `libuci20130104 2019-09-01-415f9e48-4`, `libuclient20160123 2020-06-17-51e16ebf-1`,
# `libnl-tiny 0.1-5`, `busybox 1.30.1-6`, `opkg 2021-01-31-c5dccea9-1`. Verified with
# index `packages/mipsel_24kc/base/Packages.gz` release 19.07.10. Therefore binary
# packages from there are installed to us without a dependency conflict.
#
# Signature: the image contains `openwrt-keyring`, so usign is the signature of the official indexes
# are checked regularly.
#
# HTTPS, not HTTP. In official images 19.07 is HTTP, and the reasoning behind this is sound:
# integrity is ensured by the signature, and feeds do not break if the user deletes TLS. For
# devices that access the network through a cellular operator, it does not work. Verified
# live 2026-08-24 on lifecell: the operator intercepts the simple HTTP and gives it instead
# the requested file is your page (`Packages.sig` came as a HTML document to 306 475 B).
# The substitution is caught by signature verification, but the message is lying - `Signature check failed`
# reads like a problem with the keys, not the channel. By HTTPS the same file arrives intact
# (142 B), `opkg update` gives `Signature check passed` for all five feeds, and
# `opkg install nano` passes. TLS for this purpose is already in the image: `libustream-mbedtls`
# and `ca-bundle` are enabled by the profile. If TLS is eventually demolished, the feed will be repaired in one line
# in `/etc/opkg/distfeeds.conf`.
#
# This is free software, licensed under the GNU General Public License v2.
#

HH71VM_PKG_RELEASE:=https://downloads.openwrt.org/releases/19.07.10

# 1: destination file (macro called from package/base-files/Makefile)
define FeedSourcesAppend
( \
  echo '# Packages for this board come from the upstream 19.07.10 release: the'; \
  echo '# architecture is plain mipsel_24kc and the base package versions in this'; \
  echo '# firmware are the same ones that release was built from.'; \
  echo '#'; \
  echo '# There is deliberately no "core" feed line. The kernel modules for this'; \
  echo '# board belong to target rtkmipsel/rtl8197f, which is not an upstream target,'; \
  echo '# so no server has them -- they exist only inside the image you flashed.'; \
  echo '# Install kernel modules by building a new image, not with opkg.'; \
  echo 'src/gz %d_base $(HH71VM_PKG_RELEASE)/packages/%A/base'; \
  echo 'src/gz %d_luci $(HH71VM_PKG_RELEASE)/packages/%A/luci'; \
  echo 'src/gz %d_packages $(HH71VM_PKG_RELEASE)/packages/%A/packages'; \
  echo 'src/gz %d_routing $(HH71VM_PKG_RELEASE)/packages/%A/routing'; \
  echo 'src/gz %d_telephony $(HH71VM_PKG_RELEASE)/packages/%A/telephony'; \
) >> $(1)
endef
