# Put the /mnt/extern share's binaries on PATH for interactive logins.
#
# Packages installed with `hh71vm-extern-pkg install` land under a prefix on the share
# rather than under /, so nothing finds them by name without this.  Both entries go
# last: a program on the share must never shadow one from the image.
#
# The paths are added only while the share is actually mounted.  A stale entry would
# cost a lookup on every command and, worse, would suggest the share is available when
# it is not.

[ -d /mnt/extern/opkg/usr/bin ] && {
	export PATH="$PATH:/mnt/extern/opkg/usr/sbin:/mnt/extern/opkg/usr/bin"
	export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}/mnt/extern/opkg/usr/lib:/mnt/extern/opkg/lib"
}
