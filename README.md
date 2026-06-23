# Linux Service Dashboard

Linux Service Dashboard is a local Qt 6 Widgets desktop application for monitoring common Linux services and system components from one place. It is written in C++20 and targets modern Linux desktops, including KDE Plasma on Wayland.

## Features

- Overview cards for Docker, systemd, VPN, mounts, sensors, and disk health.
- systemd service table with filtering, refresh, start, stop, restart, and journal viewing.
- Docker container table using the Docker CLI, with start, stop, restart, logs, and inspect JSON actions.
- NetworkManager VPN/tunnel status via its DBus interface (asynchronous, with a short timeout), falling back to `nmcli` when the system bus is unavailable; external `tun` links such as OpenConnect are included.
- CIFS/NFS/sshfs mount listing with file manager open and confirmed unmount.
- Temperature/sensor display using `sensors -j` with a text fallback.
- Disk inventory via `lsblk -J` and manual SMART checks via `smartctl -j`, with an installed polkit helper for permission-gated SMART reads.
- QSettings-backed refresh interval, watched services, module toggles, and theme preference.
- Light, Dark, and OLED themes with system-theme auto mode.
- System tray menu with Show Dashboard, Refresh Now, and Quit.
- Embedded application icon plus installable desktop entry for KDE Plasma Wayland/X11 taskbar matching.

Podman, remote monitoring, web servers, authentication, and cloud sync are intentionally out of scope for this MVP.

## Dependencies

Arch Linux:

```bash
sudo pacman -S qt6-base cmake gcc docker networkmanager util-linux lm_sensors smartmontools polkit
```

Debian/Ubuntu-like systems:

```bash
sudo apt install qt6-base-dev cmake g++ docker-cli network-manager util-linux lm-sensors smartmontools policykit-1
```

Some commands may require the current user to have permissions, for example access to the Docker socket. SMART checks first try the user's session and, when installed, fall back to a small `pkexec`/polkit helper that runs the SMART read batch after one administrator authentication.

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

The install step places the main binary, SMART helper, polkit policy, `.desktop` entry, and hicolor PNG/SVG icons under the chosen prefix. The desktop id is `io.github.Adiker.LinuxServiceDashboard`; KDE Plasma Wayland uses it to match the taskbar icon to the same icon used by the tray.

For SMART checks that require elevated disk access, install the project into a system prefix so polkit can find the helper policy:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLSD_INSTALL_POLKIT_POLICY_SYSTEM=ON
sudo cmake --install build --prefix /usr/local
```

When the app lives under `/usr/local`, polkit still loads action files from `/usr/share/polkit-1/actions`, so pass `-DLSD_INSTALL_POLKIT_POLICY_SYSTEM=ON` for that layout. By default the policy installs under the chosen prefix (`share/polkit-1/actions`), which keeps staged or relocatable installs working.

If you use a different install prefix, pass the same `--prefix` to `cmake --install` so the generated polkit policy points at the final helper path. The app resolves the helper relative to its own install location at runtime.

The helper is installed as `libexec/linux-service-dashboard-smart-helper` and is only invoked through `pkexec` for SMART reads. When several disks need elevated access, the app batches them into one helper invocation so polkit should ask once for that manual check.

## Themes

Settings → Theme supports:

- `System`
- `Light`
- `Dark`
- `OLED`

OLED uses a black-first palette with brighter contrast for OLED displays and dark desktop setups.

## Known Limitations

- systemd service parsing uses `systemctl list-units --plain`; it is robust enough for the MVP but should eventually move to DBus.
- SMART checks are manual from the disks page and cached by the UI; frequent automatic SMART polling is intentionally avoided. The polkit helper covers normal installed use, but unusual USB bridges may still require bridge-specific `smartctl -d` options.
- VPN detection queries NetworkManager over DBus (with an `nmcli` fallback) and treats VPN-like tunnel types such as `vpn`, `tun`, `wireguard`, and `ppp` as connected.
- Mount profiles and fstab parsing are not implemented yet.
- Module toggles are persisted but do not yet hide pages.
- Parser/provider behavior is currently validated by manual build and smoke tests; dedicated unit tests are planned.

## Screenshots

Screenshots will be added once the UI stabilizes:

- Overview page
- systemd services page
- Docker containers page
- Sensors and SMART pages
