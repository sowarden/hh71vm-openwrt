"""Build and validate the self-contained HH71VM flashing bundle."""
import json
import stat
import zipfile
from pathlib import Path, PurePosixPath

from common import IMAGE_ASSETS, SHA, digest, json_bytes, privacy, sha256


BUNDLE_ASSET = "hh71vm-openwrt-flash-bundle.zip"
BUNDLE_CHECKSUM = BUNDLE_ASSET + ".sha256"
BUNDLE_ROOT = "hh71vm-openwrt-flash-bundle"
SOURCE_FILES = {
    "LICENSE": "LICENSE",
    "LICENSE-APACHE-2.0": "LICENSE-APACHE-2.0",
    "LICENSING.md": "LICENSING.md",
    "tools/requirements.txt": "tools/requirements.txt",
    "tools/ram_boot.py": "tools/ram_boot.py",
    "tools/verify_bundle.py": "verify_bundle.py",
    "docs/flash-install.md": "docs/flash-install.md",
    "docs/ram-boot.md": "docs/ram-boot.md",
    "docs/telnet-access.md": "docs/telnet-access.md",
    "docs/testing.md": "docs/testing.md",
    "docs/assets/realtek-uart-pinout.jpg": "docs/assets/realtek-uart-pinout.jpg",
}
FLASH_TOOLS = (
    "_common.py", "_lan.py", "flash_openwrt_tftp.py", "flash_openwrt_vendor.py",
    "install_openwrt_lan.py", "restore_stock.py", "restore_stock_lan.py", "rtk_mkimg.py",
    "rtk_romloader.py", "rtk_tftp_put.py", "tftp_dump_mtd.py", "uart_ram_boot.py",
    "uart_shell.py",
)


def _read_json(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate bundle JSON key")
            result[key] = value
        return result
    return json.loads(data.decode("utf-8"), object_pairs_hook=unique)


def _read_source(path):
    path = Path(path)
    if not path.is_file() or path.is_symlink():
        raise ValueError("bundle source file is missing or unsafe: " + str(path))
    data = path.read_bytes()
    if b"\0" not in data:
        privacy(data)
    return data


def _readme(tag):
    return f"""# HH71VM OpenWrt flash bundle

Release: `{tag}`

This archive contains the matching firmware images, installation and recovery tools, and
the documentation needed to flash HH71VM. Do not replace individual files with files from
another Release.

1. Open a terminal in this directory.
2. Run `python verify_bundle.py`.
3. Read `docs/flash-install.md` for stock installation or sysupgrade.
4. Read `docs/ram-boot.md` for optional RAM boot over UART.

The LAN installer performs a dry run and stock backup before writing flash when used as
documented. Keep the generated `backup-stock` directory safe and device-specific.
""".encode()


def create(source, candidate, tag, commit, run_id, run_attempt):
    source, candidate = Path(source), Path(candidate)
    entries = {}
    for source_name, bundle_name in SOURCE_FILES.items():
        entries[bundle_name] = _read_source(source / source_name)
    for name in FLASH_TOOLS:
        bundle_name = "tools/flash/" + name
        entries[bundle_name] = _read_source(source / bundle_name)
    for asset_name in IMAGE_ASSETS.values():
        image = candidate / asset_name
        if not image.is_file() or image.is_symlink():
            raise ValueError("bundle firmware image is missing or unsafe: " + asset_name)
        entries["firmware/" + asset_name] = image.read_bytes()
    entries["README.md"] = _readme(tag)
    inventory = {name: digest(data) for name, data in sorted(entries.items())}
    metadata = {
        "schema": 1,
        "tag": tag,
        "source_commit": commit,
        "run_id": run_id,
        "run_attempt": run_attempt,
        "files": inventory,
    }
    entries["bundle.json"] = json_bytes(metadata)
    entries["SHA256SUMS"] = "".join(
        f"{checksum}  {name}\n" for name, checksum in inventory.items()).encode()

    destination = candidate / BUNDLE_ASSET
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(entries.items()):
            item = zipfile.ZipInfo(BUNDLE_ROOT + "/" + name, (2020, 1, 1, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            item.create_system = 3
            item.external_attr = (stat.S_IFREG | 0o644) << 16
            archive.writestr(item, data)
    (candidate / BUNDLE_CHECKSUM).write_text(
        f"{sha256(destination)}  {BUNDLE_ASSET}\n", encoding="utf-8")
    validate(destination, tag, commit, run_id, run_attempt, candidate)


def validate(path, tag, commit, run_id, run_attempt, candidate=None):
    payloads = {}
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        if len({item.filename for item in infos}) != len(infos):
            raise ValueError("duplicate flash bundle entry")
        prefix = BUNDLE_ROOT + "/"
        for item in infos:
            if not item.filename.startswith(prefix) or item.is_dir():
                raise ValueError("invalid flash bundle layout")
            name = item.filename[len(prefix):]
            pure = PurePosixPath(name)
            mode = item.external_attr >> 16
            if (not name or pure.is_absolute() or ".." in pure.parts or
                    (stat.S_IFMT(mode) not in (0, stat.S_IFREG))):
                raise ValueError("unsafe flash bundle entry")
            payloads[name] = archive.read(item)

    metadata = _read_json(payloads.get("bundle.json", b""))
    if (metadata.get("schema") != 1 or metadata.get("tag") != tag or
            metadata.get("source_commit") != commit or metadata.get("run_id") != run_id or
            metadata.get("run_attempt") != run_attempt):
        raise ValueError("flash bundle identity mismatch")
    inventory = metadata.get("files")
    if not isinstance(inventory, dict) or not inventory or not all(
            isinstance(name, str) and isinstance(checksum, str) and SHA.fullmatch(checksum)
            for name, checksum in inventory.items()):
        raise ValueError("invalid flash bundle inventory")
    required = (set(SOURCE_FILES.values()) | {"tools/flash/" + name for name in FLASH_TOOLS} |
                {"firmware/" + name for name in IMAGE_ASSETS.values()} | {"README.md"})
    if not required <= set(inventory):
        raise ValueError("required flash bundle file is missing")
    if set(payloads) != set(inventory) | {"bundle.json", "SHA256SUMS"}:
        raise ValueError("unexpected or missing flash bundle file")
    for name, checksum in inventory.items():
        if digest(payloads[name]) != checksum:
            raise ValueError("flash bundle file hash mismatch: " + name)
    expected_sums = "".join(
        f"{checksum}  {name}\n" for name, checksum in sorted(inventory.items())).encode()
    if payloads["SHA256SUMS"] != expected_sums:
        raise ValueError("flash bundle checksum list mismatch")
    if candidate is not None:
        candidate = Path(candidate)
        for asset_name in IMAGE_ASSETS.values():
            if payloads["firmware/" + asset_name] != (candidate / asset_name).read_bytes():
                raise ValueError("flash bundle image differs from Release asset: " + asset_name)
    return metadata
