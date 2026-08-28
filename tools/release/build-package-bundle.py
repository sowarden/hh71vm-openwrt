#!/usr/bin/env python3
# Copyright 2026 sowarden
# SPDX-License-Identifier: Apache-2.0
"""Build a deterministic user-installable ZIP from a bundle manifest."""

import argparse
import json
import re
import shutil
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

from package_metadata import control_fields, read_control, sha256


ROOT = Path(__file__).resolve().parents[2]
FIXED_ZIP_TIME = (2026, 1, 1, 0, 0, 0)


def safe_relative_path(value):
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9_./+-]+", value):
        raise ValueError(f"unsafe relative path: {value}")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or not path.parts or str(path) != value:
        raise ValueError(f"unsafe relative path: {value}")
    return path


def load_manifest(bundle_dir):
    manifest_path = bundle_dir / "bundle.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema") != 1:
        raise ValueError("unsupported bundle manifest schema")
    for field in ("name", "version", "archive_suffix", "architecture", "kernel"):
        if not isinstance(manifest.get(field), str) or not re.fullmatch(
                r"[A-Za-z0-9][A-Za-z0-9_.+-]*", manifest[field]):
            raise ValueError(f"missing manifest field: {field}")
    for field in ("files", "packages"):
        values = manifest.get(field)
        if not isinstance(values, list) or not values:
            raise ValueError(f"manifest field must be a non-empty list: {field}")
        for value in values:
            safe_relative_path(value)
        if len(values) != len(set(values)):
            raise ValueError(f"duplicate entry in manifest field: {field}")
    paths = manifest["files"] + manifest["packages"]
    if len(set(paths)) != len(paths) or set(paths) & {"SHA256SUMS", "bundle.env", "COMPATIBILITY.txt"}:
        raise ValueError("colliding bundle filenames")
    hashes = manifest.get("sha256", {})
    if not isinstance(hashes, dict) or set(hashes) != set(manifest["packages"]):
        raise ValueError("every package requires an exact SHA-256")
    if any(not isinstance(value, str) or not re.fullmatch(r"[a-f0-9]{64}", value)
           for value in hashes.values()):
        raise ValueError("invalid package SHA-256")
    return manifest


def validate_package(source, manifest):
    if sha256(source) != manifest["sha256"][source.name]:
        raise ValueError(f"package checksum mismatch: {source.name}")
    fields = control_fields(read_control(source))
    if source.name != "{Package}_{Version}_{Architecture}.ipk".format(**fields):
        raise ValueError(f"package metadata/filename mismatch: {source.name}")
    if fields["Architecture"] not in ("all", manifest["architecture"]):
        raise ValueError(f"wrong package architecture: {source.name}")
    if fields["Package"].startswith("kmod-"):
        dependencies = re.sub(r"\s+", "", fields.get("Depends", "")).split(",")
        if f"kernel(={manifest['kernel']})" not in dependencies:
            raise ValueError(f"kernel ABI mismatch: {source.name}")


def source_file(root, relative):
    source = root / relative
    if not source.is_file() or root.resolve() not in source.resolve().parents:
        raise ValueError(f"missing or out-of-tree input: {source}")
    return source


def add_to_zip(archive, source, target, executable=False):
    info = zipfile.ZipInfo(target.as_posix(), FIXED_ZIP_TIME)
    info.create_system = 3
    mode = 0o100755 if executable else 0o100644
    info.external_attr = mode << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(info, source.read_bytes())


def build_bundle(bundle_dir, package_dir, output_dir):
    manifest = load_manifest(bundle_dir)
    folder_name = f"{manifest['name']}-{manifest['version']}"
    archive_name = f"{folder_name}-{manifest['archive_suffix']}.zip"
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / archive_name

    with tempfile.TemporaryDirectory(prefix="package-bundle-") as temporary:
        stage = Path(temporary) / folder_name
        stage.mkdir()

        staged = []
        settings = [f"expected_kernel='{manifest['kernel']}'"]
        setting_keys = set()
        for name in manifest["packages"]:
            fields = control_fields(read_control(source_file(package_dir, name)))
            package = fields["Package"]
            if not re.fullmatch(r"[a-z0-9][a-z0-9+-]*", package):
                raise ValueError(f"unsafe package name: {package}")
            key = "ipk_" + package.replace("-", "_").replace("+", "_")
            if key in setting_keys:
                raise ValueError(f"colliding installer variable: {key}")
            setting_keys.add(key)
            settings.append(f"{key}='{name}'")
        (stage / "bundle.env").write_bytes(("\n".join(settings) + "\n").encode("utf-8"))
        staged.append(Path("bundle.env"))
        compatibility = (
            "HH71VM optional tools; not firmware.\n"
            f"Firmware build: {manifest.get('firmware_build', 'unspecified; see release notes')}\n"
            f"Kernel package: {manifest['kernel']}\n"
            f"Architecture: {manifest['architecture']}\n"
            f"Bundle version: {manifest['version']}\n"
            "A successful build does not establish hardware test coverage.\n"
            "Check the candidate metadata and release notes for test status.\n"
        )
        (stage / "COMPATIBILITY.txt").write_bytes(compatibility.encode("utf-8"))
        staged.append(Path("COMPATIBILITY.txt"))
        for relative in manifest["files"]:
            relative_path = Path(*safe_relative_path(relative).parts)
            source = source_file(bundle_dir, relative_path)
            target = stage / relative_path
            target.parent.mkdir(parents=True, exist_ok=True)
            # Git and Windows checkouts may use different text line endings.
            target.write_bytes(source.read_bytes().replace(b"\r\n", b"\n"))
            staged.append(relative_path)

        for name in manifest["packages"]:
            relative_path = Path(*safe_relative_path(name).parts)
            if len(relative_path.parts) != 1:
                raise ValueError(f"package must be a filename: {name}")
            source = source_file(package_dir, relative_path)
            validate_package(source, manifest)
            shutil.copyfile(source, stage / relative_path)
            staged.append(relative_path)

        checksum_path = stage / "SHA256SUMS"
        checksum_path.write_bytes(
            "".join(f"{sha256(stage / path)}  {path.as_posix()}\n"
                    for path in sorted(staged, key=lambda item: item.as_posix())).encode("utf-8")
        )
        staged.append(Path("SHA256SUMS"))

        temporary_output = output_path.with_suffix(".zip.tmp")
        try:
            with zipfile.ZipFile(temporary_output, "w") as archive:
                for relative_path in sorted(staged, key=lambda item: item.as_posix()):
                    source = stage / relative_path
                    target = PurePosixPath(folder_name) / PurePosixPath(
                        relative_path.as_posix()
                    )
                    add_to_zip(
                        archive,
                        source,
                        target,
                        executable=relative_path.name == "install.sh",
                    )
            temporary_output.replace(output_path)
        finally:
            if temporary_output.exists():
                temporary_output.unlink()

    return output_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "bundle",
        type=Path,
        help="bundle directory containing bundle.json",
    )
    parser.add_argument(
        "--package-dir",
        type=Path,
        default=ROOT / "packages",
        help="directory containing the published IPK files",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ROOT / "dist",
        help="directory for the generated ZIP",
    )
    args = parser.parse_args()
    try:
        result = build_bundle(
            args.bundle.resolve(), args.package_dir.resolve(), args.output_dir.resolve()
        )
    except (OSError, ValueError) as error:
        parser.exit(1, f"Bundle not created: {error}\n")
    print(result)


if __name__ == "__main__":
    main()
