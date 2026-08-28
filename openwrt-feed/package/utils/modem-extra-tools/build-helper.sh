#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu
cd "$(dirname "$0")"
compiler=${ARM_CC:-arm-linux-gnueabi-gcc}
strip=${ARM_STRIP:-arm-linux-gnueabi-strip}
host_compiler=${HOST_CC:-cc}
host_test=$(mktemp /tmp/hh71-imei-selftest.XXXXXX)
trap 'rm -f "$host_test"' EXIT HUP INT TERM
"$host_compiler" -DHOST_TEST -Os -fno-builtin -fno-stack-protector \
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wl,--gc-sections \
  -o "$host_test" src/hh71-imei.c
"$host_test" selftest
for helper in nas imei; do
  "$compiler" -Os -static -nostdlib -fno-builtin -fno-stack-protector \
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
    -Wl,--gc-sections,-e,_start,--build-id=none \
    -o "files/hh71-$helper-arm" "src/hh71-$helper.c"
  "$strip" "files/hh71-$helper-arm"
done
(
  cd src
  sha256sum hh71-nas.c hh71-imei.c
  cd ../files
  sha256sum hh71-nas-arm hh71-imei-arm
) > files/helpers.sha256
cat files/helpers.sha256
