# Copyright 2026 sowarden
# Licensed to the public under the Apache License 2.0.
#
# Xray, built from source with a Go toolchain that is corrected twice on the way.
# Neither correction is optional and neither can be left to whoever builds this next:
# without them the binary either refuses to start (_ENOSYS is 89 on MIPS, not 38, so
# Go's futex_time64 fallback never fires on this 4.14 kernel) or panics the board
# (writing to an eventfd registered in an epoll set corrupts kernel memory here).
# src/patch-go-toolchain.py applies both and fails the build if either context moved.
#
# The result is 34 MB and the rootfs partition has about 100 KB of slack, so this is a
# feed-only package and it belongs on the /mnt/extern share:
#
#     hh71vm-extern-pkg install xray-core
#     opkg install xray
#
# The service half is a separate, small package on purpose. If it depended on the
# binary, `opkg install xray` would drag 34 MB into a 6 MiB overlay.

include $(TOPDIR)/rules.mk

PKG_NAME:=xray-core
PKG_VERSION:=26.3.27
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/XTLS/Xray-core/tar.gz/v$(PKG_VERSION)?
PKG_HASH:=992a4997e6bb846d11469435d687f99ef812fcde1e0a009bb8e95189ea20331d
PKG_BUILD_DIR:=$(BUILD_DIR)/Xray-core-$(PKG_VERSION)

PKG_LICENSE:=MPL-2.0
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=sowarden <sowarden@proton.me>

# Xray v26 declares `go 1.26` in go.mod, and 1.26 is also where the _ENOSYS bug lives,
# so an older toolchain is not a way around it. Binary distributions are used rather
# than building Go from source: patching runtime sources is enough because a cross
# build compiles the runtime for the target from source in every case.
GO_VERSION:=1.26.1
GO_HOST_ARCH:=$(if $(filter aarch64 arm64,$(shell uname -m)),arm64,amd64)
GO_TARBALL:=go$(GO_VERSION).linux-$(GO_HOST_ARCH).tar.gz
GO_HASH_amd64:=031f088e5d955bab8657ede27ad4e3bc5b7c1ba281f05f245bcc304f327c987a
GO_HASH_arm64:=a290581cfe4fe28ddd737dde3095f3dbeb7f2e4065cab4eae44dfc53b760c2f7
GO_HASH:=$(GO_HASH_$(GO_HOST_ARCH))

GOROOT_DIR:=$(PKG_BUILD_DIR)/.goroot

# The module cache lives OUTSIDE the package build directory, for two reasons. Go marks
# everything it writes there read-only, so `rm -rf $(PKG_BUILD_DIR)` fails and takes
# `make package/xray-core/clean` down with it - that is why `go clean -modcache` exists.
# And keeping it means a rebuild does not fetch a few hundred modules again.
GO_WORK_DIR:=$(BUILD_DIR)/xray-core-gocache

# Both stages write a full log beside the build directory and print its tail on
# failure. The top-level build runs with -j6, which interleaves and truncates output
# to the point where the real error is unreadable - and a Go build that pulls a few
# hundred modules is exactly where that matters.
PREPARE_LOG:=$(BUILD_DIR)/xray-core-prepare.log
COMPILE_LOG:=$(BUILD_DIR)/xray-core-compile.log

include $(INCLUDE_DIR)/package.mk

define Download/golang
  FILE:=$(GO_TARBALL)
  URL:=https://go.dev/dl/
  HASH:=$(GO_HASH)
endef
$(eval $(call Download,golang))

# `go build -ldflags "-s -w"` has already removed the symbol table. Running the target
# strip over a Go binary buys nothing and has historically broken them.
RSTRIP:=:

define Package/xray-core
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=Web Servers/Proxies
  TITLE:=Xray core binary (34 MB, meant for the /mnt/extern share)
  URL:=https://github.com/XTLS/Xray-core
  DEPENDS:=@TARGET_rtkmipsel
endef

define Package/xray-core/description
  The Xray proxy binary for this board, built from source with the two Go runtime
  corrections this hardware needs. At 34 MB it does not fit the 6 MiB overlay: install
  it with `hh71vm-extern-pkg install xray-core`, which puts it on the 55 MiB share.
endef

define Package/xray
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=Web Servers/Proxies
  TITLE:=Xray service, configuration and init script
  URL:=https://github.com/XTLS/Xray-core
  DEPENDS:=@TARGET_rtkmipsel
endef

define Package/xray/description
  Init script, UCI settings and a launcher that finds the Xray binary wherever it was
  installed - normally on the /mnt/extern share, which mounts after this service would
  otherwise have started. Deliberately does NOT depend on xray-core: that dependency
  would pull 34 MB into the overlay.
endef

define Package/xray/conffiles
/etc/config/xray
/etc/xray/config.json
endef

# Plain `tar`, not `$(TAR)`: OpenWrt 19.07's rules.mk defines no such variable, so
# `$(TAR)` expands to nothing and the recipe silently becomes ` -xzf ...`.
define Build/Prepare
	$(call Build/Prepare/Default)
	[ -f $(DL_DIR)/$(GO_TARBALL) ] || { echo "missing $(DL_DIR)/$(GO_TARBALL)"; exit 1; }
	mkdir -p $(GOROOT_DIR)
	( set -ex; \
	  tar -xzf $(DL_DIR)/$(GO_TARBALL) -C $(GOROOT_DIR) --strip-components=1; \
	  python3 $(CURDIR)/src/patch-go-toolchain.py $(GOROOT_DIR) \
	    --netpoll $(CURDIR)/src/go-netpoll-pipe.py ) >$(PREPARE_LOG) 2>&1 || \
	  { echo "xray-core: prepare failed, tail of $(PREPARE_LOG):"; tail -40 $(PREPARE_LOG); exit 1; }
endef

# The module set is pinned by the go.sum inside the release tarball, and it is verified
# on every fetch - a stronger guarantee than one archive hash. GOENV=off and the
# explicit cache directories keep the build out of the builder's home directory, which
# on a container build host may not exist at all.
define Build/Compile
	( set -ex; cd $(PKG_BUILD_DIR); \
	  mkdir -p $(GO_WORK_DIR)/path $(GO_WORK_DIR)/mod $(GO_WORK_DIR)/cache $(GO_WORK_DIR)/tmp; \
	  GOROOT="$(GOROOT_DIR)" \
	  GOPATH="$(GO_WORK_DIR)/path" \
	  GOMODCACHE="$(GO_WORK_DIR)/mod" \
	  GOCACHE="$(GO_WORK_DIR)/cache" \
	  GOTMPDIR="$(GO_WORK_DIR)/tmp" \
	  GOENV=off GOTOOLCHAIN=local GOFLAGS=-mod=mod CGO_ENABLED=0 \
	  GOOS=linux GOARCH=mipsle GOMIPS=softfloat \
	  $(GOROOT_DIR)/bin/go build -o xray -trimpath -buildvcs=false \
	    -ldflags "-s -w -buildid=" ./main ) >$(COMPILE_LOG) 2>&1 || \
	  { echo "xray-core: compile failed, tail of $(COMPILE_LOG):"; tail -60 $(COMPILE_LOG); exit 1; }
	# A binary for the wrong architecture installs happily and then fails on the board
	# with nothing but "not found", so check the ELF header here instead.
	readelf -h $(PKG_BUILD_DIR)/xray | grep -q "MIPS R3000"
	ls -l $(PKG_BUILD_DIR)/xray
endef

# The Go distribution ships read-only files too, so make the tree writable before
# OpenWrt tries to remove it.
define Build/Clean
	[ ! -d $(PKG_BUILD_DIR) ] || chmod -R u+w $(PKG_BUILD_DIR)
endef

define Package/xray-core/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/xray $(1)/usr/bin/xray
endef

define Package/xray/install
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/xray.config $(1)/etc/config/xray
	$(INSTALL_DIR) $(1)/etc/xray
	$(INSTALL_CONF) ./files/config.json $(1)/etc/xray/config.json
	$(INSTALL_DATA) ./files/config.example.json $(1)/etc/xray/config.example.json
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/xray.init $(1)/etc/init.d/xray
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/hh71vm-xray $(1)/usr/sbin/hh71vm-xray
endef

$(eval $(call BuildPackage,xray-core))
$(eval $(call BuildPackage,xray))
