#!/usr/bin/env python3
# Copyright 2026 sowarden
# SPDX-License-Identifier: Apache-2.0
"""Index all published IPKs, including the kernel and libc virtual packages."""

import argparse
import gzip
from pathlib import Path

from package_metadata import control_fields, read_control, sha256


def build_index(directory):
    entries = []
    for path in sorted(directory.glob("*.ipk")):
        fields = control_fields(read_control(path))
        expected = "{Package}_{Version}_{Architecture}.ipk".format(**fields)
        if path.name != expected:
            raise ValueError(f"IPK filename does not match its control metadata: {path}")
        description = fields.pop("Description")
        # Match the compact public index; provenance remains in each IPK.
        for name in ("Source", "SourceName", "Maintainer"):
            fields.pop(name, None)
        fields.update(Filename=path.name, Size=str(path.stat().st_size),
                      SHA256sum=sha256(path), Description=description)
        entries.append("\n".join(f"{key}: {value}" for key, value in fields.items()))
    if not entries:
        raise ValueError("no IPK files found")
    data = ("\n\n".join(entries).rstrip() + "\n").encode("utf-8")
    compressed = gzip.compress(data, compresslevel=9, mtime=0)
    (directory / "Packages").write_bytes(data)
    (directory / "Packages.gz").write_bytes(compressed)
    return len(entries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    print(f"Indexed {build_index(args.directory)} packages")


if __name__ == "__main__":
    main()
