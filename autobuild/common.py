"""Immutable HH71VM release identities and package validation."""
import base64
import gzip
import hashlib
import io
import json
import re
import subprocess
import tarfile
from pathlib import Path, PurePosixPath

REPOSITORY = "sowarden/hh71vm-openwrt"
ARCHITECTURE = "mipsel_24kc"
ROOTS = ("luci-app-modem-extra-tools", "luci-proto-wireguard", "wireguard-tools")
NAME = re.compile(r"[a-zA-Z0-9][a-zA-Z0-9_.+~-]*\Z")
TAG = re.compile(r"hh71vm-[0-9a-f]{12}-r[1-9][0-9]*-a[1-9][0-9]*\Z")
SHA = re.compile(r"[0-9a-f]{64}\Z")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha256(path):
    with Path(path).open("rb") as stream:
        result = hashlib.sha256()
        for block in iter(lambda: stream.read(1048576), b""):
            result.update(block)
    return result.hexdigest()


def json_bytes(value):
    result = (json.dumps(value, sort_keys=True, indent=2) + "\n").encode()
    return result + b"\n\n" if (64 + len(result)) % 128 in (110, 111) else result


def write_json(path, value):
    Path(path).write_bytes(json_bytes(value))


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON key")
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique)


def identity(commit, run_id, attempt):
    if not re.fullmatch(r"[a-f0-9]{40}", commit):
        raise ValueError("source commit must be a full SHA")
    if not all(re.fullmatch(r"[1-9][0-9]*", str(x)) for x in (run_id, attempt)):
        raise ValueError("invalid build identity")
    return f"hh71vm-{commit[:12]}-r{run_id}-a{attempt}"


def feed_url(tag):
    if not TAG.fullmatch(tag):
        raise ValueError("invalid immutable release tag")
    return f"https://github.com/{REPOSITORY}/releases/download/{tag}"


def public_key(data):
    lines = data.decode("ascii").strip().splitlines()
    if len(lines) != 2 or not lines[0].startswith("untrusted comment:"):
        raise ValueError("invalid public key format")
    raw = base64.b64decode(lines[1], validate=True)
    if len(raw) != 42 or raw[:2] != b"Ed":
        raise ValueError("expected a usign Ed25519 public key")
    normalized = ("untrusted comment: HH71VM package signing key\n" + lines[1] + "\n").encode()
    return raw[2:10].hex(), normalized


def fields(text):
    result, current = {}, None
    for line in text.splitlines():
        if line.startswith((" ", "\t")) and current:
            result[current] += "\n" + line
        elif ":" in line:
            current, value = line.split(":", 1)
            value = value.lstrip(" ")
            if current in result:
                raise ValueError("duplicate package field")
            result[current] = value
        elif line:
            raise ValueError("malformed package metadata")
    return result


def records(text):
    return [fields(block) for block in text.strip().split("\n\n") if block.strip()]


def safe_members(archive):
    total = 0
    seen = set()
    for member in archive:
        path = PurePosixPath(member.name)
        if path.is_absolute() or ".." in path.parts or "\\" in member.name:
            raise ValueError("unsafe archive member")
        if str(path) in seen:
            raise ValueError("duplicate archive member")
        seen.add(str(path))
        if member.isdev() or member.isfifo():
            raise ValueError("unsupported archive member")
        total += member.size
        if total > 256 * 1024 * 1024:
            raise ValueError("archive exceeds inspection limit")
        yield member


def ipk(path):
    with tarfile.open(path, "r:gz") as outer:
        members = {m.name.removeprefix("./"): m for m in safe_members(outer)}
        if set(members) != {"debian-binary", "control.tar.gz", "data.tar.gz"}:
            raise ValueError("unexpected IPK envelope")
        if not all(m.isfile() for m in members.values()) or outer.extractfile(members["debian-binary"]).read() != b"2.0\n":
            raise ValueError("unsupported IPK format")
        with tarfile.open(fileobj=io.BytesIO(outer.extractfile(members["control.tar.gz"]).read()), mode="r:gz") as control:
            controls = {m.name.removeprefix("./"): m for m in safe_members(control)}
            metadata = fields(control.extractfile(controls["control"]).read().decode())
        for key in ("Package", "Version", "Architecture", "Description"):
            if not metadata.get(key):
                raise ValueError("missing IPK field: " + key)
        expected = "{Package}_{Version}_{Architecture}.ipk".format(**metadata)
        if Path(path).name != expected or not NAME.fullmatch(expected):
            raise ValueError("IPK filename differs from its metadata")
        if metadata["Architecture"] not in (ARCHITECTURE, "all"):
            raise ValueError("foreign package architecture")
        payload = outer.extractfile(members["data.tar.gz"]).read()
    with tarfile.open(fileobj=io.BytesIO(payload), mode="r:gz") as data:
        modules = 0
        for member in safe_members(data):
            if member.isfile():
                body = data.extractfile(member).read()
                privacy(body)
                if member.name.endswith(".ko") and (body[:6] != b"\x7fELF\x01\x01" or body[18:20] != b"\x08\x00"):
                    raise ValueError("non-MIPS kernel module")
                if member.name.endswith(".ko"):
                    modules += 1
                if member.name.endswith("/xtables-legacy-multi") and body[:4] != b"\x7fELF":
                    raise ValueError("xtables must be an ELF executable")
        if metadata["Package"] in ("kmod-wireguard", "kmod-hh71vm-ipt-ipopt") and not modules:
            raise ValueError("required kernel module payload is empty")
    return metadata


def privacy(data):
    patterns = (rb"[A-Za-z]:[\\/]Users[\\/][^\x00\r\n ]+",
                rb"/mnt/[a-z]/Users/[^\x00\r\n ]+", rb"/home/(?!build/)[^/:\x00\r\n ]+/",
                rb"untrusted comment:.*(?:secret|private) key")
    if any(re.search(pattern, data, re.IGNORECASE) for pattern in patterns):
        raise ValueError("private material detected; payload omitted")


def make_index(packages):
    blocks = []
    for path, metadata in sorted(packages, key=lambda item: item[0].name):
        item = dict(metadata)
        description = item.pop("Description")
        for key in ("Maintainer", "LicenseFiles", "Source", "SourceName", "Require"):
            item.pop(key, None)
        item.update(Filename=path.name, Size=str(path.stat().st_size), SHA256sum=sha256(path), Description=description)
        blocks.append("\n".join(f"{key}: {value}" for key, value in item.items()))
    result = ("\n\n".join(blocks) + "\n").encode()
    # OpenWrt 19.07 package/index: avoid the old usign SHA-512 boundary bug.
    if (64 + len(result)) % 128 in (110, 111):
        result += b"\n\n"
    return result


def dependency(item):
    match = re.fullmatch(r"\s*([a-zA-Z0-9][a-zA-Z0-9+_.-]*)(?:\s*\(\s*(<<|<=|>=|>>|=|<|>)\s*([^\s()]+)\s*\))?\s*", item)
    if not match:
        raise ValueError("unsupported dependency syntax: " + item)
    return match.groups()


def satisfies(record, requirement):
    name, operator, version = dependency(requirement)
    provided = [dependency(p)[0] for p in record.get("Provides", "").split(",") if p.strip()]
    if name != record["Package"] and name not in provided:
        return False
    if operator:
        # Versioned virtual providers are not inferred from unrelated package versions.
        if name != record["Package"]:
            return False
        operator = {"<": "lt", ">": "gt"}.get(operator, operator)
        return subprocess.run(["dpkg", "--compare-versions", record["Version"], operator, version],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
    return True


def validate_closure(packages, installed, roots=ROOTS):
    base = {r["Package"]: r for r in installed}
    if len(base) != len(installed):
        raise ValueError("duplicate image package metadata")
    if "kernel" not in base:
        raise ValueError("image kernel metadata is missing")
    available = {r["Package"]: r for r in packages}
    if len(available) != len(packages) or "kernel" in available:
        raise ValueError("duplicate package or installable kernel replacement")
    kernel = base["kernel"]["Version"]
    for name in set(base) & set(available):
        if base[name]["Version"] != available[name]["Version"]:
            raise ValueError("feed and image package versions differ: " + name)
    for root in roots:
        if root not in available:
            raise ValueError("required optional package missing: " + root)
    universe = list(base.values()) + list(available.values())
    for record in packages:
        if record["Package"].startswith("kmod-"):
            expected = ("kernel", "=", kernel)
            if expected not in [dependency(d) for d in record.get("Depends", "").split(",")]:
                raise ValueError("kernel ABI mismatch: " + record["Package"])
        for group in record.get("Depends", "").split(","):
            if group.strip() and not any(satisfies(r, option) for option in group.split("|") for r in universe):
                raise ValueError("unresolved dependency: " + record["Package"] + " -> " + group)
    return kernel


def validate_candidate(directory, expected_tag=None, expected_commit=None, signed=False):
    directory = Path(directory)
    manifest = read_json(directory / "release.json")
    if manifest.get("schema") != 1 or manifest.get("hardware_tested") is not False:
        raise ValueError("invalid release schema or hardware assertion")
    tag = identity(manifest["source_commit"], manifest["run_id"], manifest["run_attempt"])
    if tag != manifest["tag"] or (expected_tag and tag != expected_tag):
        raise ValueError("release identity mismatch")
    if expected_commit and manifest["source_commit"] != expected_commit:
        raise ValueError("source commit mismatch")
    if manifest["architecture"] != ARCHITECTURE or manifest["feed_url"] != feed_url(tag):
        raise ValueError("feed target mismatch")
    inventory = manifest["files"]
    if not inventory or not all(NAME.fullmatch(n) and SHA.fullmatch(h) for n, h in inventory.items()):
        raise ValueError("invalid asset inventory")
    required = {"Packages", "Packages.gz", "hh71vm-feed.pub", "image-packages.json", "build.config",
                "build-evidence.json", "build-environment.json", "source-lock.json", "packages-bundle.zip",
                "source-delta.tar.gz", "upstream-sources.tar.gz", "upstream-buildsystem.tar.gz", "download-checksums.json"}
    if not required <= set(inventory):
        raise ValueError("required release assets missing")
    extras = {"release.json"} | ({"Packages.sig", "release.json.sig", "SHA256SUMS"} if signed else set())
    if set(p.name for p in directory.iterdir()) != set(inventory) | extras:
        raise ValueError("unexpected or missing release assets")
    for name, expected in inventory.items():
        path = directory / name
        if not path.is_file() or path.is_symlink() or sha256(path) != expected:
            raise ValueError("asset hash/type mismatch: " + name)
    key_id, normalized = public_key((directory / "hh71vm-feed.pub").read_bytes())
    if key_id != manifest["key_id"] or normalized != (directory / "hh71vm-feed.pub").read_bytes():
        raise ValueError("release public key mismatch")
    packages = [(p, ipk(p)) for p in directory.glob("*.ipk")]
    installed = read_json(directory / "image-packages.json")
    if validate_closure([r for _, r in packages], installed) != manifest["kernel"]:
        raise ValueError("image and feed ABI differ")
    if (directory / "Packages").read_bytes() != make_index(packages):
        raise ValueError("package index differs from IPK metadata")
    if gzip.decompress((directory / "Packages.gz").read_bytes()) != (directory / "Packages").read_bytes():
        raise ValueError("compressed index mismatch")
    return manifest
