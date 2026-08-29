#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# No network is required. HH71VM_ROOT is for isolated filesystem tests.
set -eu
root=${HH71VM_ROOT:-}
case "$root" in ''|/*) ;; *) exit 2 ;; esac
conf="$root/etc/opkg"
lists="$root/var/opkg-lists"
lock="$root/tmp/hh71vm-feed.lock"
mkdir -p "$conf" "$root/tmp"
mkdir "$lock" 2>/dev/null || { echo 'HH71VM feed migration is already running.' >&2; exit 1; }
trap 'rm -f "$lock/clean" "$lock/names" "$lock/directories"; rmdir "$lock"' EXIT HUP INT TERM

fail() {
  rm -f "$conf/hh71vm.conf" "$lists/hh71vm" "$lists/hh71vm.sig"
  echo "HH71VM feed disabled: $1" >&2
  exit 1
}

atomic_copy() {
  temporary=$(mktemp "$2.hh71vm.XXXXXX") || fail 'cannot stage configuration'
  cat "$1" > "$temporary"
  chmod 0644 "$temporary"
  mv -f "$temporary" "$2"
}

# Clear recognized cached indexes even when a restored configuration relocated
# lists_dir. Never follow path traversal or remove an unrelated feed's index.
printf '%s\n' /var/opkg-lists > "$lock/directories"
for file in "$root/etc/opkg.conf" "$conf/"*.conf; do
  [ -f "$file" ] || continue
  awk '$1 == "lists_dir" && $3 ~ /^\// && $3 !~ /(^|\/)\.\.(\/|$)/ { print $3 }' "$file" >> "$lock/directories"
done

# Retire old project feeds before checking the restored configuration. Unrelated
# lines are preserved, including comments, spacing and custom feed names.
for file in "$root/etc/opkg.conf" "$conf/"*.conf; do
  [ -f "$file" ] || continue
  awk -v names="$lock/names" '
    function owned(url) {
      return url ~ /^https?:\/\/github.com\/sowarden\/hh71vm-openwrt\/releases\/download\/hh71vm-/ ||
             url ~ /^https?:\/\/raw.githubusercontent.com\/sowarden\/hh71vm-openwrt\//
    }
    $1 ~ /^src(\/gz)?$/ && owned($3) {
      if ($2 !~ /^[A-Za-z0-9_.-]+$/ || $2 == "." || $2 == "..") exit 2
      print $2 >> names; next
    }
    { print }
  ' "$file" > "$lock/clean" || fail 'invalid legacy feed name'
  if [ -f "$lock/names" ]; then
    while IFS= read -r directory; do
      while IFS= read -r name; do rm -f "$root$directory/$name" "$root$directory/$name.sig"; done < "$lock/names"
    done < "$lock/directories"
    rm -f "$lock/names"
  fi
  if ! cmp -s "$file" "$lock/clean"; then
    if [ "$file" != "$conf/hh71vm.conf" ]; then
      mkdir -p "$root/etc/hh71vm-feed/backups"
      name=${file##*/}
      number=0
      while [ -e "$root/etc/hh71vm-feed/backups/$name.$number" ]; do number=$((number + 1)); done
      cp "$file" "$root/etc/hh71vm-feed/backups/$name.$number"
    fi
    atomic_copy "$lock/clean" "$file"
  fi
done
rm -f "$conf/hh71vm.conf" "$lists/hh71vm" "$lists/hh71vm.sig"

data="$root/rom/usr/share/hh71vm-feed"
if [ ! -d "$root/rom" ]; then data="$root/usr/share/hh71vm-feed"; fi
[ -f "$data/release.conf" ] && [ -f "$data/release.pub" ] || fail 'current image descriptor is missing'
value() {
  awk -F= -v key="$1" '$1 == key { count++; value=substr($0,length(key)+2) }
    END { if(count != 1) exit 1; print value }' "$data/release.conf"
}
release=$(value release) || fail 'invalid release identity'
kernel=$(value kernel) || fail 'invalid kernel identity'
architecture=$(value architecture) || fail 'invalid architecture'
key_id=$(value key_id) || fail 'invalid key identity'
url=$(value feed_url) || fail 'invalid feed URL'
printf '%s\n' "$release" | grep -Eq '^hh71vm-[a-f0-9]{12}-r[1-9][0-9]*-a[1-9][0-9]*$' || fail 'invalid release tag'
printf '%s\n' "$key_id" | grep -Eq '^[a-f0-9]{16}$' || fail 'invalid key fingerprint'
[ "$architecture" = mipsel_24kc ] || fail 'unsupported architecture'
[ "$url" = "https://github.com/sowarden/hh71vm-openwrt/releases/download/$release" ] || fail 'URL does not match this image'
[ "$(cat "$root/tmp/sysinfo/board_name" 2>/dev/null)" = hh71vm ] || fail 'unsupported board'
installed=$(awk '/^Package: kernel$/ { found=1; next } found && /^Version: / { print $2; exit } /^$/ { found=0 }' "$root/usr/lib/opkg/status")
[ -n "$installed" ] && [ "$installed" = "$kernel" ] || fail 'installed kernel ABI does not match this image'
[ "$(usign -F -p "$data/release.pub" 2>/dev/null)" = "$key_id" ] || fail 'public key fingerprint mismatch'

for file in "$root/etc/opkg.conf" "$conf/"*.conf; do
  [ -f "$file" ] || continue
  awk '
    $1 == "option" && $2 == "check_signature" && NF > 2 && $3 != "1" { exit 1 }
    $1 == "option" && $2 ~ /^force_(signature|depends)$/ && $3 != "0" { exit 1 }
    $1 == "lists_dir" && $3 != "/var/opkg-lists" { exit 1 }
    $1 ~ /^src(\/gz)?$/ && $2 == "hh71vm" { exit 1 }
  ' "$file" || fail 'unsafe opkg overrides; review signature, list directory or reserved feed name'
done
mkdir -p "$conf/keys"
for old_key in "$conf/keys/"*; do
  [ -f "$old_key" ] || continue
  [ "${old_key##*/}" != "$key_id" ] || continue
  if [ "$(head -n 1 "$old_key")" = 'untrusted comment: HH71VM package signing key' ]; then
    rm -f "$old_key"
  fi
done
atomic_copy "$data/release.pub" "$conf/keys/$key_id"
printf '%s\n' "src/gz hh71vm $url" > "$lock/clean"
atomic_copy "$lock/clean" "$conf/hh71vm.conf"
echo 'HH71VM feed matches the installed firmware.'
