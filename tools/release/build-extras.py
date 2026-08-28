#!/usr/bin/env python3
# Copyright 2026 sowarden
# SPDX-License-Identifier: Apache-2.0
"""Build all extras from pinned OpenWrt sources in a fresh build directory."""

import argparse
import importlib.util
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from extras_common import ROOT, compatibility, read_json, release_policy, write_json
from package_metadata import control_fields, read_control, sha256

spec = importlib.util.spec_from_file_location("bundle_builder", ROOT / "tools/release/build-package-bundle.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def run(*args, cwd=None):
    print("+ " + " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, check=True)


def recipes(root):
    result = []
    sources = {}
    for path in sorted((root / "extras").glob("*/bundle.json")):
        manifest = read_json(path)
        if manifest.get("schema") == 1:
            manifest = builder.load_manifest(path.parent)
        elif manifest.get("schema") != 2:
            raise ValueError(f"unsupported recipe schema: {path}")
        if (not re.fullmatch(r"[a-z0-9][a-z0-9+-]*", manifest.get("name", "")) or
                manifest["name"] != path.parent.name):
            raise ValueError(f"bundle name must match its directory: {path}")
        if not isinstance(manifest.get("files"), list) or not manifest["files"]:
            raise ValueError(f"missing bundle files: {path}")
        for filename in manifest["files"]:
            builder.safe_relative_path(filename)
            builder.source_file(path.parent, filename)
        recipe = manifest.get("build", {})
        packages = recipe.get("packages", {})
        if not packages or recipe.get("version_package") not in packages:
            raise ValueError(f"missing build recipe: {path}")
        for package, source in packages.items():
            if not re.fullmatch(r"[a-z0-9][a-z0-9+-]*", package):
                raise ValueError(f"invalid package name: {package}")
            builder.safe_relative_path(source)
            if not source.startswith("package/"):
                raise ValueError(f"package source must be under package/: {source}")
            builder.source_file(root / "openwrt-feed", source + "/Makefile")
            if package in sources and sources[package] != source:
                raise ValueError(f"conflicting source directories for {package}")
            sources[package] = source
        for script in recipe.get("prepare", []):
            builder.safe_relative_path(script)
            builder.source_file(root / "openwrt-feed", script)
        result.append((path.parent, manifest))
    if not result:
        raise ValueError("no bundle recipes found")
    return result


def clone(source, destination, cache=None):
    if cache and (cache / ".git").is_dir():
        run("git", "clone", "--shared", "--no-checkout", cache, destination)
    else:
        destination.mkdir(parents=True)
        run("git", "init", destination)
        run("git", "-C", destination, "remote", "add", "origin", source["url"])
        run("git", "-C", destination, "fetch", "--depth=1", "origin", source["revision"])
    run("git", "-C", destination, "checkout", "--detach", source["revision"])


def copy_overlay(source, destination):
    shutil.copytree(source, destination, dirs_exist_ok=True)
    for path in source.rglob("*"):
        if path.is_file():
            data = path.read_bytes()
            if b"\0" not in data and b"\r\n" in data:
                (destination / path.relative_to(source)).write_bytes(data.replace(b"\r\n", b"\n"))


def prepare(root, build, lock, entries, cache=None):
    if build.exists():
        raise ValueError("build directory must not exist; use a new directory")
    clone(lock["upstream"]["openwrt"], build, cache)
    feeds = []
    for name, source in lock["upstream"].items():
        if name == "openwrt":
            continue
        clone(source, build / "feeds" / name, cache / "feeds" / name if cache else None)
        feeds.append(f"src-git {name} {source['url']}^{source['revision']}\n")
    (build / "feeds.conf").write_text("".join(feeds), encoding="utf-8")
    run("./scripts/feeds", "update", "-i", cwd=build)
    run("./scripts/feeds", "install", "-a", cwd=build)
    copy_overlay(root / "openwrt-feed/target/linux/rtkmipsel", build / "target/linux/rtkmipsel")
    copy_overlay(root / "openwrt-feed/package", build / "package")
    run("sh", root / "openwrt-feed/scripts/prepare-build-host.sh", build)
    (build / ".config").write_bytes((root / "openwrt-feed/build.config").read_bytes().replace(b"\r\n", b"\n"))
    scripts = sorted({script for _, item in entries for script in item["build"].get("prepare", [])})
    for script in scripts:
        run("sh", build / script, cwd=build)
    for directory, _ in entries:
        installer = directory / "install.sh"
        if installer.exists():
            run("sh", "-n", installer)
    run("make", "defconfig", cwd=build)
    config = (build / ".config").read_text()
    for _, manifest in entries:
        for package in manifest["build"]["packages"]:
            if f"CONFIG_PACKAGE_{package}=m\n" not in config:
                raise ValueError(f"select optional package as =m in build.config: {package}")


def compile_packages(build, entries, jobs):
    for target in ("tools/install", "toolchain/install", "target/linux/compile"):
        run("make", f"-j{jobs}", target, "V=s", cwd=build)
    for source in sorted({s for _, item in entries for s in item["build"]["packages"].values()}):
        run("make", f"-j{jobs}", source + "/compile", "V=s", cwd=build)


def collect(root, build, output, lock, entries, commit, run_id, attempt, number):
    if output.exists() and any(output.iterdir()):
        raise ValueError("candidate output directory must be empty")
    output.mkdir(parents=True, exist_ok=True)
    kernel_dirs = list((build / "build_dir").glob("target-*/linux-rtkmipsel_*/linux-*/.vermagic"))
    if len(kernel_dirs) != 1 or not lock["kernel"].endswith("-" + kernel_dirs[0].read_text().strip()):
        raise ValueError("rebuilt kernel ABI does not match the published firmware")
    available = {}
    for path in (build / "bin").rglob("*.ipk"):
        fields = control_fields(read_control(path))
        key = fields["Package"]
        if key in available and sha256(available[key][0]) != sha256(path):
            raise ValueError(f"ambiguous built package: {key}")
        available[key] = (path, fields)
    candidate = {
        "schema": 1, "hardware_tested": False, "source_commit": commit,
        "run_id": run_id, "run_attempt": attempt, "run_number": number,
        "firmware": lock, "assets": {}, "bundles": {},
    }
    with tempfile.TemporaryDirectory(prefix="extras-collect-") as temporary:
        stage = Path(temporary)
        for directory, old in entries:
            manifest = dict(old)
            selected = [available[name] for name in manifest["build"]["packages"]]
            manifest.update(
                schema=1,
                version=available[manifest["build"]["version_package"]][1]["Version"],
                kernel=lock["kernel"], architecture=lock["architecture"], firmware_build=lock["firmware_build"],
                archive_suffix=f"hh71vm-{lock['firmware_build']}-r{run_id}-a{attempt}",
                packages=[path.name for path, _ in selected],
                sha256={path.name: sha256(path) for path, _ in selected},
            )
            bundle = stage / manifest["name"]
            shutil.copytree(directory, bundle)
            write_json(bundle / "bundle.json", manifest)
            packages = bundle / "ipks"
            packages.mkdir()
            for path, _ in selected:
                shutil.copyfile(path, packages / path.name)
            archive = builder.build_bundle(bundle, packages, output)
            candidate["assets"][archive.name] = sha256(archive)
            candidate["bundles"][manifest["name"]] = {
                "version": manifest["version"], "packages": manifest["sha256"], "archive": archive.name,
            }
    write_json(output / "candidate.json", candidate)
    sums = dict(candidate["assets"], **{"candidate.json": sha256(output / "candidate.json")})
    (output / "SHA256SUMS").write_bytes(
        "".join(f"{digest}  {name}\n" for name, digest in sorted(sums.items())).encode("utf-8"))
    print("Candidate SHA-256: " + sums["candidate.json"])
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a") as stream:
            stream.write(f"candidate_sha256={sums['candidate.json']}\n")
            stream.write(f"auto_publish={str(release_policy(root)).lower()}\n")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
            stream.write(f"## Extras candidate (not hardware-tested)\n\nFirmware: `{lock['firmware_build']}`\n\n")
            stream.write(f"Kernel: `{lock['kernel']}`\n\nCandidate SHA-256: `{sums['candidate.json']}`\n\n")
            stream.write("Download the Actions artifact, test these exact bundles, then run **Publish tested extras**.\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "dist/candidate")
    parser.add_argument("--source-cache", type=Path, help="local Git objects only; never reuses compiled output")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--phase", choices=("all", "prepare", "compile", "collect"), default="all")
    args = parser.parse_args()
    lock, entries = compatibility(ROOT), recipes(ROOT)
    release_policy(ROOT)
    if args.phase in ("all", "prepare"):
        prepare(ROOT, args.build_dir, lock, entries, args.source_cache)
    if args.phase in ("all", "compile"):
        compile_packages(args.build_dir, entries, args.jobs)
    if args.phase in ("all", "collect"):
        commit = os.environ.get("GITHUB_SHA") or subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        collect(ROOT, args.build_dir, args.output, lock, entries, commit,
                int(os.environ.get("GITHUB_RUN_ID", 1)), int(os.environ.get("GITHUB_RUN_ATTEMPT", 1)),
                int(os.environ.get("GITHUB_RUN_NUMBER", 1)))


if __name__ == "__main__":
    main()
