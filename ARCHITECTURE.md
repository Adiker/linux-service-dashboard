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
│   │   └── SmartHelper.cpp
│   ├── core/
│   │   ├── CommandResult.h
│   │   ├── CommandRunner.h/cpp
│   │   └── RefreshScheduler.h/cpp
│   ├── models/
│   │   ├── ServiceModel.h/cpp
│   │   ├── DockerContainerModel.h/cpp
│   │   ├── MountModel.h/cpp
│   │   ├── SensorModel.h/cpp
│   │   └── DiskModel.h/cpp
│   ├── services/
│   │   ├── SystemdServiceProvider.h/cpp
│   │   ├── DockerProvider.h/cpp
│   │   ├── NetworkProvider.h/cpp
│   │   ├── MountProvider.h/cpp
│   │   ├── SensorProvider.h/cpp
│   │   └── SmartProvider.h/cpp
│   ├── widgets/
│   │   ├── OverviewPage.h/cpp
│   │   ├── SystemdPage.h/cpp
│   │   ├── DockerPage.h/cpp
│   │   ├── VpnPage.h/cpp
│   │   ├── MountsPage.h/cpp
│   │   ├── SensorsPage.h/cpp
│   │   ├── DisksPage.h/cpp
│   │   ├── SettingsPage.h/cpp
│   │   ├── ConfirmActionDialog.h/cpp
│   │   └── LogViewerDialog.h/cpp
│   └── utils/
│       ├── JsonUtils.h/cpp
│       ├── StatusUtils.h/cpp
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
2. `MainWindow` builds the sidebar/stack UI and the system tray icon/menu.
3. `SettingsPage::settingsChanged` triggers `MainWindow::reloadSettings()`.
4. `RefreshScheduler` emits periodic refresh requests based on `refresh/intervalSeconds`.
5. `MainWindow::refreshAll()` fans out to all pages.
6. Pages call providers in `src/services/`.
7. Providers run system commands through `CommandRunner` and emit parsed rows/status back to the UI.

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
| `theme/preference` | string | `System` | `System`, `Light`, `Dark`, or `OLED` |
| `tables/<page>/headerState` | byte array | – | Saved column sizes/order per table (`systemd`, `docker`, `mounts`, `sensors`, `disks`); sort order is persisted only for the proxy-backed systemd table |

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

`MainWindow::applyTheme()` owns the central stylesheet.

Supported preferences:

- `System`: Light or Dark based on the current palette
- `Light`: explicit light theme
- `Dark`: explicit dark theme
- `OLED`: black-first high-contrast dark theme

Pages should use shared object names instead of local styles:

- `pageTitle` for page headings
- `card` for overview/VPN panels
- `cardValue` for overview card values

---

## Providers

Providers own command execution and parsing:

- `SystemdServiceProvider`: `systemctl`, `journalctl`
- `DockerProvider`: `docker ps`, `docker start/stop/restart/logs/inspect`
- `NetworkProvider`: NetworkManager DBus (`org.freedesktop.NetworkManager`) for active VPN-like connections (`vpn`, `tun`, `wireguard`, `ppp`), with an `nmcli` fallback when the system bus is unavailable. The `ActiveConnections` query is asynchronous; per-connection property reads use a bounded (5s) timeout.
- `MountProvider`: mount listing and unmount commands
- `SensorProvider`: `sensors -j` with text fallback
- `SmartProvider`: `lsblk -J`, `smartctl -j`, and `pkexec` fallback to the installed SMART helper

UI pages should stay thin: trigger provider methods, update models/status labels, and show confirmations/dialogs.

### SMART / polkit helper

SMART inventory is split into two privilege levels:

- The Disks page exposes two manual actions: `Check SMART` probes only the selected disk, while `Check All SMART` queues every listed disk so a multi-disk batch shares a single authorization prompt.
- `SmartProvider` runs `lsblk -J` and first attempts `smartctl -j -H -A` from the user's session.
- If `smartctl` reports a permission error, `SmartProvider` batches the active disk plus the remaining manual SMART queue into one `pkexec linux-service-dashboard-smart-helper` invocation.
- `linux-service-dashboard-smart-helper` validates narrow `/dev/...` disk paths and optional transports, then runs only the corresponding `smartctl` read commands.
- The helper does not expose mount, write, shell, or arbitrary command execution.

The helper policy source is `resources/io.github.Adiker.LinuxServiceDashboard.smart-helper.policy.in`. CMake generates the installed policy at `cmake --install` time from the install prefix, and the app resolves the helper through a configure-time relative path from `bindir` to `libexecdir` at runtime. Keep SMART checks manual unless a future change adds explicit rate limiting and user opt-in.

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

- systemd parsing uses command output instead of DBus.
- VPN status uses the NetworkManager DBus interface (linking `Qt6::DBus`), with an `nmcli` fallback when the system bus is unavailable. Active VPN-like tunnel types (`vpn`, `tun`, `wireguard`, `ppp`) are treated as connected so externally created tunnels such as OpenConnect are visible.
- SMART checks are manual and permission-dependent; installed builds use the polkit helper for authorized read-only SMART access.
- Disabling a module hides its page and skips its scheduled refresh, but the underlying provider classes are still constructed.
- Automated coverage is limited to the provider parser unit tests (`provider-parser-tests`); UI and provider command execution are still validated manually.
