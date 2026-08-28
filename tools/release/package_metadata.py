# Copyright 2026 sowarden
# SPDX-License-Identifier: Apache-2.0
"""Read the tar-based IPK format produced by this OpenWrt 19.07 build."""

import hashlib
import io
import tarfile


def read_control(path):
    with tarfile.open(path, "r:gz") as archive:
        control_data = archive.extractfile("./control.tar.gz").read()
    with tarfile.open(fileobj=io.BytesIO(control_data), mode="r:gz") as archive:
        return archive.extractfile("./control").read().decode("utf-8")


def control_fields(text):
    fields = {}
    current = None
    for line in text.splitlines():
        if line.startswith((" ", "\t")) and current:
            fields[current] += "\n" + line
        elif ": " in line:
            current, value = line.split(": ", 1)
            if current in fields:
                raise ValueError(f"duplicate control field: {current}")
            fields[current] = value
        elif line:
            raise ValueError(f"invalid control line: {line}")
    for name in ("Package", "Version", "Architecture", "Description"):
        if not fields.get(name):
            raise ValueError(f"missing control field: {name}")
    return fields


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()
