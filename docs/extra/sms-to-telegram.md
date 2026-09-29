# SMS to Telegram

`sms-to-telegram` and `luci-app-sms-to-telegram` are optional packages. They are not
installed in the base firmware. Install them from the immutable signed feed that belongs
to the exact firmware build; do not mix packages from another Release.

```sh
opkg update
opkg install luci-app-sms-to-telegram
```

Log out of LuCI and sign in again, then open **Modem > SMS to Telegram**.

## Telegram setup

The LuCI page presents the initial setup as seven short steps:

1. Create a bot with [BotFather](https://t.me/BotFather), copy its token, and paste it into
   **Telegram Bot Token**.
2. If Telegram is blocked on the router's connection, select an optional HTTP or SOCKS5
   proxy and enter its address. Authentication is optional.
3. Open a private chat with the new bot and send it a fresh message. Telegram bots cannot
   start a private conversation.
4. Select **Detect Chat IDs**. Detection may use the token currently entered in the form
   without saving it first. If the field is blank, it uses the saved token.
5. Select one detected private recipient, or enter its positive numeric ID in
   **Destination Chat ID**. A normal `@username`, link, zero or negative ID is not accepted.
6. Choose whether confirmed messages should be removed from the SIM.
7. Select the standard **Save & Apply** action.

## Optional Telegram proxy

The proxy setting is deliberately local to this package. It is used only by the private
transport helper when it opens `api.telegram.org` for chat detection or SMS delivery. It
does not set system proxy variables, add firewall rules, change routes, or proxy LuCI,
clients, modem traffic, updates, or any other service.

HTTP proxies use an HTTPS `CONNECT` tunnel. SOCKS5 uses remote hostname resolution so a
local DNS block of Telegram does not prevent the request. Both modes continue to verify
Telegram's TLS certificate normally after the tunnel is established. Proxy username and
password fields are optional. The saved password is never returned to LuCI; leaving its
field blank preserves it, while the separate clear option removes it.

Detection reads a bounded set of currently available Bot API updates without acknowledging
them. It is not a permanent address book: old updates may already have been consumed. The
page shows up to 20 unique private-chat candidates and excludes groups and channels. Each
candidate contains only its numeric `chat_id` and any username or first/last name actually
returned by Telegram. Duplicate updates are merged, message text is discarded, and malformed
private-chat data is rejected instead of being guessed.

The token field is intentionally blank whenever the page is opened. Leaving it blank while
saving keeps the existing token. Selecting a detected candidate updates only the editable
form field; UCI is not changed until **Save & Apply**. The status interface reports only
whether configuration is present; it does not return the token, recipient, or SMS text.

## Delivery model

The worker polls the cached `sms_snapshot` from `hh71vm-modemd`. New-message notifications
and the modem daemon's serialized SMS path update that cache; the package never opens a second
AT, Telnet, QMI, or Qualcomm control connection.

Each assembled SMS is identified from its complete modem slot list, sender, timestamp and
decoded text. Persistent state under `/etc/sms-to-telegram/` records these stages:

- pending Telegram delivery;
- Telegram delivery confirmed;
- confirmed, pending SIM deletion;
- completed.

A long message whose later parts have not arrived yet is held for up to ten minutes, so it is
forwarded once and whole. If a part never arrives, the message is forwarded as it is, marked
`[incomplete: N of M parts arrived]`. The state file is written only when a record or the kind
of error changes, not on every poll, and records of messages that have left the modem are
dropped after 30 days.

The HTTPS client verifies the normal CA chain and never enables an insecure certificate
bypass. That makes it sensitive to the router's clock: this board has no battery-backed
clock, so it boots years in the past, and until `sysntpd` has corrected it Telegram's
certificate is "not valid yet" and every send fails. The page names the clock when a
connection cannot be made, so this is not mistaken for a bad bot token.

## Message format

The message sent to Telegram is built from a template you can edit on the LuCI page, under
**Step 6 — Message format**. The default is:

```
<b>SMS from %sender%</b>
%incomplete%
%receive_time%

%sms_text%
```

Each `%name%` is replaced by its value. Anything else is left exactly as typed, so an unknown
name or a bare `%` is never mangled. A line that held only placeholders and comes out empty is
dropped, which is why `%incomplete%` leaves no blank gap for an ordinary message.

| Placeholder | Value |
|---|---|
| `%sender%` | Sender number, or the name a service used. |
| `%receiver%` | This router's own SIM number, when the network reports one. |
| `%sms_text%` | The message text, with the parts of a long SMS already joined. |
| `%receive_time%` | When the message was sent, as the message itself records it. |
| `%router_time%` | The router's own clock when it forwarded the message. |
| `%parts%` | How many parts a long SMS arrived in; `1` for an ordinary message. |
| `%incomplete%` | A note naming the missing parts. Empty when the message is whole. |
| `%storage%` | Which SIM store the message came from: `ME` or `SM`. |
| `%hostname%` | This router's hostname. |

`parse_mode` is `HTML` by default, and can be set to `MarkdownV2` or to plain text. **The
template's own markup is sent as written; the values put into it are escaped for the chosen
mode.** An SMS containing `<b>` or `*` is therefore shown literally and cannot forge
formatting or break the message.

If Telegram rejects the formatting, which it does for the whole request when the markup does
not parse, the message is delivered once as plain text instead and the page reports it. A
mistake in a template cannot stop messages arriving.

HTTP 200 alone is not success: the response must be valid JSON with `ok: true`. Timeouts,
transport failures, Bot API errors and rate limits leave the SIM message intact and schedule
a bounded retry. Telegram provides no idempotency key for `sendMessage`, so an ambiguous
timeout may result in a duplicate after retry. This is at-least-once delivery, not absolute
exactly-once delivery.

When SIM removal is disabled, a confirmed message remains on the SIM but its completed state
prevents repeated forwarding. When removal is enabled, deletion starts only after confirmed
Telegram delivery and uses every slot in the assembled message's `indexes` array. A deletion
failure remains pending deletion and retries only the delete operation. A fresh SMS list is
read back before deletion is marked complete.

## Privacy and diagnostics

The UCI configuration file and persistent state directory use restricted permissions. The
HTTPS helper reads the token, request body and optional proxy credentials from a temporary
mode-0600 file, unlinks it immediately after opening, and does not place those values in
process arguments. Temporary request and fingerprint files are removed on both normal and
failed paths.

LuCI status may show configured/running state, pending counts, and safe last-success/error
times. It never shows the token, recipient, sender, or SMS body. Service logs use fixed error
classes only.

For Telegram's current rules, see the official [Bot introduction](https://core.telegram.org/bots),
[Bot API](https://core.telegram.org/bots/api), and [Bot FAQ](https://core.telegram.org/bots/faq).
