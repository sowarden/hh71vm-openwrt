#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
	printf 'Usage: %s OPENWRT_ROOT\n' "$0" >&2
	exit 2
fi

openwrt_root=$1

if [ ! -f "$openwrt_root/Makefile" ] || \
	[ ! -f "$openwrt_root/tools/libtool/patches/000-relocatable.patch" ]; then
	printf 'Not an OpenWrt source tree: %s\n' "$openwrt_root" >&2
	exit 1
fi

host_grep=$(command -v grep)
case "$host_grep" in
	/*) ;;
	*)
		printf 'grep did not resolve to an absolute host path: %s\n' "$host_grep" >&2
		exit 1
		;;
esac

if [ ! -x "$host_grep" ]; then
	printf 'Host grep is not executable: %s\n' "$host_grep" >&2
	exit 1
fi

host_bin="$openwrt_root/staging_dir/host/bin"
mkdir -p "$host_bin"
ln -sfn "$host_grep" "$host_bin/grep"

if ! "$host_bin/grep" --version >/dev/null 2>&1; then
	printf 'Staged host grep is not executable: %s\n' "$host_bin/grep" >&2
	exit 1
fi

printf 'Prepared OpenWrt host grep: %s -> %s\n' "$host_bin/grep" "$host_grep"
