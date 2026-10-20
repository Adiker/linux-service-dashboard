# Linux Service Dashboard

A local C++20 / Qt 6 Widgets system center for Linux desktops, including KDE Plasma on Wayland and X11. The interface supports English and Polish, grouped navigation, shared status data and optional modules.

## What is included

| Area | Views |
|---|---|
| Dashboard | Overview cards, pinned items, items requiring attention and active collections |
| Services and tasks | System/user services, installed unloaded units, groups, timers, sockets, autostart and journal |
| Containers | Compose project → service → container hierarchy, health, ports, visible-page CPU/memory, logs and inspect; read-only images/volumes |
| Network and access | Interfaces, addresses, routes, DNS, transfer counters, all detected VPN/tunnels, read-only firewall zones and listening ports |
| Data and disks | Disk/partition inventory, capacity, local/network/rclone mounts, manual SMART, history and opt-in scheduling |
| Backups and synchronization | Backup job outcomes and schedules, existing-repository Kopia snapshots, Syncthing/rclone service and mount state |
| Hardware and session | CPU/RAM/swap/GPU/VRAM, short metric history, sensor units, audio, Bluetooth, power, printing and USB |
| Servers and sharing | Shared systemd/container status and user-configured panel links |
| Software and environments | Categorized XDG application catalog, native packages, Flatpak apps/runtimes, VM and tool inventories |
| Diagnostics and settings | Bounded local events, diagnostic report preview/export, module toggles, language, themes and refresh settings |

Ctrl+K searches pages and indexed entities. Searching at least two characters also initializes the application/package catalogs when needed. Catalogs otherwise load on entering their page or on explicit refresh. Select a row to see its fields; tables support filtering and sorting. New tree tables also allow column visibility from the header context menu.

Pin an item from its page or search results to keep it on the dashboard. Service groups and a scope selector allow personal views without restricting the shared service inventory.

## Status and actions

Installed tools, configured entries, running processes and successful operations are separate observations:

- Stopped unused services/containers are not automatically alarms. A running container with an unhealthy check is flagged.
- A tunnel or NetworkManager activation does not prove reachability.
- A successful backup job does not verify backup integrity. Snapshot listing is not verification either.
- A running synchronization daemon or mounted cloud drive does not prove transfer completion.
- QEMU process detection does not prove guest health.
- A failed refresh retains the previous data with an unavailable/stale indication and its last successful read time.

Service/container start, stop and restart and mount unmount keep confirmation dialogs. Critical session services and core system mountpoints are read-only. There are no bulk removals, package updates, volume pruning, backup restores, firewall/VPN edits, hardware tuning, remote monitoring server or dashboard login system.

Open specialist tools for configuration. Applications launch through their desktop entries using `gio`; the dashboard does not interpret desktop `Exec` commands. Local server/synchronization panel addresses are explicitly configured per selected item and must be HTTP(S) without credentials or query parameters.

## Build

Arch/EndeavourOS build dependencies:

```bash
sudo pacman -S qt6-base qt6-tools cmake gcc
```

Debian/Ubuntu build dependencies:

```bash
sudo apt install qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools cmake g++
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
build/linux-service-dashboard
```

`linux-service-dashboard --version` prints the version and configured Git revision without needing a display. Dirty builds are identified. About is available in the tray menu.

## Optional runtime tools

Missing tools, inaccessible daemons and permission failures are reported in the relevant view. The dashboard never installs tools automatically.

| Integration | Tools |
|---|---|
| Services/tasks | systemd DBus, `systemctl`, `journalctl`; structured task views require systemctl JSON support |
| Containers | `docker` with a usable local/user-configured context; Compose metadata comes from labels |
| Network | NetworkManager DBus / `nmcli`, `ip`, `ss`, `firewall-cmd` |
| Disks/mounts/sensors | `lsblk`, `findmnt`, `umount`, `smartctl`, `sensors` |
| Backups | Existing systemd jobs; `kopia` or the CLI bundled with KopiaUI |
| Session hardware | `pw-dump`, `bluetoothctl`, `upower`, optional `powerprofilesctl`, `lpstat`, `lsusb` |
| Packages/VMs | Pacman, dpkg-query or RPM; optional Flatpak and VBoxManage; QEMU process inventory |
| GUI launch | `gio` (`glib2` on Arch; `libglib2.0-bin` on Debian), installed specialist applications |

Kopia snapshots requires selecting an existing repository configuration. Reads close stdin, avoid keyring authentication and do not persist credentials. An inaccessible repository stays unknown/unavailable. Creating/connecting/restoring repositories is left to KopiaUI. Timeshift restore stays in Timeshift.

## Install and desktop integration

Before replacing an existing installation, retain its binary/install prefix and QSettings file for rollback. Build and test the candidate first.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLSD_INSTALL_POLKIT_POLICY_SYSTEM=ON
cmake --build build -j$(nproc)
sudo cmake --install build --prefix /usr/local
```

The install includes the binary, narrow SMART helper, polkit policy, desktop entry and hicolor icons. The desktop id is `io.github.Adiker.LinuxServiceDashboard`. Window/tray icons are embedded PNGs, so an uninstalled build also has an icon.

For `/usr/local`, polkit loads policies from `/usr/share/polkit-1/actions`; enable `LSD_INSTALL_POLKIT_POLICY_SYSTEM` as above. For staged/relocatable installs, leave it off to place policy files under the chosen prefix. The policy is generated at install time for that prefix, and the helper path resolves relative to the application.

SMART first uses the user's permissions, then the installed narrow `pkexec` helper when required. Manual batches share one authorization request. The helper only accepts validated disk paths and SMART read operations. The application does not request or store sudo passwords.

## Settings and privacy

Settings retain the `LocalTools` / `Linux Service Dashboard` QSettings identity. Migration is additive: service groups, mount profiles, table layouts, SMART history and theme remain. Theme values are `System`, `Light`, `Dark`, `OLED`; language selection takes effect after restart.

Regular enabled-module status refresh defaults to 30 seconds. Performance and container resource sampling run every two seconds only on their visible page. SMART is manual unless its minimum-five-minute opt-in schedule is enabled. Disabling a module skips its collection; derived service views indicate unavailable sources rather than claiming cached state is fresh.

Desktop notifications are off by default. When enabled, attention increases are deduplicated and rate limited. Local events retain at most 1000 standardized entries and never store raw command arguments/output. Diagnostic export has a preview and only includes version/platform metadata, module availability/counts/timestamps and error flags. It excludes settings values, raw logs, paths, account names, device identities and network addresses. Newly saved mount profiles omit inline password options; existing profiles are not rewritten.

## Validation and limits

CTest covers parsers, command timeouts/output limits, scoped fallbacks, backup evidence, XDG precedence, sorted selection, settings migration, safe reports and navigation in both languages. UI tests use temporary settings and never invoke destructive actions or automatic SMART checks.

```bash
QT_QPA_PLATFORM=offscreen timeout 3s build/linux-service-dashboard
QT_QPA_PLATFORM=minimal timeout 3s build/linux-service-dashboard
```

Exit 124 from these commands means the app remained alive until timeout. Also smoke-test a normal Wayland session and `QT_QPA_PLATFORM=xcb` when available. The application never forces a platform plugin or layer-shell.

Optional daemon/API configuration, synchronization progress, backup verification and advanced GPU controls remain in their native tools. USB bridge SMART transports may need device-specific support. See [ARCHITECTURE.md](ARCHITECTURE.md) for internal contracts and [ROADMAP.md](ROADMAP.md) for delivery status.

Firewall inspection uses asynchronous DBus reads with a noninteractive polkit permission check for the same bus sender before each protected request. Missing authorization is reported in the view; open `firewall-config` explicitly to authenticate. Background refresh does not invoke `firewall-cmd`.
