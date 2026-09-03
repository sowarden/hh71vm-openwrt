# Modem control

`luci-app-hh71vm-modem` and the `hh71vm-modemd` daemon behind it are part of the base
firmware. Nothing has to be installed and nothing has to be enabled.

## What it does

The HH71VM is two separate Linux systems in one case: the Realtek half runs OpenWrt, and the
Qualcomm half runs the modem with its own firmware. They are joined over USB/RNDIS. Every
modem operation therefore has to cross that link, and `hh71vm-modemd` is the only thing on
this side that does.

The daemon keeps a parsed snapshot of the modem in memory and in a JSON file and refreshes it
in the background, so the web pages open immediately instead of waiting on the radio. LuCI
never talks to the modem directly: the pages call ubus, `rpcd` hands the call to the daemon,
and the daemon serialises it against everything else on the single control channel.

## The pages

Open **Modem** in the LuCI menu.

| Page | What is on it |
|---|---|
| Overview | Signal, operator, registration, connection state, data counters |
| Messages | Read, delete and send SMS, including multipart messages |
| Network | Operator selection, 2G/3G/4G mode selection, USSD requests |
| Profiles | APN profiles: create, edit, choose the active one |
| SIM and PIN | SIM state, PIN entry, enable/disable and change PIN |
| Phonebook | Read, add and delete SIM phonebook entries |
| AT console | Send raw AT commands and read the replies |

**System > About this port** carries the licence information, the source link, and the GPL
written offer.

Some operations are slower than a single web request is allowed to be. USSD is the clearest
case: `AT+CUSD` returns `OK` immediately and the network's answer arrives later as an
unsolicited message. Those run as a job the page polls, and the page shows how long the
network has been thinking rather than failing at the request ceiling.

## Command line

The daemon is also usable directly, which is the fastest way to check something without the
browser:

```sh
hh71vm-modemd --at 'AT+CSQ' 'AT+COPS?'
hh71vm-modemd --call sms_list
```

`--foreground -v` runs it in the terminal with verbose logging instead of as a service. The
service itself is the usual `/etc/init.d/hh71vm-modemd`.

The same API is reachable over ubus, which is what a script on the router should use:

```sh
ubus call hh71vm-modem status
ubus call hh71vm-modem sms_list
```

## Notes

- The daemon owns the modem channel. Do not open a second AT, telnet or QMI session to the
  Qualcomm half while it is running; the two will interleave and both will read nonsense.
- A router that boots with no SIM in the slot re-reads the slot by itself, so inserting a
  SIM afterwards does not require a reboot.
- Incoming messages are announced by the modem rather than polled for, which is what keeps
  the unread flag meaningful.
