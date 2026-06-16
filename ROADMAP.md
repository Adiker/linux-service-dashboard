# linux-service-dashboard — roadmap

The project is a functional Qt6 MVP. The next work should make it safer, easier to package, and easier to validate.

---

## Priority 1 — Stability and Safety

### Provider tests

Add focused tests for parsers and provider error handling:

- systemd unit parsing
- Docker row parsing
- `nmcli` VPN parsing
- `sensors -j` and text fallback parsing
- `lsblk -J` and SMART JSON parsing

### Safer command reporting

Improve status/error surfaces so permission errors, missing tools, and empty command output are visually distinct.

### Module toggles

The settings page persists module toggles, but pages are not hidden/disabled yet. Wire these toggles into the sidebar and refresh scheduler.

---

## Priority 2 — Desktop Integration

### Packaging

Add distro packaging:

- Arch PKGBUILD
- CPack `.deb` / `.rpm`
- optional AppImage

### CI

Add GitHub Actions for:

- Debug and Release build
- changed-file `clang-format`
- optional warning-only `clang-tidy`

### Wayland/X11 smoke checks

Document manual verification on KDE Plasma Wayland and X11/XWayland. If practical, add a small scripted startup smoke test using Qt's `offscreen` and `minimal` QPA plugins.

---

## Priority 3 — Feature Growth

### Direct DBus providers

Replace shell parsing with DBus where it materially improves reliability:

- systemd DBus
- NetworkManager DBus

### Mount profiles

Add saved mount profiles and fstab-aware display.

### Persistent table layouts

Persist sort order, column widths, and visible columns per page.

### SMART history

Add opt-in SMART history with conservative polling and clear permission handling.
