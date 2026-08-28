# Copyright 2026 sowarden
# SPDX-License-Identifier: Apache-2.0
"""Shared build identity and candidate validation for optional packages."""

import hashlib
import json
import re
from pathlib import Path

from package_metadata import sha256

ROOT = Path(__file__).resolve().parents[2]
LOCK = "extras/firmware-compatibility.json"
WORKFLOW = ".github/workflows/build-extras.yml"


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def write_json(path, value):
    Path(path).write_bytes((json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8"))


def kernel_inputs(root):
    """Conservative source lock, not a substitute for a runtime test."""
    paths = [root / "openwrt-feed/build.config"]
    paths.extend(p for p in (root / "openwrt-feed/target/linux").rglob("*")
                 if p.is_file() and "__pycache__" not in p.parts and p.suffix != ".pyc")
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda p: p.relative_to(root).as_posix()):
        data = path.read_bytes()
        if b"\0" not in data:
            data = data.replace(b"\r\n", b"\n")
        digest.update(path.relative_to(root).as_posix().encode() + b"\0")
        digest.update(hashlib.sha256(data).digest())
    return digest.hexdigest()


def compatibility(root):
    lock = read_json(root / LOCK)
    if lock.get("schema") != 1:
        raise ValueError("unsupported firmware compatibility schema")
    for key in ("firmware_build", "kernel", "architecture"):
        if not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_.+-]*", lock.get(key, "")):
            raise ValueError(f"invalid firmware {key}")
    if kernel_inputs(root) != lock["kernel_inputs_sha256"]:
        raise ValueError("kernel/config sources changed: publish matching firmware and renew its compatibility lock")
    for name, expected in lock["images"].items():
        if not re.fullmatch(r"[a-zA-Z0-9_.-]+\.bin", name):
            raise ValueError("invalid firmware filename")
        if sha256(root / "firmware" / name) != expected:
            raise ValueError(f"firmware image changed without a compatibility update: {name}")
    for source in lock["upstream"].values():
        if not re.fullmatch(r"[a-f0-9]{40}", source["revision"]):
            raise ValueError("upstream revisions must be full commit hashes")
    return lock


def release_policy(root):
    policy = read_json(root / "extras/release-policy.json")
    if set(policy) != {"auto_publish_unverified"} or type(policy["auto_publish_unverified"]) is not bool:
        raise ValueError("auto_publish_unverified must be a JSON boolean")
    return policy["auto_publish_unverified"]


def validate_candidate(directory, expected_sha=None):
    directory = Path(directory)
    if expected_sha is not None and (
            not re.fullmatch(r"[a-f0-9]{64}", expected_sha) or
            sha256(directory / "candidate.json") != expected_sha):
        raise ValueError("candidate manifest SHA-256 does not match the tested candidate")
    candidate = read_json(directory / "candidate.json")
    if candidate.get("schema") != 1 or candidate.get("hardware_tested") is not False:
        raise ValueError("invalid candidate or self-asserted hardware test")
    if not re.fullmatch(r"[a-f0-9]{40}", candidate.get("source_commit", "")):
        raise ValueError("invalid source commit")
    for key in ("run_id", "run_attempt", "run_number"):
        if type(candidate.get(key)) is not int or candidate[key] < 1:
            raise ValueError(f"invalid {key}")
    assets = candidate.get("assets")
    if not isinstance(assets, dict) or not assets:
        raise ValueError("candidate has no bundle assets")
    for name, digest in assets.items():
        if not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_.+-]*\.zip", name):
            raise ValueError("invalid candidate asset name")
        if not re.fullmatch(r"[a-f0-9]{64}", digest) or sha256(directory / name) != digest:
            raise ValueError(f"candidate asset checksum mismatch: {name}")
    bundles = candidate.get("bundles", {})
    if not isinstance(bundles, dict) or not bundles:
        raise ValueError("candidate has no bundle inventory")
    for name, item in bundles.items():
        if (not re.fullmatch(r"[a-z0-9][a-z0-9+-]*", name) or not isinstance(item, dict) or
                not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.+-]*", item.get("version", ""))):
            raise ValueError("invalid bundle inventory")
    if sorted(item.get("archive", "") for item in bundles.values()) != sorted(assets):
        raise ValueError("bundle inventory does not match candidate assets")
    if set(p.name for p in directory.iterdir()) != set(assets) | {"candidate.json", "SHA256SUMS"}:
        raise ValueError("unexpected candidate files")
    expected_sums = dict(assets, **{"candidate.json": sha256(directory / "candidate.json")})
    checksum_text = "".join(f"{digest}  {name}\n" for name, digest in sorted(expected_sums.items()))
    if (directory / "SHA256SUMS").read_text() != checksum_text:
        raise ValueError("candidate checksum inventory mismatch")
    return candidate
