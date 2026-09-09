#!/bin/sh
# Appends one CSV line describing Xray's resource use, meant to run ON the router
# itself - by cron every few minutes during a multi-hour soak test, or by hand.
#
# Nothing here needs anything beyond what a stock OpenWrt 19.07 image ships: no `ss`,
# no `curl`. TCP state counts come from parsing /proc/net/tcp{,6} the same way the
# rest of this package already does; the optional goroutine count uses `wget` against
# the loopback pprof endpoint (see the metrics_listen setting) only when one is
# configured and reachable, and is left blank otherwise.
#
# Usage: xray-soak-sample.sh [csv-file]
#   default csv-file: /tmp/xray-soak.csv

CSV="${1:-/tmp/xray-soak.csv}"
PIDFILE=/var/run/xray.pid

now=$(date -u +%Y-%m-%dT%H:%M:%SZ)

pid=""
if [ -f "$PIDFILE" ]; then
	pid=$(cat "$PIDFILE" 2>/dev/null)
	[ -d "/proc/$pid" ] || pid=""
fi

vmrss_kb=""
vmsize_kb=""
fds=""
if [ -n "$pid" ]; then
	vmrss_kb=$(awk '/^VmRSS:/ {print $2}' "/proc/$pid/status" 2>/dev/null)
	vmsize_kb=$(awk '/^VmSize:/ {print $2}' "/proc/$pid/status" 2>/dev/null)
	fds=$(ls "/proc/$pid/fd" 2>/dev/null | wc -l)
fi

memfree_kb=$(awk '/^MemFree:/ {print $2}' /proc/meminfo 2>/dev/null)
memavail_kb=$(awk '/^MemAvailable:/ {print $2}' /proc/meminfo 2>/dev/null)

# TCP connection-state census across both address families. State codes are the same
# hex values xray-lib.lua and hh71vm-xray-fw already key off of (01 = ESTABLISHED and
# so on, per the kernel's net/tcp_states.h); counted here, not looked up by name, so
# this has no dependency on anything but /proc.
#
# fin_wait reports FIN_WAIT2 (05), not FIN_WAIT1 (04): FIN_WAIT2 is the state
# XTLS/Xray-core#6684 gets stuck in when a peer goes silent instead of closing, so
# it is the one worth graphing. FIN_WAIT1 is a normal, brief transit state.
tcp_counts() {
	awk '
		NR > 1 {
			split($4, a, ":")
			c[a[2]]++
		}
		END {
			printf "%d %d %d %d %d %d\n",
				c["01"] + 0, c["06"] + 0, c["08"] + 0, c["02"] + 0, c["05"] + 0, c["09"] + 0
		}
	' "$1" 2>/dev/null
}

t4=$(tcp_counts /proc/net/tcp)
t6=$(tcp_counts /proc/net/tcp6)
set -- $t4 0 0 0 0 0 0
e4=$1; tw4=$2; cw4=$3; ss4=$4; fw4=$5; la4=$6
set -- $t6 0 0 0 0 0 0
e6=$1; tw6=$2; cw6=$3; ss6=$4; fw6=$5; la6=$6

established=$((e4 + e6))
time_wait=$((tw4 + tw6))
close_wait=$((cw4 + cw6))
syn_sent=$((ss4 + ss6))
fin_wait=$((fw4 + fw6))
last_ack=$((la4 + la6))

# The goroutine count is a much more direct look at whether #6684 (a leaked peer per
# goroutine) is still happening than RSS alone is, but it depends on metrics_listen
# being set - it is off by default because the endpoint is unauthenticated - and it
# should never block a sample if the endpoint is not there or Xray is between starts.
goroutines=""
listen=$(uci -q get xray.main.metrics_listen 2>/dev/null)
if [ -n "$listen" ] && command -v wget >/dev/null 2>&1; then
	prof=$(wget -q -T 2 -O - "http://${listen}/debug/pprof/goroutine?debug=1" 2>/dev/null)
	goroutines=$(printf '%s\n' "$prof" | awk '/^goroutine profile: total/ {print $4}')
fi

if [ ! -s "$CSV" ]; then
	printf 'timestamp,pid,vmrss_kb,vmsize_kb,fds,memfree_kb,memavailable_kb,tcp_established,tcp_time_wait,tcp_close_wait,tcp_syn_sent,tcp_fin_wait,tcp_last_ack,goroutines\n' > "$CSV"
fi

printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
	"$now" "$pid" "$vmrss_kb" "$vmsize_kb" "$fds" "$memfree_kb" "$memavail_kb" \
	"$established" "$time_wait" "$close_wait" "$syn_sent" "$fin_wait" "$last_ack" \
	"$goroutines" >> "$CSV"
