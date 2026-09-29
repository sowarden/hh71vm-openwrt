"""No modem, network or profile text may reach innerHTML in the LuCI pages.

LuCI's E(tag, attrs, child) hands `child` to dom.append(), which writes a lone string
with `node.innerHTML` and turns only array members into text nodes.  SMS bodies and
senders, operator names from the radio network, AT answers and the names of imported
share links are all somebody else's text, so every place that renders them must pass
an array (or go through one of the text helpers).  This is a static check over the
page sources, since the build never executes them.
"""

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
FEED = ROOT / "openwrt-feed"
SOURCES = sorted(
    list((FEED / "package/luci/applications").rglob("*.js"))
    + [FEED / "target/linux/rtkmipsel/base-files/www/luci-static/resources/hh71vm/updater.js"]
)

CALL = re.compile(r"\bE\(\s*'[a-z0-9]+'\s*,\s*(\{[^{}]*\}|null)\s*,\s*")
CONTENT = re.compile(r"\bdom\.content\(\s*[A-Za-z_][\w.]*\s*,\s*")

# Expressions that are already text nodes, nodes or arrays.
SAFE_PREFIXES = (
    "[", "E(", "_(", "'", '"', "document.createTextNode(",
    "m.text(", "x.text(", "txt(", "asText(", "text(",
    # page helpers that build nodes or arrays themselves
    "m.copyable(", "m.action(", "x.action(", "m.label(", "x.label(", "m.state(",
    "x.state(", "m.facts(", "x.facts(", "exampleLines(", "button(",
)

# Fields that carry text from outside the router's own code.
RISKY_FIELD = re.compile(
    r"\.(text|sender|name|note|operator|long|short|error|message|detail|log|stderr|"
    r"output|address|apn|token|step|title|hint|state|reason|ifaces|uplink|warning|"
    r"backup_error|desired_error|current_imei|numeric|act_name|pdp_type|auth_name)\b"
)
# Local variables that hold such text in the page helpers.
RISKY_NAME = re.compile(
    r"^(value|label|descr|description|caption|sub|title|text|message|okMsg|"
    r"warnText|lvl|uri|l|t|d)$"
)
STRING_LITERAL = re.compile(r"'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"")


def argument(source, start):
    """The expression starting at `start`, up to the comma or parenthesis closing it."""
    depth, out = 0, []
    for ch in source[start:]:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            if depth == 0:
                break
            depth -= 1
        elif ch == "," and depth == 0:
            break
        out.append(ch)
    return "".join(out).strip()


def branches(expr):
    """The value branches of a top-level `a ? b : c`, or None.  The condition is never
    rendered, so only what the expression can evaluate to matters."""
    depth, quote, marks = 0, None, []
    for i, ch in enumerate(expr):
        if quote:
            if ch == quote and expr[i - 1] != "\\":
                quote = None
            continue
        if ch in "'\"":
            quote = ch
        elif ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif depth == 0 and ch in "?:":
            marks.append((i, ch))
    question = next((i for i, ch in marks if ch == "?"), None)
    if question is None:
        return None
    level, colon = 0, None
    for i, ch in marks:
        if i <= question:
            continue
        if ch == "?":
            level += 1
        elif level:
            level -= 1
        else:
            colon = i
            break
    if colon is None:
        return None
    return [expr[question + 1:colon].strip(), expr[colon + 1:].strip()]


def wholly_safe(expr):
    """True when a safe call or literal is the *whole* expression.

    Merely starting safely is not enough: `_('Network ') + res.error` still reaches
    innerHTML with the operator's text glued on the end.
    """
    bare = STRING_LITERAL.sub(lambda m: "'" + "_" * (len(m.group(0)) - 2) + "'", expr)
    for prefix in SAFE_PREFIXES:
        if not bare.startswith(prefix):
            continue
        if prefix in ("'", '"'):
            match = STRING_LITERAL.match(bare)
            if match and match.end() == len(bare):
                return True
            continue
        depth = 0
        for index, ch in enumerate(bare):
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
                if depth == 0:
                    if index == len(bare) - 1:
                        return True
                    break
    return False


def risky(expr):
    parts = branches(expr)
    if parts:
        return any(risky(part) for part in parts)
    if wholly_safe(expr):
        return False
    bare = STRING_LITERAL.sub("''", expr)
    if RISKY_NAME.match(bare):
        return True
    return RISKY_FIELD.search(bare) is not None


class LuciTextSinks(unittest.TestCase):
    def test_sources_were_found(self):
        names = {path.name for path in SOURCES}
        for expected in ("sms.js", "console.js", "main.js", "updater.js", "70_modem.js"):
            self.assertIn(expected, names)

    def test_untrusted_fields_are_never_a_lone_string_child(self):
        found = []
        for path in SOURCES:
            source = path.read_text(encoding="utf-8")
            for pattern in (CALL, CONTENT):
                for match in pattern.finditer(source):
                    expr = argument(source, match.end())
                    if risky(expr):
                        line = source.count("\n", 0, match.start()) + 1
                        found.append("%s:%d: %s" % (path.relative_to(ROOT), line, expr[:90]))
        self.assertEqual(found, [], "\n".join(found))

    def test_the_checker_itself_catches_the_original_defect(self):
        self.assertTrue(risky("msg.text || ''"))
        self.assertTrue(risky("n.long || n.short || '?'"))
        self.assertTrue(risky("l"))
        self.assertTrue(risky("String(e.message || e)"))
        self.assertFalse(risky("[ String(e.message || e) ]"))
        self.assertFalse(risky("m.text(msg.sender || '?')"))
        self.assertFalse(risky("_('Messages')"))
        self.assertFalse(risky("e.apn ? m.copyable(e.apn) : E('em', {}, _('x'))"))
        self.assertTrue(risky("ok ? _('fine') : res.error"))
        # a safe call at the start is not enough when text is concatenated onto it
        self.assertTrue(risky("_('Network ') + res.error"))
        self.assertFalse(risky("_('Network ') + _('unknown')"))


if __name__ == "__main__":
    unittest.main()
