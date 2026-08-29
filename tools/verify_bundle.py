#!/usr/bin/env python3
"""Verify files extracted from an HH71VM OpenWrt flash bundle."""
import hashlib
import json
import re
import sys
from pathlib import Path, PurePosixPath


SHA256 = re.compile(r"[0-9a-f]{64}\Z")


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1048576), b""):
            result.update(block)
    return result.hexdigest()


def main():
    root = Path(__file__).resolve().parent
    metadata = json.loads((root / "bundle.json").read_text(encoding="utf-8"))
    if metadata.get("schema") != 1 or not isinstance(metadata.get("files"), dict):
        raise ValueError("invalid bundle manifest")
    for name, expected in sorted(metadata["files"].items()):
        relative = PurePosixPath(name)
        if relative.is_absolute() or ".." in relative.parts or not SHA256.fullmatch(expected):
            raise ValueError("unsafe bundle manifest entry")
        path = root.joinpath(*relative.parts)
        if not path.is_file() or path.is_symlink() or digest(path) != expected:
            raise ValueError("missing or modified bundle file: " + name)
    expected_sums = "".join(
        f"{checksum}  {name}\n" for name, checksum in sorted(metadata["files"].items()))
    if (root / "SHA256SUMS").read_text(encoding="utf-8") != expected_sums:
        raise ValueError("bundle checksum list differs from its manifest")
    print("HH71VM flash bundle verified: " + metadata["tag"])


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print("ERROR: " + str(error), file=sys.stderr)
        raise SystemExit(1)
