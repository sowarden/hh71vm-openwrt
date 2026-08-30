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

The LuCI page presents the initial setup as six short steps:

1. Create a bot with [BotFather](https://t.me/BotFather), copy its token, and paste it into
   **Telegram Bot Token**.
2. Open a private chat with the new bot and send it a fresh message. Telegram bots cannot
   start a private conversation.
3. Select **Detect Chat IDs**. Detection may use the token currently entered in the form
   without saving it first. If the field is blank, it uses the saved token.
4. Select one detected private recipient, or enter its positive numeric ID in
   **Destination Chat ID**. A normal `@username`, link, zero or negative ID is not accepted.
5. Choose whether confirmed messages should be removed from the SIM.
6. Select the standard **Save & Apply** action.

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

The message sent to Telegram contains the sender, a timestamp when the modem supplied one,
and the SMS text. No `parse_mode` is used, so SMS contents are not interpreted as Markdown
or HTML. The HTTPS client verifies the normal CA chain and never enables an insecure
certificate bypass.

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
HTTPS helper reads the token and request body from a temporary mode-0600 file, unlinks it
immediately after opening, and does not place either value in process arguments. Temporary
request and fingerprint files are removed on both normal and failed paths.

LuCI status may show configured/running state, pending counts, and safe last-success/error
times. It never shows the token, recipient, sender, or SMS body. Service logs use fixed error
classes only.

For Telegram's current rules, see the official [Bot introduction](https://core.telegram.org/bots),
[Bot API](https://core.telegram.org/bots/api), and [Bot FAQ](https://core.telegram.org/bots/faq).
