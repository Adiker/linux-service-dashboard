# Linux Service Dashboard

Linux Service Dashboard is a local Qt 6 Widgets desktop application for monitoring common Linux services and system components from one place. It is written in C++20 and targets modern Linux desktops, including KDE Plasma on Wayland.

## Features

- Overview cards for Docker, systemd, VPN, mounts, sensors, and disk health.
- systemd service table with filtering, refresh, start, stop, restart, and journal viewing.
- Docker container table using the Docker CLI, with start, stop, restart, logs, and inspect JSON actions.
- NetworkManager VPN status via `nmcli`.
- CIFS/NFS/sshfs mount listing with file manager open and confirmed unmount.
- Temperature/sensor display using `sensors -j` with a text fallback.
- Disk inventory via `lsblk -J` and manual SMART checks via `smartctl -j`.
- QSettings-backed refresh interval, watched services, module toggles, and theme preference.
- Light, Dark, and OLED themes with system-theme auto mode.
- System tray menu with Show Dashboard, Refresh Now, and Quit.
- Embedded application icon plus installable desktop entry for KDE Plasma Wayland/X11 taskbar matching.

Podman, remote monitoring, web servers, authentication, and cloud sync are intentionally out of scope for this MVP.

## Dependencies

Arch Linux:

```bash
sudo pacman -S qt6-base cmake gcc docker networkmanager util-linux lm_sensors smartmontools
```

Debian/Ubuntu-like systems:

```bash
sudo apt install qt6-base-dev cmake g++ docker-cli network-manager util-linux lm-sensors smartmontools
```

Some commands may require the current user to have permissions, for example access to the Docker socket or SMART data. The app does not ask for sudo passwords and reports permission errors in the UI.

## Build

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j$(nproc)
./linux-service-dashboard
```

## Install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cmake --install build --prefix /usr/local
```

The install step places the binary, `.desktop` entry, and hicolor PNG/SVG icons under the chosen prefix. The desktop id is `io.github.Adiker.LinuxServiceDashboard`; KDE Plasma Wayland uses it to match the taskbar icon to the same icon used by the tray.

## Themes

Settings → Theme supports:

- `System`
- `Light`
- `Dark`
- `OLED`

OLED uses a black-first palette with brighter contrast for OLED displays and dark desktop setups.

## Known Limitations

- systemd service parsing uses `systemctl list-units --plain`; it is robust enough for the MVP but should eventually move to DBus.
- SMART checks are manual from the disks page and cached by the UI; frequent automatic SMART polling is intentionally avoided.
- VPN detection uses `nmcli`; the provider is shaped so it can later be replaced with NetworkManager DBus calls.
- Mount profiles and fstab parsing are not implemented yet.
- Module toggles are persisted but do not yet hide pages.
- Parser/provider behavior is currently validated by manual build and smoke tests; dedicated unit tests are planned.

## Screenshots

Screenshots will be added once the UI stabilizes:

- Overview page
- systemd services page
- Docker containers page
- Sensors and SMART pages
