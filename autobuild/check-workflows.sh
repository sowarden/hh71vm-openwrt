#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu
[ "$#" = 1 ] || { echo 'Usage: check-workflows.sh EMPTY_DIRECTORY' >&2; exit 2; }
directory=$1
mkdir "$directory"
curl --fail --silent --show-error --location --connect-timeout 20 --max-time 120 \
  -o "$directory/actionlint.tar.gz" \
  https://github.com/rhysd/actionlint/releases/download/v1.7.12/actionlint_1.7.12_linux_amd64.tar.gz
printf '%s  %s\n' 8aca8db96f1b94770f1b0d72b6dddcb1ebb8123cb3712530b08cc387b349a3d8 \
  "$directory/actionlint.tar.gz" | sha256sum -c -
tar -xzf "$directory/actionlint.tar.gz" -C "$directory" actionlint
"$directory/actionlint" -shellcheck= -pyflakes= \
  .github/workflows/autobuild.yml \
  .github/workflows/release-resume.yml \
  .github/workflows/validate.yml
