# linux-service-dashboard — roadmap

Most MVP hardening milestones have been completed on the integration branch. Remaining follow-up work focuses on packaging polish, deeper integration testing, and optional desktop enhancements.

---

## Completed Milestones

- Provider parser, device rule, settings store, and `AlertMonitor` unit tests (`provider-parser-tests`)
- Shared `ServiceRegistry` (eliminating duplicate provider execution) and deduplicated `ProviderParsers` / `SmartDeviceRules`
- Asynchronous DBus queries for systemd and NetworkManager (with `systemctl` / `nmcli` fallbacks)
- Safer command error classification (`CommandErrorKind`), `StatusBanner`, and `DiagnosticsDialog`
- Module toggles wired into sidebar, Overview cards, and refresh scheduling
- Persistent table layouts, sorting (`TableFilterProxyModel`), search/state filters, and context menus across all tables
- Mount profiles, `/etc/fstab` display, mount/unmount actions, and profile removal
- SMART history with opt-in scheduled refresh, CSV export, and NVMe health log support
- Edge-triggered tray notifications, dynamic tray tooltip, and window geometry/tray lifecycle settings
- Centralized `ThemeManager` (`System`, `Light`, `Dark`, `OLED`), `StatusBadgeDelegate`, clickable `OverviewCard`, and scrollable `SettingsPage`
- Distro packaging recipes (`packaging/PKGBUILD`, CPack `.deb`/`.rpm`) and CI workflow

---

## Next Potential Improvements

- Automated Qt widget/integration tests covering page interactions and proxy filtering
- Optional AppImage packaging recipe
- `clang-tidy` CI check with warning tracking
- Screenshots in `README.md` across Light, Dark, and OLED themes
