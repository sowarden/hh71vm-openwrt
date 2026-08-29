#!/usr/bin/env python3
"""Sign and publish an exact firmware/feed candidate without rebuilding."""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
import time
import urllib.request
from pathlib import Path

from common import REPOSITORY, read_json, sha256, validate_candidate, write_json
from inspect_image import inspect_release_images


def checksum_text(directory):
    return "".join(f"{sha256(p)}  {p.name}\n" for p in sorted(directory.iterdir()) if p.name != "SHA256SUMS")


def verify_signatures(directory):
    for message in ("Packages", "release.json"):
        signature = "Packages.sig" if message == "Packages" else "release.json.sig"
        subprocess.run(["usign", "-V", "-p", str(directory / "hh71vm-feed.pub"), "-m", str(directory / message),
                        "-x", str(directory / signature)], check=True, timeout=30)
    if (directory / "SHA256SUMS").read_text() != checksum_text(directory):
        raise ValueError("release checksum file mismatch")


def sign(directory, tag, commit, expected_key, expected_run_id=None):
    # Remove the secret from child process environments before any inspection.
    secret = os.environ.pop("HH71VM_FEED_SIGNING_KEY", "")
    manifest = validate_candidate(directory, tag, commit)
    run_id = str(expected_run_id) if expected_run_id is not None else os.environ.get("GITHUB_RUN_ID")
    if str(manifest["run_id"]) != run_id:
        raise ValueError("candidate belongs to a different workflow run")
    if (directory / "hh71vm-feed.pub").read_bytes() != expected_key:
        raise ValueError("builder changed the configured public key")
    inspect_release_images(None, directory, tag, expected_key, manifest["kernel"])
    if not secret:
        raise ValueError("release signing secret is not configured")
    with tempfile.TemporaryDirectory(prefix="hh71vm-sign-") as temporary:
        private = Path(temporary) / "signing.key"
        fd = os.open(private, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w") as stream:
            stream.write(secret.rstrip() + "\n")
        secret = ""
        fingerprint = subprocess.check_output(["usign", "-F", "-s", str(private)], text=True, timeout=30).strip()
        if fingerprint != manifest["key_id"]:
            raise ValueError("signing key does not match image trust anchor")
        for message, signature in (("Packages", "Packages.sig"), ("release.json", "release.json.sig")):
            subprocess.run(["usign", "-S", "-s", str(private), "-m", str(directory / message),
                            "-x", str(directory / signature), "-c", "HH71VM release"], check=True, timeout=30)
    (directory / "SHA256SUMS").write_text(checksum_text(directory))
    verify_signatures(directory)
    validate_candidate(directory, tag, commit, signed=True)


class GitHub:
    def api(self, endpoint, method="GET", payload=None, missing=False):
        args = ["gh", "api", f"repos/{REPOSITORY}/{endpoint}", "--method", method]
        if payload is not None:
            args += ["--input", "-"]
        result = subprocess.run(args, input=json.dumps(payload) if payload is not None else None,
                                capture_output=True, text=True, timeout=120)
        if result.returncode:
            if missing and "(HTTP 404)" in result.stderr:
                return None
            raise RuntimeError("GitHub API request failed: " + endpoint)
        return json.loads(result.stdout or "null")

    def assets(self, release_id):
        result, page = [], 1
        while True:
            batch = self.api(f"releases/{release_id}/assets?per_page=100&page={page}")
            result += batch
            if len(batch) < 100:
                return result
            page += 1

    def find_release(self, tag):
        release = self.api("releases/tags/" + tag, missing=True)
        if release:
            return release
        page = 1
        while True:
            batch = self.api(f"releases?per_page=100&page={page}")
            matches = [r for r in batch if r["tag_name"] == tag]
            if matches:
                if len(matches) != 1:
                    raise ValueError("duplicate release identity")
                return matches[0]
            if len(batch) < 100:
                return None
            page += 1

    def upload(self, tag, path):
        subprocess.run(["gh", "release", "upload", tag, str(path), "--repo", REPOSITORY], check=True, timeout=1800)

    def asset_hash(self, asset):
        with tempfile.TemporaryFile() as stream:
            subprocess.run(["gh", "api", f"repos/{REPOSITORY}/releases/assets/{asset['id']}",
                            "-H", "Accept: application/octet-stream"], stdout=stream,
                           stderr=subprocess.PIPE, check=True, timeout=1800)
            stream.seek(0)
            result = hashlib.sha256()
            for block in iter(lambda: stream.read(1048576), b""):
                result.update(block)
            return result.hexdigest()

    def anonymous_hash(self, url):
        for attempt in range(4):
            try:
                result = hashlib.sha256()
                with urllib.request.urlopen(url, timeout=60) as stream:
                    for block in iter(lambda: stream.read(1048576), b""):
                        result.update(block)
                return result.hexdigest()
            except OSError:
                if attempt == 3:
                    raise RuntimeError("anonymous release readback failed") from None
                time.sleep(2 ** attempt)


def release_body(manifest, marker):
    return (f"HH71VM OpenWrt build `{manifest['tag']}`\n\n"
            "Automated build from the published source revision.\n\n"
            f"Kernel ABI: `{manifest['kernel']}`. Architecture: `{manifest['architecture']}`.\n\n"
            "The images include the signed package feed for this build.\n\n"
            "```sh\nopkg update\nopkg install luci-app-modem-extra-tools\n"
            "opkg install luci-proto-wireguard\n```\n\n"
            "Older images keep their own feeds. Do not force dependencies or run a global package upgrade.\n\n"
            "Use `sysupgrade.bin` to update OpenWrt; follow the installation guide for first installation. "
            "The `nfjrom.bin` image is for RAM boot.\n\n"
            f"[Installation](https://github.com/{REPOSITORY}/blob/{manifest['source_commit']}/docs/flash-install.md)\n\n{marker}\n")


def retire_transfer_artifact(github, artifact_id, manifest):
    if not artifact_id or artifact_id < 1:
        raise ValueError("missing candidate artifact identity")
    artifact = github.api(f"actions/artifacts/{artifact_id}")
    if artifact["name"] != manifest["tag"] or artifact["workflow_run"]["id"] != manifest["run_id"]:
        raise ValueError("refusing to delete an unrelated transfer artifact")
    github.api(f"actions/artifacts/{artifact_id}", "DELETE")


def publish(directory, manifest, github, prerelease=True):
    tag = manifest["tag"]
    marker = "<!-- hh71vm-release:" + sha256(directory / "release.json") + " -->"
    release = github.find_release(tag)
    ref = github.api("git/ref/tags/" + tag, missing=True)
    if ref and (ref["object"]["type"] != "commit" or ref["object"]["sha"] != manifest["source_commit"]):
        raise ValueError("existing tag points to another source")
    if release:
        if marker not in release.get("body", "") or release["tag_name"] != tag:
            raise ValueError("unmanaged release or different manifest")
        if not release["draft"] and not release.get("immutable"):
            raise ValueError("existing public release is not immutable")
    else:
        if ref:
            raise ValueError("tag exists without a matching managed release")
        release = github.api("releases", "POST", {"tag_name": tag, "target_commitish": manifest["source_commit"],
            "name": tag, "body": release_body(manifest, marker), "draft": True,
            "prerelease": prerelease, "make_latest": "false"})
    local = {p.name: p for p in directory.iterdir()}
    remote = github.assets(release["id"])
    if len({a["name"] for a in remote}) != len(remote) or set(a["name"] for a in remote) - set(local):
        raise ValueError("unexpected release assets; no deletion performed")
    for asset in remote:
        if github.asset_hash(asset) != sha256(local[asset["name"]]):
            raise ValueError("existing asset differs; overwrite refused")
    missing = set(local) - {a["name"] for a in remote}
    if missing and not release["draft"]:
        raise ValueError("published release is incomplete")
    for name in sorted(missing):
        github.upload(tag, local[name])
    remote = github.assets(release["id"])
    if set(a["name"] for a in remote) != set(local) or len(remote) != len(local):
        raise ValueError("uploaded inventory differs")
    for asset in remote:
        if github.asset_hash(asset) != sha256(local[asset["name"]]):
            raise ValueError("uploaded asset readback differs")
    if release["draft"]:
        github.api(f"releases/{release['id']}", "PATCH", {"draft": False, "make_latest": "false"})
    final = github.api(f"releases/{release['id']}")
    final_ref = github.api("git/ref/tags/" + tag)
    if not final.get("immutable") or final.get("draft") or final_ref["object"]["sha"] != manifest["source_commit"]:
        raise ValueError("published release identity or immutability check failed")
    for name, path in sorted(local.items()):
        if github.anonymous_hash(manifest["feed_url"] + "/" + name) != sha256(path):
            raise ValueError("anonymous asset hash mismatch")


def resume(directory, tag, commit, artifact_id, source_run_id, key, github):
    unsigned = validate_candidate(directory, tag, commit, signed=False)
    if unsigned["run_id"] != source_run_id:
        raise ValueError("release recovery run identity differs from candidate")
    if "hardware_tested" in unsigned:
        if unsigned["hardware_tested"] is not False:
            raise ValueError("release recovery contains an invalid hardware assertion")
        del unsigned["hardware_tested"]
        write_json(Path(directory) / "release.json", unsigned)
        validate_candidate(directory, tag, commit, signed=False)
    sign(directory, tag, commit, key, expected_run_id=source_run_id)
    main_ref = github.api("git/ref/heads/main")
    if (main_ref["object"]["type"] != "commit" or
            main_ref["object"]["sha"] != commit):
        raise ValueError("release recovery source is not the current main commit")
    artifact = github.api(f"actions/artifacts/{artifact_id}")
    if artifact["name"] != tag or artifact["workflow_run"]["id"] != source_run_id:
        raise ValueError("release recovery artifact identity differs from candidate")
    manifest = validate_candidate(directory, tag, commit, signed=True)
    verify_signatures(directory)
    publish(directory, manifest, github, prerelease=False)
    retire_transfer_artifact(github, artifact_id, manifest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("sign", "publish", "resume"))
    parser.add_argument("directory", type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--artifact-id", type=int)
    parser.add_argument("--source-run-id", type=int)
    args = parser.parse_args()
    if os.environ.get("GITHUB_REPOSITORY") != REPOSITORY:
        parser.error("release jobs require the canonical repository")
    event = os.environ.get("GITHUB_EVENT_NAME")
    if event not in ("push", "workflow_dispatch"):
        parser.error("release event is not allowed")
    if os.environ.get("GITHUB_REF") not in ("refs/heads/main", "refs/heads/openwrt-autobuild"):
        parser.error("release branch is not allowed")
    if args.command != "resume" and (event != "push" or args.commit != os.environ.get("GITHUB_SHA")):
        parser.error("release source differs from this workflow")
    if args.command == "publish" and os.environ.get("GITHUB_REF") != "refs/heads/main":
        parser.error("publication is restricted to main")
    if args.command == "resume" and (event != "workflow_dispatch" or
            os.environ.get("GITHUB_REF") != "refs/heads/openwrt-autobuild"):
        parser.error("release recovery requires an explicit test-branch dispatch")
    if args.command in ("sign", "resume"):
        from common import public_key
        key = public_key(os.environ["HH71VM_FEED_PUBLIC_KEY"].encode())[1]
    if args.command == "sign":
        sign(args.directory, args.tag, args.commit, key)
    elif args.command == "publish":
        if not args.artifact_id or args.artifact_id < 1:
            parser.error("publication requires the exact transfer artifact ID")
        manifest = validate_candidate(args.directory, args.tag, args.commit, signed=True)
        if str(manifest["run_id"]) != os.environ.get("GITHUB_RUN_ID"):
            parser.error("candidate belongs to a different workflow run")
        verify_signatures(args.directory)
        github = GitHub()
        publish(args.directory, manifest, github, prerelease=os.environ["GITHUB_REF"] != "refs/heads/main")
        retire_transfer_artifact(github, args.artifact_id, manifest)
    else:
        if not args.artifact_id or args.artifact_id < 1 or not args.source_run_id or args.source_run_id < 1:
            parser.error("release recovery requires exact artifact and source run IDs")
        resume(args.directory, args.tag, args.commit, args.artifact_id, args.source_run_id, key, GitHub())


if __name__ == "__main__":
    main()
