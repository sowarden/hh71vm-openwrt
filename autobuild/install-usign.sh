#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu
[ "$#" = 1 ] || { echo 'Usage: install-usign.sh EMPTY_DIRECTORY' >&2; exit 2; }
directory=$1
[ ! -e "$directory" ] || { echo 'Refusing an existing directory.' >&2; exit 1; }
git init -q "$directory"
git -C "$directory" fetch -q --depth=1 https://git.openwrt.org/project/usign.git f1f65026a94137c91b5466b149ef3ea3f20091e9
git -C "$directory" checkout -q --detach FETCH_HEAD
cmake -S "$directory" -B "$directory/build" -DUSE_LIBUBOX=OFF
cmake --build "$directory/build" --parallel 2
test -x "$directory/build/usign"
