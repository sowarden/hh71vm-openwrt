"""Inspect the files actually embedded in HH71VM firmware images."""
import gzip
import lzma
import struct
import subprocess
import tempfile
from pathlib import Path, PurePosixPath

from common import IMAGE_ASSETS, public_key, privacy, records, sha256, read_json, feed_url


class ImageFiles(dict):
    """Image file bodies plus the exact modes recorded by CPIO or SquashFS."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.modes = {}


def cpio_files(blob):
    offset, result = 0, ImageFiles()
    while offset + 110 <= len(blob):
        if blob[offset:offset + 6] not in (b"070701", b"070702"):
            raise ValueError("invalid initramfs CPIO header")
        values = [int(blob[offset + i:offset + i + 8], 16) for i in range(6, 110, 8)]
        mode, size, namesize = values[1], values[6], values[11]
        start = offset + 110
        if not 1 <= namesize <= 4096 or start + namesize > len(blob):
            raise ValueError("invalid initramfs filename")
        name = blob[start:start + namesize - 1].decode()
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError("unsafe initramfs path")
        start = (start + namesize + 3) & ~3
        if start + size > len(blob):
            raise ValueError("truncated initramfs file")
        if name == "TRAILER!!!":
            return result
        if mode & 0o170000 == 0o100000:
            normalized = name.removeprefix("./")
            result[normalized] = blob[start:start + size]
            result.modes[normalized] = mode
        offset = (start + size + 3) & ~3
    raise ValueError("initramfs trailer missing")


def check_files(files, tag, key, expected_installed=None):
    for body in files.values():
        privacy(body)
    desc = files["usr/share/hh71vm-feed/release.conf"].decode()
    if f"release={tag}\n" not in desc:
        raise ValueError("image has a different release identity")
    if files["usr/share/hh71vm-feed/release.pub"] != key:
        raise ValueError("image public key mismatch")
    key_id = public_key(key)[0]
    if files["etc/opkg/keys/" + key_id] != key or "key_id=" + key_id not in desc:
        raise ValueError("image trust anchor mismatch")
    if b"option check_signature" not in files["etc/opkg.conf"]:
        raise ValueError("image signature checking is disabled")
    if ("src/gz hh71vm " + feed_url(tag) + "\n").encode() not in files["etc/opkg/hh71vm.conf"]:
        raise ValueError("image feed URL mismatch")
    for name, source in (("usr/libexec/hh71vm-feed-reconcile", "reconcile.sh"),
                         ("etc/uci-defaults/99-hh71vm-feed", "99-hh71vm-feed")):
        expected = (Path(__file__).parent / "package/hh71vm-feed/files" / source).read_bytes().replace(b"\r\n", b"\n")
        if files.get(name) != expected:
            raise ValueError("image feed migration code differs from source")
    updater = (Path(__file__).parents[1] /
               "openwrt-feed/target/linux/rtkmipsel/base-files/usr/sbin/autosysupgrade")
    if files.get("usr/sbin/autosysupgrade") != updater.read_bytes().replace(b"\r\n", b"\n"):
        raise ValueError("image autosysupgrade differs from source")
    frontend = (Path(__file__).parents[1] /
                "openwrt-feed/target/linux/rtkmipsel/base-files/www/luci-static/resources/hh71vm/updater.js")
    if files.get("www/luci-static/resources/hh71vm/updater.js") != frontend.read_bytes().replace(b"\r\n", b"\n"):
        raise ValueError("image LuCI updater differs from source")
    if b"'require hh71vm.updater as updater';" not in files.get("www/luci-static/resources/view/system/flash.js", b""):
        raise ValueError("image flash page does not load the firmware updater")
    if b'"/usr/sbin/autosysupgrade": [ "exec" ]' not in files.get("usr/share/rpcd/acl.d/luci-base.json", b""):
        raise ValueError("image LuCI ACL does not authorize the firmware updater")
    installed = records(files["usr/lib/opkg/status"].decode())
    if expected_installed is not None and sorted(installed, key=lambda r: r["Package"]) != sorted(expected_installed, key=lambda r: r["Package"]):
        raise ValueError("manifest package inventory differs from embedded image")
    kernel = next(r["Version"] for r in installed if r["Package"] == "kernel")
    if f"kernel={kernel}\n" not in desc:
        raise ValueError("image descriptor kernel mismatch")
    xtables = files["usr/sbin/xtables-legacy-multi"]
    if xtables[:6] != b"\x7fELF\x01\x01" or xtables[18:20] != b"\x08\x00":
        raise ValueError("image xtables is not MIPS ELF")
    iwpriv = files.get("usr/sbin/iwpriv", b"")
    if iwpriv[:4] != b"\x7fELF":
        raise ValueError("image iwpriv is not ELF")
    if not getattr(files, "modes", {}).get("usr/sbin/iwpriv", 0) & 0o111:
        raise ValueError("image iwpriv is not executable")
    check_executable_scripts(files)
    return kernel


# Directories whose contents are EXECUTED. Everything under lib/, etc/hotplug.d/ and
# lib/upgrade/ is sourced by another shell instead, and is legitimately 0644.
EXECUTED_DIRECTORIES = (
    "bin/", "sbin/", "usr/bin/", "usr/sbin/", "usr/libexec/",
    "etc/init.d/", "etc/uci-defaults/", "etc/rc.button/",
)


def check_executable_scripts(files):
    """A script that cannot be executed is a broken image, not a cosmetic detail.

    The iwpriv check above was written for one instance of this defect and only ever
    covered that one file. On 2026-09-05 the same class of defect shipped
    /usr/sbin/hh71vm-modemd at 0644 - the image was otherwise correct, and the router
    reported the Qualcomm control channel as down because the init script's `[ -x ]`
    guard skipped the daemon in silence. Fail the build instead of the router.
    """
    modes = getattr(files, "modes", {})
    broken = sorted(name for name, body in files.items()
                    if body[:2] == b"#!"
                    and name.startswith(EXECUTED_DIRECTORIES)
                    and not modes.get(name, 0) & 0o111)
    if broken:
        raise ValueError("image ships non-executable scripts: " + ", ".join(broken))


def squashfs_files(path, offset):
    with tempfile.TemporaryDirectory(prefix="hh71vm-image-") as temporary:
        root = Path(temporary) / "root"
        subprocess.run(["unsquashfs", "-no-progress", "-no-xattrs", "-d", str(root), "-o", str(offset), "-excludes", str(path), "dev"],
                       check=True, stdout=subprocess.DEVNULL, timeout=120)
        result = ImageFiles()
        for item in root.rglob("*"):
            if item.is_file() and not item.is_symlink():
                name = item.relative_to(root).as_posix()
                result[name] = item.read_bytes()
                result.modes[name] = item.stat().st_mode
        return result


def inspect_release_images(build, output, tag, key, expected_kernel=None):
    sysupgrade = output / IMAGE_ASSETS["sysupgrade"]
    fwupg = output / IMAGE_ASSETS["fwupg"]
    ram = output / IMAGE_ASSETS["nfjrom"]
    if not all(path.is_file() and not path.is_symlink() for path in (sysupgrade, fwupg, ram)):
        raise ValueError("required release image is missing or invalid")
    sysdata, fwdata, ramdata = sysupgrade.read_bytes(), fwupg.read_bytes(), ram.read_bytes()
    if len(sysdata) > 6094848 or len(sysdata) <= 2949120 or sysdata[:4] != b"cr6c":
        raise ValueError("invalid sysupgrade size/header")
    if sysdata[2949120:2949124] != b"hsqs" or fwdata[:4] != b"cr6c":
        raise ValueError("invalid firmware layout")
    kernel_size = struct.unpack(">I", fwdata[12:16])[0] + 16
    if not 16 < kernel_size <= 2949120:
        raise ValueError("kernel exceeds its flash partition")
    if fwdata[kernel_size:kernel_size + 4] != b"r6cr":
        raise ValueError("missing fwupg rootfs section")
    root_offset = kernel_size + 16
    root_size = struct.unpack(">I", fwdata[kernel_size + 12:root_offset])[0]
    if root_size > 3145728 or root_offset + root_size != len(fwdata):
        raise ValueError("invalid rootfs section length")
    if struct.unpack(">I", fwdata[root_offset + 8:root_offset + 12])[0] + 642 != root_size:
        raise ValueError("invalid Realtek rootfs length stamp")
    for body in (fwdata[16:kernel_size], fwdata[root_offset:]):
        if len(body) % 2 or sum(value[0] for value in struct.iter_unpack(">H", body)) & 0xffff:
            raise ValueError("Realtek firmware section checksum mismatch")
    if fwdata[:kernel_size] != sysdata[:kernel_size] or not sysdata[2949120:].startswith(fwdata[root_offset:]):
        raise ValueError("firmware containers do not share the same kernel/rootfs")
    expected_installed = read_json(output / "image-packages.json")
    kernels = [check_files(squashfs_files(sysupgrade, 2949120), tag, key, expected_installed),
               check_files(squashfs_files(fwupg, root_offset), tag, key, expected_installed)]
    if build is None:
        # Locate the LZMA-alone stream in the RAM loader without trusting offsets
        # or executables supplied by the builder. Limit decompression and memory.
        images = []
        for offset in range(16, min(len(ramdata) - 13, 131072)):
            if ramdata[offset] not in (0x5d, 0x6d):
                continue
            if int.from_bytes(ramdata[offset + 1:offset + 5], "little") not in (1 << n for n in range(16, 27)):
                continue
            try:
                decoder = lzma.LZMADecompressor(format=lzma.FORMAT_ALONE, memlimit=128 * 1024**2)
                body = decoder.decompress(ramdata[offset:], max_length=64 * 1024**2)
                if len(body) < 64 * 1024**2 and decoder.eof:
                    images.append(body)
            except lzma.LZMAError:
                pass
        if len(images) != 1:
            raise ValueError("RAM image compressed kernel is ambiguous")
        binary = images[0]
        position = binary.find(b"070701")
        candidates = []
        while position >= 0:
            try:
                found = cpio_files(binary[position:])
                if {"usr/share/hh71vm-feed/release.conf", "etc/opkg.conf", "usr/sbin/xtables-legacy-multi"} <= set(found):
                    candidates.append(found)
                    break
            except (ValueError, UnicodeDecodeError):
                pass
            position = binary.find(b"070701", position + 6)
        if len(candidates) != 1:
            raise ValueError("RAM image must contain one uncompressed initramfs")
        kernels.append(check_files(candidates[0], tag, key, expected_installed))
    else:
        kernel_dirs = list((build / "build_dir").glob("target-*/linux-rtkmipsel_*/linux-*/usr/initramfs_data.cpio"))
        compressed = list((build / "build_dir").glob("target-*/linux-rtkmipsel_*/vmlinux-initramfs.bin.lzma"))
        if len(kernel_dirs) != 1 or len(compressed) != 1:
            raise ValueError("expected uncompressed initramfs and matching RAM kernel")
        packed = compressed[0].read_bytes()
        if packed not in ramdata:
            raise ValueError("RAM image differs from compiled kernel")
        raw = lzma.decompress(packed, format=lzma.FORMAT_ALONE)
        cpio = kernel_dirs[0].read_bytes()
        if cpio not in raw:
            raise ValueError("compiled kernel does not contain the inspected initramfs")
        kernels.append(check_files(cpio_files(cpio), tag, key, expected_installed))
    if len(set(kernels)) != 1:
        raise ValueError("firmware image ABIs differ")
    if expected_kernel and kernels[0] != expected_kernel:
        raise ValueError("manifest ABI differs from embedded image ABI")
    return {p.name: sha256(p) for p in (sysupgrade, fwupg, ram)}
