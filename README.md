# Linux Service Dashboard

Linux Service Dashboard is a local Qt 6 Widgets desktop application for monitoring common Linux services and system components from one place. It is written in C++20 and targets modern Linux desktops, including KDE Plasma on Wayland.

## Features

- Clickable Overview cards for Docker, systemd, VPN, mounts, sensors, and disk/SMART health with live status indicators.
- systemd service table with search/state filtering, sortable columns, service groups (add/rename/delete), "All services" toggle, context menu, start/stop/restart, and enhanced journal log viewing.
- Docker container table using the Docker CLI, with search/state filtering, sortable columns, context menu, start/stop/restart, refreshable logs, and JSON inspection.
- NetworkManager VPN/tunnel monitoring via asynchronous DBus (with `nmcli` fallback), multi-tunnel reporting, configured VPN profile list, and connect/disconnect actions; external `tun` links such as OpenConnect are included.
- CIFS/NFS/sshfs mount listing alongside `/etc/fstab` entries and saved mount profiles, with file manager open, mount, confirmed unmount, and save/remove profile actions.
- Temperature/sensor display using `sensors -j` with automatic plain-text fallback, search/warning filter, and numeric sorting.
- Disk inventory via `lsblk -J` and manual or opt-in scheduled SMART checks via `smartctl -j` (supporting both ATA attributes and NVMe health logs), with an installed polkit helper and SMART history dialog (CSV export and clear).
- Enhanced log viewer (`Ctrl+F` search, monospace font, word-wrap toggle, copy/save, live refresh with selectable tail size) and Command Diagnostics dialog for failed commands.
- Edge-triggered system tray notifications (failed services, high sensors, SMART warnings, VPN disconnects), dynamic tray tooltip, remembered window geometry, and close-to-tray / start-minimized options.
- QSettings-backed refresh interval, watched service groups, module toggles, persistent table layouts/sorting, and theme preference.
- Light, Dark, and OLED themes with system-theme auto mode and theme-aware status badges.
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

### Distro packages

- Arch Linux: use `packaging/PKGBUILD` from a release tarball.
- Debian/RPM: configure a Release build, then run `cpack -G DEB` or `cpack -G RPM` from the build directory.

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

- systemd listing uses asynchronous systemd1 DBus calls with a `systemctl list-units --plain` fallback; service control actions still run through `systemctl`.
- SMART checks are manual by default and cached by the UI. An opt-in scheduled refresh (Settings → SMART scheduling, minimum 5-minute interval) can re-run checks automatically; it is skipped while a check is already in progress. The polkit helper covers normal installed use, but unusual USB bridges may still require bridge-specific `smartctl -d` options.
- VPN detection queries NetworkManager over asynchronous DBus (with an `nmcli` fallback) and treats VPN-like tunnel types such as `vpn`, `tun`, `wireguard`, and `ppp` as connected.
- Mounting configured fstab/profile entries runs `mount <target>` without `sudo`; shares that do not specify `user`/`users` in `/etc/fstab` will report a permission error, and the app does not edit `/etc/fstab`.
- Module toggles hide a page from the sidebar and skip its scheduled refresh; the Overview and Settings pages are always available.
- Core parsers, device validation rules, settings stores, and alert transitions have unit tests (`ctest --test-dir build`); UI behavior and live system commands are validated by Qt QPA smoke tests and manual sessions.

## Screenshots

Screenshots will be added once the UI stabilizes:

- Overview page
- systemd services page
- Docker containers page
- Sensors and SMART pages
