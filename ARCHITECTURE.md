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
│   ├── icons/
│   │   └── linux-service-dashboard-*.png
│   ├── linux-service-dashboard.svg
│   └── resources.qrc
├── src/
│   ├── main.cpp
│   ├── MainWindow.h/cpp
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
| `systemd/watchedServices` | string list | Docker, NetworkManager, sshd, PostgreSQL, Redis | Units shown in the systemd page |
| `modules/systemd` | bool | `true` | Reserved module toggle |
| `modules/docker` | bool | `true` | Reserved module toggle |
| `modules/vpn` | bool | `true` | Reserved module toggle |
| `modules/mounts` | bool | `true` | Reserved module toggle |
| `modules/sensors` | bool | `true` | Reserved module toggle |
| `modules/smart` | bool | `true` | Reserved module toggle |
| `theme/preference` | string | `System` | `System`, `Light`, `Dark`, or `OLED` |

Module toggles are persisted by the settings UI but are not yet used to hide pages.

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
- `NetworkProvider`: `nmcli` active connection parsing for VPN-like types (`vpn`, `tun`, `wireguard`, `ppp`)
- `MountProvider`: mount listing and unmount commands
- `SensorProvider`: `sensors -j` with text fallback
- `SmartProvider`: `lsblk -J`, `smartctl -j`

UI pages should stay thin: trigger provider methods, update models/status labels, and show confirmations/dialogs.

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

This installs:

- `bin/linux-service-dashboard`
- `share/applications/io.github.Adiker.LinuxServiceDashboard.desktop`
- `share/icons/hicolor/*/apps/io.github.Adiker.LinuxServiceDashboard.png`
- `share/icons/hicolor/scalable/apps/io.github.Adiker.LinuxServiceDashboard.svg`

---

## Checks

There is no CTest suite yet. For code changes, run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
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
- VPN status uses `nmcli` instead of NetworkManager DBus. Active VPN-like tunnel types (`vpn`, `tun`, `wireguard`, `ppp`) are treated as connected so externally created tunnels such as OpenConnect are visible.
- SMART checks are manual and permission-dependent.
- Module toggles are saved but do not yet hide pages.
- There is no automated parser test suite yet.
