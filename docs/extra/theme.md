# The HH71VM theme

`luci-theme-hh71vm` ships in the base firmware and is the default look on a fresh
installation. It is a theme written for this port, not a restyle of an existing one.

## What it changes

- **Light and dark.** The switch is the button in the top bar. The choice is remembered in
  the browser, and a browser that has no choice stored follows the system setting. The theme
  is picked before the stylesheet paints, so the page does not flash the wrong one on load.
- **A sidebar menu.** LuCI's own menu script only knows how to build a horizontal bar, so the
  theme renders the navigation itself.
- **A modem status strip** in the header, and in the drawer on a narrow screen: signal,
  operator and connection state on every page. Without the modem daemon it simply stays
  hidden.
- **Copy buttons** on the values worth copying — the IMEI, the release identity, and similar.
- **A phone layout.** Wide tables get their own scroll box instead of forcing the page
  sideways, and the active tab of a strip is scrolled into view.

## Choosing a different theme

`luci-theme-bootstrap` stays installed, so the stock look is always available. Switch under
**System > System > Language and Style**.

The HH71VM theme is made the default exactly once, on the first boot of a fresh installation.
After that your choice is yours: a firmware update will not throw you back onto it, because
the marker that records "the default has been applied" is part of the preserved configuration.

## If the page loses its styling

A theme is a stylesheet plus the templates that go with it, and the two have to match. If a
page renders as unstyled text after an update, force a reload that bypasses the browser cache
(Ctrl+F5 in most browsers). The theme adds a version to its stylesheet URL for this reason,
so this should not happen; it is worth reporting if it does.
