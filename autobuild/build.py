#!/usr/bin/env python3
"""Build firmware and its immutable feed together in a disposable buildroot."""
import argparse
import gzip
import os
import re
import shutil
import subprocess
import tarfile
import tempfile
import time
import zipfile
from pathlib import Path

from common import (ARCHITECTURE, ROOTS, IMAGE_ASSETS, NAME, SHA, identity, feed_url, public_key, read_json,
                    read_release_notes, write_json, sha256, ipk, records, make_index, validate_closure,
                    validate_candidate, privacy)
from flash_bundle import create as create_flash_bundle


def run(*args, cwd=None, timeout=21600):
    print("stage: " + str(args[0]) + " " + " ".join(map(str, args[1:3])), flush=True)
    started = time.monotonic()
    subprocess.run(list(map(str, args)), cwd=cwd, check=True, timeout=timeout)
    print(f"stage elapsed: {time.monotonic() - started:.1f}s", flush=True)


def rewrite_config(text, settings):
    keys = set(settings)
    lines = [line for line in text.splitlines()
             if line.split("=", 1)[0] not in keys and
             not any(line == f"# {key} is not set" for key in keys)]
    lines += [f"# {key} is not set" if value == "n" else f"{key}={value}"
              for key, value in sorted(settings.items())]
    return "\n".join(lines) + "\n"


def copy_overlay(source, destination):
    shutil.copytree(source, destination, dirs_exist_ok=True, symlinks=True)
    for path in destination.rglob("*"):
        if path.is_file() and not path.is_symlink():
            data = path.read_bytes()
            if b"\0" not in data:
                path.write_bytes(data.replace(b"\r\n", b"\n"))
                if data.startswith(b"#!"):
                    path.chmod(0o755)


def apply_feed_patches(source, build, feed):
    """Apply repository-owned patches to an exact, freshly checked out feed."""
    patch_dir = source / "openwrt-feed" / "patches" / feed
    if not patch_dir.is_dir():
        return
    checkout = build / "feeds" / feed
    patches = sorted(patch_dir.glob("*.patch"))
    if not patches:
        raise ValueError("empty feed patch directory: " + feed)
    for patch in patches:
        run("patch", "-p1", "--fuzz=0", "--input=" + str(patch.resolve()), cwd=checkout)
    run("git", "diff", "--check", cwd=checkout)


def order_ipkg_outputs(build):
    """Prevent the legacy IPK recipe from running once per declared output."""
    path = build / "include/package-ipkg.mk"
    text = path.read_text()
    shared_rule = (
        "    $(PKG_INFO_DIR)/$(1).provides $$(IPKG_$(1)): "
        "$(STAMP_BUILT) $(INCLUDE_DIR)/package-ipkg.mk\n"
    )
    ipk_rule = "    $$(IPKG_$(1)): $(STAMP_BUILT) $(INCLUDE_DIR)/package-ipkg.mk\n"
    recipe_end = "\t@[ -f $$(IPKG_$(1)) ]\n\n    $(1)-clean:"
    ordered_end = (
        "\t@[ -f $$(IPKG_$(1)) ]\n\n"
        "    $(PKG_INFO_DIR)/$(1).provides: $$(IPKG_$(1))\n"
        "\t@[ -f $$@ ]\n\n"
        "    $(1)-clean:"
    )
    if text.count(shared_rule) != 1 or text.count(recipe_end) != 1:
        raise ValueError("parallel IPK output patch context changed")
    path.write_text(text.replace(shared_rule, ipk_rule).replace(recipe_end, ordered_end))


def prepare(source, build, cache, key, tag, lock):
    if build.exists():
        raise ValueError("refusing an existing buildroot")
    for name, upstream in lock["upstream"].items():
        if not re.fullmatch(r"[0-9a-f]{40}", upstream["revision"]) or not upstream["url"].startswith("https://"):
            raise ValueError("sources require HTTPS and exact revisions")
        destination = build if name == "openwrt" else build / "feeds" / name
        destination.mkdir(parents=True)
        run("git", "init", destination)
        run("git", "-C", destination, "fetch", "--depth=1", upstream["url"], upstream["revision"], timeout=600)
        run("git", "-C", destination, "checkout", "--detach", "FETCH_HEAD")
    (build / "version").write_text(tag + "\n")
    (build / "feeds.conf").write_text("".join(
        f"src-git {name} {s['url']}^{s['revision']}\n" for name, s in lock["upstream"].items() if name != "openwrt"))
    apply_feed_patches(source, build, "luci")
    run("./scripts/feeds", "update", "-i", cwd=build)
    run("./scripts/feeds", "install", "-a", cwd=build)
    order_ipkg_outputs(build)
    copy_overlay(source / "openwrt-feed/target/linux/rtkmipsel", build / "target/linux/rtkmipsel")
    copy_overlay(source / "openwrt-feed/package", build / "package")
    copy_overlay(source / "autobuild/package", build / "package")
    # iwpriv is a prebuilt MIPS ELF, not a shebang script. Its executable bit was
    # previously lost in hosted builds because the repository entry was 0644,
    # leaving netifd unable to configure either rtl8192cd radio. Do not depend on
    # checkout filesystem semantics for a runtime-critical binary.
    iwpriv = build / "target/linux/rtkmipsel/base-files/usr/sbin/iwpriv"
    if iwpriv.read_bytes()[:4] != b"\x7fELF":
        raise ValueError("iwpriv overlay is not an ELF executable")
    iwpriv.chmod(0o755)
    key_id, normalized = public_key(key)
    package = build / "package/hh71vm-feed"
    (package / "files/release.pub").write_bytes(normalized)
    (build / "hh71vm-release.mk").write_text(f"HH71VM_RELEASE:={tag}\nHH71VM_FEED_URL:={feed_url(tag)}\nHH71VM_KEY_ID:={key_id}\n")
    backend = build / "package/utils/modem-extra-tools/Makefile"
    text = backend.read_text()
    if "+iptables-mod-ipopt +kmod-ipt-ipopt" not in text:
        raise ValueError("modem dependency patch context changed")
    backend.write_text(text.replace("+iptables-mod-ipopt +kmod-ipt-ipopt", "+iptables-mod-ipopt +kmod-hh71vm-ipt-ipopt +hh71vm-feed"))
    # Never depend on executable bits inherited from a Windows checkout.
    driver = build / "target/linux/rtkmipsel/files/drivers/net/wireless/realtek/rtl8192cd/Makefile"
    driver.write_text(re.sub(r"(?m)^(\s*)\$\(obj\)/bin2c.pl", r"\1perl $(obj)/bin2c.pl", driver.read_text()))
    downloader = build / "scripts/download.pl"
    download_text = downloader.read_text()
    old = "curl -f --connect-timeout 20 --retry 5 --location --insecure"
    if old not in download_text:
        raise ValueError("download timeout patch context changed")
    downloader.write_text(download_text.replace(old, "curl -f --connect-timeout 20 --retry 2 --retry-max-time 600 --max-time 300 --speed-time 60 --speed-limit 1024 --location"))
    download_rules = build / "include/download.mk"
    text = download_rules.read_text()
    anchor = "  $(eval $(Download/$(1)))\n"
    if text.count(anchor) != 1:
        raise ValueError("download inventory patch context changed")
    text = text.replace(anchor, anchor + "  $(if $(HH71VM_DOWNLOAD_MANIFEST),$(file >>$(HH71VM_DOWNLOAD_MANIFEST),$(FILE) $(if $(filter default,$(call dl_method,$(URL),$(PROTO))),$(HASH),$(MIRROR_HASH))))\n")
    download_rules.write_text(text)
    for name, staging in (("host-build.mk", "STAGING_DIR_HOST"), ("package.mk", "STAGING_DIR")):
        path = build / "include" / name
        text = path.read_text()
        old = f"export CCACHE_DIR:=$({staging})/ccache"
        if text.count(old) != 1:
            raise ValueError("ccache directory patch context changed")
        path.write_text(text.replace(old, f"export CCACHE_DIR:=$(if $(HH71VM_CCACHE_DIR),$(HH71VM_CCACHE_DIR),$({staging})/ccache)"))
    base_hook = build / "target/linux/rtkmipsel/base-files.mk"
    base_hook.write_text(base_hook.read_text().replace(
        "echo '# so no server has them -- they exist only inside the image you flashed.';",
        "echo '# kernel modules are supplied by the matching HH71VM release feed.';").replace(
        "echo '# Install kernel modules by building a new image, not with opkg.';",
        "echo '# Do not add upstream core feeds or install foreign kernel modules.';"))
    run("sh", source / "openwrt-feed/scripts/prepare-build-host.sh", build)
    run("sh", build / "package/utils/modem-extra-tools/build-helper.sh", cwd=build)
    settings = lock["config"]
    (build / ".config").write_text(rewrite_config((source / "openwrt-feed/build.config").read_text(), settings))
    cache.mkdir(parents=True, exist_ok=True)
    (build / "dl").symlink_to(cache.resolve(), target_is_directory=True)
    run("make", "defconfig", cwd=build)
    config = (build / ".config").read_text()
    for name, value in settings.items():
        expected = f"# {name} is not set" if value == "n" else f"{name}={value}"
        if expected not in config.splitlines():
            raise ValueError("resolved config mismatch: " + name)
    if "CONFIG_KERNEL_BUILD_USER=\"openwrt\"" not in config or "CONFIG_KERNEL_BUILD_DOMAIN=\"build\"" not in config:
        raise ValueError("noncanonical kernel build identity")
    return normalized


def download_inventory(build):
    inventory = {}
    for line in (build / "download-manifest.txt").read_text().splitlines():
        parts = line.split()
        if len(parts) != 2 or not NAME.fullmatch(parts[0]) or not SHA.fullmatch(parts[1]):
            raise ValueError("source download lacks a pinned SHA-256")
        name, expected = parts
        if name in inventory and inventory[name] != expected:
            raise ValueError("source archive filename collision: " + name)
        inventory[name] = expected
    if not inventory:
        raise ValueError("source download inventory is empty")
    return inventory


def snapshot_inventory(build):
    snapshot = build / ".hh71vm-source-downloads"
    inventory = {}
    for line in (snapshot / "checksums.txt").read_text().splitlines():
        parts = line.split()
        if len(parts) != 2 or not NAME.fullmatch(parts[0]) or not SHA.fullmatch(parts[1]):
            raise ValueError("source snapshot lacks a pinned SHA-256")
        name, expected = parts
        if name in inventory:
            raise ValueError("duplicate source snapshot filename: " + name)
        inventory[name] = expected
    if not inventory:
        raise ValueError("source snapshot is empty")
    return inventory


def verify_downloads(build, repair=False):
    inventory = download_inventory(build)
    invalid = []
    for name, expected in inventory.items():
        path = build / "dl" / name
        if path.is_symlink() or not path.is_file() or sha256(path) != expected:
            invalid.append(name)
            if repair:
                path.unlink(missing_ok=True)
    if invalid and not repair:
        raise ValueError("source download checksum mismatch: " + ", ".join(invalid))
    return invalid


def snapshot_downloads(build):
    inventory = download_inventory(build)
    snapshot = build / ".hh71vm-source-downloads"
    snapshot.mkdir(exist_ok=False)
    for name, expected in sorted(inventory.items()):
        source = build / "dl" / name
        destination = snapshot / name
        if source.is_symlink() or not source.is_file() or sha256(source) != expected:
            raise ValueError("cannot snapshot unverified source download: " + name)
        shutil.copyfile(source, destination)
        if sha256(destination) != expected:
            raise ValueError("source snapshot copy mismatch: " + name)
    (snapshot / "checksums.txt").write_text(
        "".join(f"{name} {expected}\n" for name, expected in sorted(inventory.items())))
    return snapshot


def verify_download_snapshot(build):
    snapshot = build / ".hh71vm-source-downloads"
    invalid = []
    for name, expected in snapshot_inventory(build).items():
        path = snapshot / name
        if path.is_symlink() or not path.is_file() or sha256(path) != expected:
            invalid.append(name)
    if invalid:
        raise ValueError("source snapshot checksum mismatch: " + ", ".join(invalid))


def download(build, lock, jobs):
    for item in lock["prefetch"]:
        path = build / "dl" / item["filename"]
        if path.exists() and sha256(path) == item["sha256"]:
            continue
        temporary = path.with_suffix(path.suffix + ".download")
        try:
            run("curl", "--fail", "--location", "--proto", "=https", "--connect-timeout", "20",
                "--max-time", "300", "--retry", "2", "--output", temporary, item["url"], timeout=960)
            if sha256(temporary) != item["sha256"]:
                raise ValueError("download hash mismatch")
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)
    inventory = build / "download-manifest.txt"
    inventory.write_text("")
    os.environ["HH71VM_DOWNLOAD_MANIFEST"] = str(inventory)
    run("make", f"-j{jobs}", "download", "V=s", cwd=build, timeout=3600)
    if verify_downloads(build, repair=True):
        run("make", f"-j{jobs}", "download", "V=s", cwd=build, timeout=3600)
    verify_downloads(build)
    snapshot_downloads(build)
    os.environ.pop("HH71VM_DOWNLOAD_MANIFEST", None)


def unique(paths, description):
    paths = list(paths)
    if len(paths) != 1:
        raise ValueError("expected one " + description)
    return paths[0]


def solver_probe(build, root, output):
    with tempfile.TemporaryDirectory(prefix="opkg-probe-") as temporary:
        probe = Path(temporary)
        for directory in ("usr/lib/opkg/info", "etc/opkg", "var/opkg-lists", "var/lock", "tmp"):
            (probe / directory).mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / "usr/lib/opkg/status", probe / "usr/lib/opkg/status")
        for file_list in (root / "usr/lib/opkg/info").glob("*.list"):
            shutil.copyfile(file_list, probe / "usr/lib/opkg/info" / file_list.name)
        shutil.copyfile(output / "Packages", probe / "var/opkg-lists/hh71vm")
        config = probe / "etc/opkg.conf"
        config.write_text(f"dest root /\narch all 100\narch {ARCHITECTURE} 200\nlists_dir ext /var/opkg-lists\nsrc hh71vm file://{output}\n")
        result = subprocess.run([str(build / "staging_dir/host/bin/opkg"), "--offline-root", str(probe),
                                 "--conf", str(config), "--noaction", "install", *ROOTS],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        if result.returncode or re.search(r"Unknown package|Cannot satisfy|Collected errors|incompatible|Installing kernel", result.stdout, re.I):
            raise ValueError(f"offline opkg dependency probe failed ({result.returncode}): " + result.stdout[-3000:])
        if not all(name in result.stdout for name in ROOTS):
            raise ValueError("offline opkg did not resolve all optional roots")


def inspect_images(build, output, tag, key):
    from inspect_image import inspect_release_images
    return inspect_release_images(build, output, tag, key)


def collect(source, build, output, args, key, lock):
    if output.exists() and any(output.iterdir()):
        raise ValueError("output directory must be empty")
    output.mkdir(parents=True, exist_ok=True)
    tag = identity(args.commit, args.run_id, args.attempt)
    root = unique((build / "build_dir").glob("target-*/root-rtkmipsel"), "root filesystem")
    installed = records((root / "usr/lib/opkg/status").read_text())
    packages, seen = [], {}
    for path in sorted((build / "bin").rglob("*.ipk")):
        metadata = ipk(path)
        name = metadata["Package"]
        if name == "kernel":
            continue
        if name in seen:
            if sha256(path) != seen[name]:
                raise ValueError("conflicting package outputs: " + name)
            continue
        seen[name] = sha256(path)
        destination = output / path.name
        shutil.copyfile(path, destination)
        packages.append((destination, metadata))
    kernel = validate_closure([r for _, r in packages], installed)
    vermagic = unique((build / "build_dir").glob("target-*/linux-rtkmipsel_*/linux-*/.vermagic"), "kernel ABI")
    if not kernel.endswith("-" + vermagic.read_text().strip()):
        raise ValueError("kernel metadata and compiled ABI differ")
    index = make_index(packages)
    (output / "Packages").write_bytes(index)
    (output / "Packages.gz").write_bytes(gzip.compress(index, compresslevel=9, mtime=0))
    write_json(output / "image-packages.json", installed)
    (output / "hh71vm-feed.pub").write_bytes(key)
    (output / "build.config").write_bytes((build / ".config").read_bytes())
    write_json(output / "source-lock.json", lock)
    write_json(output / "build-environment.json", {
        "machine": os.uname().machine,
        "packages": subprocess.check_output(["dpkg-query", "-W", "-f=${Package}=${Version}\n"], text=True).splitlines(),
    })
    binaries = build / "bin/targets/rtkmipsel/rtl8197f"
    for suffix, asset_name in IMAGE_ASSETS.items():
        path = unique(binaries.glob(f"*-hh71vm-{suffix}.bin"), suffix + " image")
        shutil.copyfile(path, output / asset_name)
    evidence = inspect_images(build, output, tag, key)
    solver_probe(build, root, output)
    write_json(output / "build-evidence.json", {"image_inspection": evidence, "opkg_solver": "PASS", "hardware": "NOT_RUN"})
    create_flash_bundle(source, output, tag, args.commit, args.run_id, args.attempt)
    with zipfile.ZipFile(output / "packages-bundle.zip", "w", zipfile.ZIP_DEFLATED) as bundle:
        for path, _ in packages:
            item = zipfile.ZipInfo(path.name, (2020, 1, 1, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            bundle.writestr(item, path.read_bytes())
    # Source archives contain only public source trees and upstream downloads.
    with (output / "source-delta.tar.gz").open("wb") as stream:
        archive = subprocess.Popen(["git", "-C", str(source), "archive", args.commit, "autobuild",
                                    "openwrt-feed", "tools", "docs", "release-notes.json", "LICENSE",
                                    "LICENSE-APACHE-2.0", "LICENSE-ISC", "LICENSING.md"], stdout=subprocess.PIPE)
        with gzip.GzipFile(fileobj=stream, mode="wb", mtime=0) as zipped:
            shutil.copyfileobj(archive.stdout, zipped)
        if archive.wait():
            raise ValueError("source export failed")
    with tarfile.open(output / "upstream-buildsystem.tar.gz", "w:gz") as combined:
        for name, upstream in lock["upstream"].items():
            tree = build if name == "openwrt" else build / "feeds" / name
            process = subprocess.Popen(["git", "-C", str(tree), "archive", "--prefix=" + name + "/", upstream["revision"]], stdout=subprocess.PIPE)
            with tarfile.open(fileobj=process.stdout, mode="r|") as archive:
                for member in archive:
                    combined.addfile(member, archive.extractfile(member) if member.isfile() else None)
            if process.wait():
                raise ValueError("upstream buildsystem source export failed")
    verify_download_snapshot(build)
    sources = snapshot_inventory(build)
    write_json(output / "download-checksums.json", sources)
    with tarfile.open(output / "upstream-sources.tar.gz", "w:gz") as archive:
        def canonical_owner(info):
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            return info
        for name in sorted(sources):
            archive.add(build / ".hh71vm-source-downloads" / name, arcname=name,
                        recursive=False, filter=canonical_owner)
    for path in output.iterdir():
        if path.stat().st_size >= 2 * 1024**3:
            raise ValueError("asset exceeds GitHub's per-file size limit")
    manifest = {"schema": 1, "tag": tag, "source_commit": args.commit, "run_id": args.run_id,
                "run_attempt": args.attempt, "architecture": ARCHITECTURE, "kernel": kernel,
                "feed_url": feed_url(tag), "key_id": public_key(key)[0],
                "changelog": read_release_notes(source / "release-notes.json"),
                "files": {p.name: sha256(p) for p in sorted(output.iterdir())}}
    write_json(output / "release.json", manifest)
    validate_candidate(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("/src"))
    parser.add_argument("--build", type=Path, default=Path("/build/openwrt"))
    parser.add_argument("--output", type=Path, default=Path("/output"))
    parser.add_argument("--downloads", type=Path, default=Path("/cache/downloads"))
    parser.add_argument("--commit", required=True)
    parser.add_argument("--run-id", type=int, required=True)
    parser.add_argument("--attempt", type=int, required=True)
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 64:
        parser.error("jobs must be between 1 and 64")
    if os.geteuid() == 0:
        parser.error("build as an unprivileged user")
    if subprocess.check_output(["git", "-C", str(args.source), "rev-parse", "HEAD"], text=True).strip() != args.commit:
        parser.error("checkout differs from requested source commit")
    lock = read_json(args.source / "autobuild/lock.json")
    key = os.environ["HH71VM_FEED_PUBLIC_KEY"].encode()
    tag = identity(args.commit, args.run_id, args.attempt)
    normalized = prepare(args.source, args.build, args.downloads, key, tag, lock)
    download(args.build, lock, args.jobs)
    run("make", f"-j{args.jobs}", "V=s", cwd=args.build)
    collect(args.source, args.build, args.output, args, normalized, lock)


if __name__ == "__main__":
    main()
