#!/usr/bin/env python3
"""Apply the two corrections a Go toolchain needs before it can target this board.

Both are mandatory and neither is obvious, so they are applied inside the build rather
than by hand.  Each one asserts its own context and exits non-zero if the upstream text
has moved, because a silently skipped fix produces a binary that either refuses to start
or takes the board down with a kernel panic.

1. `_ENOSYS` is wrong for MIPS.  arch/mips/include/uapi/asm/errno.h defines ENOSYS as
   89; every other architecture uses 38, and runtime/defs_linux_mipsx.go took the
   generic value.  Go 1.26 calls futex_time64 first and falls back when the kernel
   answers -ENOSYS, but on MIPS the comparison never matches, so on any kernel older
   than 5.1 - ours is 4.14 - the fallback never fires and futexwakeup kills the process
   before main() runs.  This is an upstream Go bug affecting every 32-bit MIPS Linux
   older than 5.1, not only this board.

2. The netpoll wakeup must not be an eventfd.  Writing to an eventfd that is registered
   in an epoll set panics this Realtek 4.14 kernel; a pipe and a socketpair in the same
   shape are both fine.  Go used a pipe until 1.21.  See go-netpoll-pipe.py, which does
   that half.

    python3 patch-go-toolchain.py <GOROOT> [--netpoll <path to go-netpoll-pipe.py>]
"""
import argparse
import io
import pathlib
import subprocess
import sys

ENOSYS_FILE = "src/runtime/defs_linux_mipsx.go"
ENOSYS_OLD = "_ENOSYS = 0x26"
ENOSYS_NEW = "_ENOSYS = 0x59"


def fix_enosys(goroot):
    path = goroot / ENOSYS_FILE
    text = io.open(path, encoding="utf-8", newline="").read()
    if ENOSYS_NEW in text:
        print("_ENOSYS: already 0x59")
        return
    if text.count(ENOSYS_OLD) != 1:
        raise SystemExit(f"_ENOSYS: expected exactly one '{ENOSYS_OLD}' in {path}")
    io.open(path, "w", encoding="utf-8", newline="").write(text.replace(ENOSYS_OLD, ENOSYS_NEW))
    print(f"_ENOSYS: 0x26 -> 0x59 in {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("goroot", type=pathlib.Path)
    parser.add_argument("--netpoll", type=pathlib.Path,
                        default=pathlib.Path(__file__).with_name("go-netpoll-pipe.py"))
    args = parser.parse_args()

    goroot = args.goroot.resolve()
    if not (goroot / "src/runtime").is_dir():
        raise SystemExit(f"not a Go source tree: {goroot}")

    fix_enosys(goroot)
    result = subprocess.run([sys.executable, str(args.netpoll), str(goroot)])
    if result.returncode:
        raise SystemExit("netpoll patch failed")

    # Prove both landed rather than trusting the two steps above: a toolchain that is
    # only half patched is the worst outcome, and it looks exactly like a good one
    # until the board panics.
    enosys = io.open(goroot / ENOSYS_FILE, encoding="utf-8").read()
    netpoll = io.open(goroot / "src/runtime/netpoll_epoll.go", encoding="utf-8").read()
    if ENOSYS_NEW not in enosys:
        raise SystemExit("verification failed: _ENOSYS was not corrected")
    if "netpollEventFd" in netpoll or "netpollBreakRd" not in netpoll:
        raise SystemExit("verification failed: netpoll still uses an eventfd")
    print("go toolchain patched and verified")


if __name__ == "__main__":
    main()
