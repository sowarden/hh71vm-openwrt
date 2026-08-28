#!/usr/bin/env python3
# Copyright 2026 sowarden
# SPDX-License-Identifier: Apache-2.0
"""Promote exact Actions artifacts to the managed extra-tools rolling release."""

import argparse
import html
import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from extras_common import ROOT, WORKFLOW, compatibility, release_policy, validate_candidate
from package_metadata import sha256

TAG = "extra-tools"
MARKER = "<!-- hh71vm-extra-tools:v1 -->"
WARNING = ("These bundles are built automatically and published to Releases without manual "
           "testing on real hardware. They may contain errors. Use at your own risk.")


def gh(*args, payload=None, raw=False):
    result = subprocess.run(["gh", *map(str, args)],
                            input=json.dumps(payload) if payload is not None else None,
                            capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr.strip())
    return result.stdout if raw else json.loads(result.stdout or "null")


def api(repo, endpoint, method="GET", payload=None, missing=False):
    args = ["api", f"repos/{repo}/{endpoint}", "--method", method]
    if payload is not None:
        args += ["--input", "-"]
    try:
        return gh(*args, payload=payload)
    except RuntimeError as error:
        if missing and "(HTTP 404)" in str(error):
            return None
        raise


def positive(value):
    if not re.fullmatch(r"[1-9][0-9]*", str(value)):
        raise ValueError("run ID and attempt must be positive integers")
    return int(value)


def check_run(repo, run_id, attempt):
    run = api(repo, f"actions/runs/{positive(run_id)}/attempts/{positive(attempt)}")
    workflow = api(repo, "actions/workflows/build-extras.yml")
    if (run["workflow_id"] != workflow["id"] or run["path"] != WORKFLOW or
            run["head_repository"]["full_name"] != repo or run["head_branch"] != "main" or
            run["event"] not in ("push", "workflow_dispatch")):
        raise ValueError("candidate must come from this repository's Build extras workflow on main")
    pages = gh("api", f"repos/{repo}/actions/runs/{run_id}/attempts/{attempt}/jobs?per_page=100",
               "--paginate", "--slurp")
    jobs = [job for page in pages for job in page["jobs"] if job["name"] == "build"]
    run["build_succeeded"] = len(jobs) == 1 and jobs[0]["conclusion"] == "success"
    return run


def fetch(repo, run_id, attempt, output):
    run = check_run(repo, run_id, attempt)
    if not run["build_succeeded"]:
        raise ValueError("only a successful build job can be promoted")
    if output.exists() and any(output.iterdir()):
        raise ValueError("download directory must be empty")
    output.mkdir(parents=True, exist_ok=True)
    gh("run", "download", str(run_id), "--repo", repo, "--name",
       f"extra-tools-{run_id}-{attempt}", "--dir", output, raw=True)


def authorize(candidate, run, mode, tested, notes, enabled):
    if (run["head_sha"] != candidate["source_commit"] or
            run["run_attempt"] != candidate["run_attempt"] or
            run["run_number"] != candidate["run_number"] or run["id"] != candidate["run_id"]):
        raise ValueError("candidate provenance does not match the Actions run")
    if not run.get("build_succeeded"):
        raise ValueError("publication requires a completed successful build job")
    if mode == "tested":
        if not tested or not notes.strip():
            raise ValueError("explicit hardware confirmation and a short test report are required")
    elif mode == "unverified":
        if not enabled:
            raise ValueError("automatic unverified releases are disabled")
        # The publishing job is part of the still-running build workflow.
        if (str(candidate["run_id"]) != os.environ.get("GITHUB_RUN_ID") or
                str(candidate["run_attempt"]) != os.environ.get("GITHUB_RUN_ATTEMPT") or
                candidate["source_commit"] != os.environ.get("GITHUB_SHA")):
            raise ValueError("automatic publication must run inside its own build workflow")
    else:
        raise ValueError("unknown publication mode")


def release_body(repo, candidate, tested, actor, notes):
    firmware = candidate["firmware"]
    status = ("Hardware test confirmed by " + html.escape(actor) + ".\n\n" +
              html.escape(notes.strip()).replace("`", "\\`") if tested else "**Warning: " + WARNING + "**")
    lines = [MARKER, "# HH71VM extra tools", "", "**Optional add-ons, NOT firmware.**", "",
             f"To install the firmware, [open the repository](https://github.com/{repo}), "
             "read the installation instructions and clone the entire repository.", "", status, "",
             f"Compatible firmware build: **{firmware['firmware_build']}**", "",
             f"Exact kernel package / ABI: `{firmware['kernel']}`", "",
             f"Architecture: `{firmware['architecture']}`. Never force kernel dependencies.", "",
             f"Sources: `{candidate['source_commit']}`. "
             f"[Build run](https://github.com/{repo}/actions/runs/{candidate['run_id']}) "
             f"(attempt {candidate['run_attempt']}).", "",
             "| Bundle | Version | Download |", "|---|---|---|"]
    for name, item in sorted(candidate["bundles"].items()):
        lines.append(f"| {name} | {item['version']} | [{item['archive']}]"
                     f"(https://github.com/{repo}/releases/download/{TAG}/{item['archive']}) |")
    lines += ["", "Firmware image SHA-256 (compatibility references, not release assets):", ""]
    lines.extend(f"- `{name}`: `{digest}`" for name, digest in sorted(firmware["images"].items()))
    lines += ["", "Use the ZIP links above, not GitHub's automatically generated Source code archives.",
              "The installer checks the board identity, checksums and exact kernel package version.",
              "Hardware confirmation covers the reported checks, not every operator or configuration.",
              "", f"<!-- extras-build:{candidate['run_number']}:{candidate['run_attempt']} "
              f"candidate:{sha256_current(candidate)} -->", ""]
    return "\n".join(lines)


def sha256_current(candidate):
    # Canonical serialization is also used by the build step.
    import hashlib
    return hashlib.sha256((json.dumps(candidate, indent=2, sort_keys=True) + "\n").encode()).hexdigest()


def check_existing(release, candidate, tested):
    if release is None:
        return
    if release.get("immutable") or MARKER not in (release.get("body") or ""):
        raise ValueError("extra-tools is immutable or is not managed by this publisher")
    previous = re.search(r"<!-- extras-build:(\d+):(\d+) candidate:([a-f0-9]{64}) -->", release["body"])
    if previous:
        order = (int(previous[1]), int(previous[2]))
        incoming = (candidate["run_number"], candidate["run_attempt"])
        if incoming < order:
            raise ValueError("refusing to replace a newer release with an older candidate")
        if incoming == order and previous[3] != sha256_current(candidate):
            raise ValueError("same build identity has different candidate bytes")
        if incoming == order and not tested and "Hardware test confirmed by " in release["body"]:
            raise ValueError("refusing to downgrade an already confirmed hardware test")


def publish(repo, directory, candidate, tested, notes):
    release = api(repo, f"releases/tags/{TAG}", missing=True)
    # GET by tag can omit drafts. Discover an interrupted first publication too.
    if release is None:
        drafts = gh("api", f"repos/{repo}/releases?per_page=100", "--paginate", "--slurp")
        matches = [item for page in drafts for item in page if item["tag_name"] == TAG]
        if len(matches) > 1:
            raise ValueError("multiple extra-tools releases found")
        release = matches[0] if matches else None
    check_existing(release, candidate, tested)
    if release is None:
        existing_tag = api(repo, f"git/ref/tags/{TAG}", missing=True)
        if existing_tag:
            raise ValueError("extra-tools tag already exists without a managed release")
        release = api(repo, "releases", "POST", {
            "tag_name": TAG, "target_commitish": candidate["source_commit"],
            "name": "HH71VM extra tools", "body": MARKER + "\nPublication in progress.",
            "draft": True, "make_latest": "false",
        })
    suffix = f"r{candidate['run_id']}-a{candidate['run_attempt']}"
    with tempfile.TemporaryDirectory(prefix="extras-publish-") as temporary:
        stage = Path(temporary)
        uploads = {name: directory / name for name in candidate["assets"]}
        for local, name in (("candidate.json", f"extra-tools-{suffix}.json"),
                            ("SHA256SUMS", f"extra-tools-{suffix}-SHA256SUMS")):
            target = stage / name
            if local == "SHA256SUMS":
                # The release copy uses the unique manifest asset name.
                target.write_bytes((directory / local).read_text().replace(
                    "  candidate.json\n", f"  extra-tools-{suffix}.json\n").encode("utf-8"))
            else:
                shutil.copyfile(directory / local, target)
            uploads[name] = target
        existing = {asset["name"] for asset in release.get("assets", [])}
        for name, path in uploads.items():
            if name not in existing:
                gh("release", "upload", TAG, path, "--repo", repo, raw=True)
            verification = stage / ("verify-" + name)
            verification.mkdir()
            gh("release", "download", TAG, "--repo", repo, "--pattern", name, "--dir", verification, raw=True)
            if sha256(verification / name) != sha256(path):
                raise ValueError(f"uploaded asset differs: {name}")
        body = release_body(repo, candidate, tested, os.environ.get("GITHUB_ACTOR", "maintainer"), notes)
        ref = api(repo, f"git/ref/tags/{TAG}", missing=True)
        if ref:
            if ref["object"]["type"] != "commit":
                raise ValueError("managed rolling tag must be lightweight")
            api(repo, f"git/refs/tags/{TAG}", "PATCH", {"sha": candidate["source_commit"], "force": True})
        # New draft releases create their tag when published.
        api(repo, f"releases/{release['id']}", "PATCH", {
            "name": f"HH71VM extra tools - {candidate['firmware']['firmware_build']}",
            "body": body, "draft": False, "prerelease": not tested, "make_latest": "false",
            "target_commitish": candidate["source_commit"],
        })
        # Only remove old assets after all new files have been uploaded and verified.
        refreshed = api(repo, f"releases/{release['id']}")
        for asset in refreshed["assets"]:
            if asset["name"] not in uploads:
                api(repo, f"releases/assets/{asset['id']}", "DELETE")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("fetch", "publish"))
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--run-id", type=positive)
    parser.add_argument("--attempt", type=positive, default=1)
    parser.add_argument("--candidate-sha256")
    parser.add_argument("--mode", choices=("tested", "unverified"), default="tested")
    parser.add_argument("--hardware-tested", choices=("true", "false"), default="false")
    parser.add_argument("--test-notes", default="")
    args = parser.parse_args()
    repo = os.environ["GITHUB_REPOSITORY"]
    if args.command == "fetch":
        fetch(repo, args.run_id, args.attempt, args.directory)
        return
    if not args.candidate_sha256:
        parser.error("--candidate-sha256 is required")
    candidate = validate_candidate(args.directory, args.candidate_sha256)
    if candidate["firmware"] != compatibility(ROOT):
        raise ValueError("candidate targets a different firmware compatibility lock")
    if subprocess.run(["git", "merge-base", "--is-ancestor", candidate["source_commit"], "HEAD"], cwd=ROOT).returncode:
        raise ValueError("candidate source is not part of this main branch history")
    run = check_run(repo, candidate["run_id"], candidate["run_attempt"])
    authorize(candidate, run, args.mode, args.hardware_tested == "true", args.test_notes, release_policy(ROOT))
    publish(repo, args.directory, candidate, args.mode == "tested", args.test_notes)


if __name__ == "__main__":
    main()
