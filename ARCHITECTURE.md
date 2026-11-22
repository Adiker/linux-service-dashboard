# ARCHITECTURE.md — linux-service-dashboard

This is the full technical reference for the project: structure, module behavior, runtime flows, settings, platform integration, packaging, and checks.

For mandatory agent workflow rules, branch policy, and sharp implementation guardrails, read `AGENTS.md`. For Claude-specific quick-start context, read `CLAUDE.md`.

---

## Project Context

**linux-service-dashboard** is a local Qt6 Widgets desktop application for monitoring and controlling common Linux services from one place. It focuses on a single-user desktop workflow: systemd units, Docker containers, VPN status, network mounts, sensors, and disk/SMART information.

**Stack:** C++20, Qt6 Widgets
**Build system:** CMake 3.21+
**Platform:** Linux desktop, with KDE Plasma Wayland and X11/XWayland as explicit targets
**Persistence:** `QSettings`

---

## Project Structure

```text
linux-service-dashboard/
├── CMakeLists.txt
├── resources/
│   ├── io.github.Adiker.LinuxServiceDashboard.desktop
│   ├── io.github.Adiker.LinuxServiceDashboard.smart-helper.policy.in
│   ├── icons/
│   │   └── linux-service-dashboard-*.png
│   ├── linux-service-dashboard.svg
│   └── resources.qrc
├── src/
│   ├── main.cpp
│   ├── MainWindow.h/cpp
│   ├── helpers/
│   │   ├── SmartDeviceRules.h
│   │   └── SmartHelper.cpp
│   ├── core/
│   │   ├── AlertMonitor.h/cpp
│   │   ├── CommandResult.h
│   │   ├── CommandRunner.h/cpp
│   │   ├── ModuleSettings.h/cpp
│   │   ├── MountProfileStore.h/cpp
│   │   ├── RefreshScheduler.h/cpp
│   │   ├── ServiceGroupSettings.h/cpp
│   │   ├── ServiceRegistry.h/cpp
│   │   ├── SmartHistoryStore.h/cpp
│   │   └── SmartRefreshScheduler.h/cpp
│   ├── models/
│   │   ├── ServiceModel.h/cpp
│   │   ├── DockerContainerModel.h/cpp
│   │   ├── MountModel.h/cpp
│   │   ├── SensorModel.h/cpp
│   │   ├── DiskModel.h/cpp
│   │   └── TableFilterProxyModel.h/cpp
│   ├── parsers/
│   │   └── ProviderParsers.h/cpp
│   ├── services/
│   │   ├── SystemdServiceProvider.h/cpp
│   │   ├── DockerProvider.h/cpp
│   │   ├── NetworkProvider.h/cpp
│   │   ├── MountProvider.h/cpp
│   │   ├── SensorProvider.h/cpp
│   │   └── SmartProvider.h/cpp
│   ├── ui/
│   │   ├── StatusBadgeDelegate.h/cpp
│   │   └── ThemeManager.h/cpp
│   ├── widgets/
│   │   ├── OverviewPage.h/cpp
│   │   ├── SystemdPage.h/cpp
│   │   ├── DockerPage.h/cpp
│   │   ├── VpnPage.h/cpp
│   │   ├── MountsPage.h/cpp
│   │   ├── SensorsPage.h/cpp
│   │   ├── DisksPage.h/cpp
│   │   ├── SettingsPage.h/cpp
│   │   ├── StatusBanner.h/cpp
│   │   ├── ConfirmActionDialog.h/cpp
│   │   ├── DiagnosticsDialog.h/cpp
│   │   ├── LogViewerDialog.h/cpp
│   │   └── SmartHistoryDialog.h/cpp
│   └── utils/
│       ├── FstabParser.h/cpp
│       ├── JsonUtils.h/cpp
│       ├── StatusUtils.h/cpp
│       ├── TableLayoutPersistence.h/cpp
│       └── TimeUtils.h/cpp
├── AGENTS.md
├── CLAUDE.md
├── GEMINI.md
├── ROADMAP.md
├── README.md
└── TODO.md
```

---

## Runtime Flow

1. `main.cpp` creates `QApplication`, sets organization/application metadata, sets the KDE/Wayland desktop file name, applies Fusion style, and sets the application icon.
2. `MainWindow` creates a single `ServiceRegistry` (shared across `OverviewPage`, feature pages, and `AlertMonitor`), builds the sidebar/stack/status bar UI, restores saved window geometry, and configures the system tray icon/menu.
3. `SettingsPage::settingsChanged` triggers `MainWindow::reloadSettings()`.
4. `RefreshScheduler` emits periodic refresh requests based on `refresh/intervalSeconds`.
5. `MainWindow::refreshAll()` fans out once per enabled module through the shared `ServiceRegistry` providers so Overview and detail pages stay synchronized without duplicate command execution.
6. Providers run asynchronous DBus queries or system commands through `CommandRunner` (with context deduplication and ring-buffered failure logging) and parse output through `ProviderParsers`.
7. `AlertMonitor` observes provider signals and raises edge-triggered tray notifications plus live tray tooltip updates.

---

## Settings Schema

Settings use Qt `QSettings` under organization `LocalTools` and application `Linux Service Dashboard`.

Known keys:

| Key | Type | Default | Purpose |
|---|---:|---:|---|
| `refresh/intervalSeconds` | int | `30` | Periodic refresh interval |
| `systemd/watchedServices` | string list | Docker, NetworkManager, sshd, PostgreSQL, Redis | Units for the active group (legacy/compatibility mirror of the active group's list) |
| `systemd/groups` | string list | `Default` | Names of user-defined service groups |
| `systemd/group/<name>` | string list | Default service set | Watched units for a specific group |
| `systemd/activeGroup` | string | `Default` | Group currently selected on the systemd page |
| `modules/systemd` | bool | `true` | Show the systemd page and include it in refresh scheduling |
| `modules/docker` | bool | `true` | Show the Docker page and include it in refresh scheduling |
| `modules/vpn` | bool | `true` | Show the VPN page and include it in refresh scheduling |
| `modules/mounts` | bool | `true` | Show the Mounts page and include it in refresh scheduling |
| `modules/sensors` | bool | `true` | Show the Sensors page and include it in refresh scheduling |
| `modules/smart` | bool | `true` | Show the Disks/SMART page and include it in refresh scheduling |
| `mounts/profileNames` | string list | – | Names of saved mount profiles |
| `mounts/profiles/<encoded>/{source,target,filesystemType,options}` | string | – | Saved mount profile fields (percent-encoded profile name) |
| `smart/scheduledRefreshEnabled` | bool | `false` | Enable the opt-in scheduled SMART refresh |
| `smart/scheduledRefreshMinutes` | int | `30` | Scheduled SMART refresh interval in minutes (minimum 5) |
| `smart/history/serial/*`, `smart/history/path/*` | string list | – | Per-disk SMART history entries (keyed by serial, else device path) |
| `notifications/enabled` | bool | `true` | Enable edge-triggered desktop tray alerts |
| `window/closeToTray` | bool | `true` | Hide to system tray on window close instead of quitting |
| `window/startMinimizedToTray` | bool | `false` | Start hidden in system tray when tray is available |
| `window/geometry` | byte array | – | Saved main window size and position |
| `theme/preference` | string | `System` | `System`, `Light`, `Dark`, or `OLED` |
| `tables/<page>/headerState` | byte array | – | Saved column sizes, order, and sort state per proxy-backed table (`systemd`, `docker`, `mounts`, `sensors`, `disks`) |

Module toggles drive sidebar visibility and refresh scheduling: `MainWindow::applyModuleVisibility()` hides disabled pages and `refreshAll()` skips their providers, while the Overview page hides the matching cards. The Overview and Settings pages are always shown.

---

## Desktop Integration

The app uses a single icon identity for taskbar, tray, and packaging:

- Desktop file id: `io.github.Adiker.LinuxServiceDashboard`
- Desktop file: `resources/io.github.Adiker.LinuxServiceDashboard.desktop`
- Runtime icon helper: `serviceDashboardIcon()`
- Runtime resources: `:/icons/linux-service-dashboard-*.png`
- Installed hicolor icons: `share/icons/hicolor/*/apps/io.github.Adiker.LinuxServiceDashboard.png`
- Installed scalable source icon: `share/icons/hicolor/scalable/apps/io.github.Adiker.LinuxServiceDashboard.svg`

`QGuiApplication::setDesktopFileName("io.github.Adiker.LinuxServiceDashboard")` is required before showing windows so KDE Plasma Wayland can match the app window to the desktop file when the desktop entry is installed. `serviceDashboardIcon()` loads embedded PNG resources directly, so the tray and window icon do not require any desktop/icon installation.

The app does not force Wayland or X11. Qt chooses the platform plugin from the user's session or `QT_QPA_PLATFORM`.

---

## Theme System

`MainWindow::applyTheme()` delegates stylesheet generation and palette selection to `ThemeManager` (`src/ui/ThemeManager.h/cpp`).

Supported preferences:

- `System`: Light or Dark based on the current palette
- `Light`: explicit light theme with high-contrast semantic status tones
- `Dark`: explicit dark theme
- `OLED`: black-first high-contrast dark theme

Pages use shared object names and delegates instead of local styles:

- `pageTitle` for page headings
- `card`, `cardHeader`, `cardValue`, `cardSubtitle`, `statusDot` for clickable overview cards and VPN panel
- `statusBanner` (`StatusBanner`) for classified info/warning/error feedback
- `StatusBadgeDelegate` for theme-aware status pills across tables

---

## Providers

Providers own command execution and parsing, and are owned once by `ServiceRegistry`:

- `SystemdServiceProvider`: asynchronous systemd1 DBus (`ListUnits`/`ListUnitsFiltered`, bounded timeout) with a `systemctl` fallback; `journalctl` for logs (configurable tail lines); start/stop/restart actions use `systemctl`
- `DockerProvider`: `docker ps`, `docker start/stop/restart/logs/inspect`
- `NetworkProvider`: fully asynchronous NetworkManager DBus (`org.freedesktop.NetworkManager`) reporting all active VPN-like tunnels (`vpn`, `tun`, `wireguard`, `ppp`), with an `nmcli` fallback, plus `nmcli` profile listing and `up`/`down` actions
- `MountProvider`: `findmnt --json` parsed via `ProviderParsers`, `/etc/fstab` + `MountProfileStore` merging, and `mount`/`umount` commands
- `SensorProvider`: `sensors -j` with automatic plain-text `sensors` fallback (`ProviderParsers::parseSensorsText`)
- `SmartProvider`: `lsblk -J`, `smartctl -j` supporting both ATA attributes and NVMe health logs (`ProviderParsers::parseSmartctlJson`), shared device rules in `SmartDeviceRules.h`, and `pkexec` fallback to the installed SMART helper

UI pages should stay thin: trigger provider methods, update models/status labels, and show confirmations/dialogs.

### SMART / polkit helper

SMART inventory is split into two privilege levels:

- The Disks page exposes two manual actions: `Check SMART` probes only the selected disk, while `Check All SMART` queues every listed disk so a multi-disk batch shares a single authorization prompt.
- `SmartProvider` runs `lsblk -J` and first attempts `smartctl -j -H -A` from the user's session.
- If `smartctl` reports a permission error, `SmartProvider` batches the active disk plus the remaining manual SMART queue into one `pkexec linux-service-dashboard-smart-helper` invocation.
- `linux-service-dashboard-smart-helper` validates narrow `/dev/...` disk paths and optional transports, then runs only the corresponding `smartctl` read commands.
- The helper does not expose mount, write, shell, or arbitrary command execution.

The helper policy source is `resources/io.github.Adiker.LinuxServiceDashboard.smart-helper.policy.in`. CMake generates the installed policy at `cmake --install` time from the install prefix, and the app resolves the helper through a configure-time relative path from `bindir` to `libexecdir` at runtime.

SMART checks are manual by default. An opt-in scheduled refresh (`SmartRefreshScheduler`, minimum 5-minute interval) re-runs `Check All` automatically; it is gated by `SmartProvider::isSmartCheckBusy()` so it never enqueues a second batch while one is in flight. Each successful check is appended to `SmartHistoryStore`, keyed by the disk's serial when available and otherwise by device path, and surfaced through the Disks page History dialog. Any change to automatic SMART cadence must preserve this rate limiting and explicit user opt-in.

---

## Build, Run, Install

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run:

```bash
build/linux-service-dashboard
```

Install to a prefix:

```bash
cmake --install build --prefix /usr/local
```

For `/usr/local` installs that need working SMART authorization, configure with `-DLSD_INSTALL_POLKIT_POLICY_SYSTEM=ON` so the policy lands in `/usr/share/polkit-1/actions`.

This installs:

- `bin/linux-service-dashboard`
- `${CMAKE_INSTALL_LIBEXECDIR}/linux-service-dashboard-smart-helper`
- `share/applications/io.github.Adiker.LinuxServiceDashboard.desktop`
- `share/icons/hicolor/*/apps/io.github.Adiker.LinuxServiceDashboard.png`
- `share/icons/hicolor/scalable/apps/io.github.Adiker.LinuxServiceDashboard.svg`
- `${LSD_POLKIT_POLICY_DIR}/io.github.Adiker.LinuxServiceDashboard.smart-helper.policy` (defaults to `share/polkit-1/actions` under the install prefix; use `LSD_INSTALL_POLKIT_POLICY_SYSTEM=ON` for the system polkit directory)

### Distro packaging

- **Arch Linux:** `packaging/PKGBUILD` builds a system package with polkit policy installed to `/usr/share/polkit-1/actions`.
- **Debian/RPM:** CPack generators are enabled in CMake. Debian packages use `dpkg-shlibdeps` for Qt runtime libraries plus explicit dependencies on CLI tools (`smartmontools`, `network-manager`, etc.). After configuring a Release build, run `cpack -G DEB` or `cpack -G RPM` from the build directory.
- **Builder prerequisites:** `cpack -G DEB` shells out to `dpkg-shlibdeps` and `file`, so the build host needs `dpkg-dev` and `file` installed (they are present by default on most Debian/Ubuntu systems but missing on minimal CI images). `cpack -G RPM` requires `rpmbuild` from the `rpm` package.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cpack -G DEB --config build/CPackConfig.cmake
```

---

## Checks

Provider parsers are covered by a CTest target (`provider-parser-tests`, built from `tests/test_provider_parsers.cpp`). For code changes, build and run the tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

For platform smoke tests where no display interaction is needed:

```bash
QT_QPA_PLATFORM=offscreen timeout 3s build/linux-service-dashboard
QT_QPA_PLATFORM=minimal timeout 3s build/linux-service-dashboard
```

When a real desktop session is available, also test:

```bash
QT_QPA_PLATFORM=wayland build/linux-service-dashboard
QT_QPA_PLATFORM=xcb build/linux-service-dashboard
```

---

## Known Limitations

- systemd listing uses asynchronous systemd1 DBus calls (linking `Qt6::DBus`) with a `systemctl` fallback; service control (start/stop/restart) still uses `systemctl`.
- VPN detection uses asynchronous NetworkManager DBus queries (with an `nmcli` fallback) and lists configured VPN profiles via `nmcli`; active VPN-like tunnel types (`vpn`, `tun`, `wireguard`, `ppp`) are included so externally created tunnels such as OpenConnect are visible.
- Mounting configured entries from the Mounts page runs `mount <target>` without elevation; entries without `user`/`users` in `/etc/fstab` will report a permission error.
- SMART checks are manual by default (with an opt-in scheduled refresh, minimum 5 minutes) and permission-dependent; installed builds use the polkit helper for authorized read-only SMART access.
- Disabling a module hides its page and skips its scheduled refresh, while the shared provider instance stays managed by `ServiceRegistry`.
- Automated coverage focuses on parsers, rules, stores, and `AlertMonitor` (`provider-parser-tests`); UI rendering and live system commands are validated via Qt QPA smoke tests and manual sessions.
