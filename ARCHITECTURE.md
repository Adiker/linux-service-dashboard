# Architecture — Linux Service Dashboard

The application is a single local C++20 / Qt 6 Widgets process. It adds no network listener, account system or background daemon. `AGENTS.md` defines repository workflow and operational guardrails.

## Structure and data flow

- `src/core/`: shared snapshots, module descriptors, refresh coordination, settings/profile/history stores, bounded events and command execution.
- `src/services/`: native DBus/CLI providers, structured inventories, bounded local-file discovery and specialist tool launching.
- `src/models/`: existing table models and stable row identities.
- `src/widgets/`: grouped navigation content, common tree-table pages, metric chart, existing disk/service/mount pages and dialogs.
- `src/utils/`: parsing helpers, table persistence and filter/detail/selection utilities.
- `translations/`: Qt Linguist Polish catalog; English is the source language.
- `tests/`: parser, runner, provider and UI/evidence regression targets.

`MainWindow` owns one `SystemCenter`. It builds pages from stable module IDs and a grouped sidebar. Existing pages borrow shared providers; overview/search use their published snapshots. There are no separate overview collectors.

1. A page or scheduler requests a module refresh.
2. Providers collect asynchronously with one in-flight request per context/module.
3. `SystemCenter::publish` stores successful rows and their timestamp. Failure retains previous rows/time and marks them stale; partial initial data carries an error rather than a successful timestamp.
4. Visible pages render cached data. Hidden tree pages defer rendering until navigation, avoiding repeated catalog rebuilds.
5. Shared service/timer/mount/container data supplies backup, synchronization and server views. Source failures remain failures in derived views.
6. Explicit actions go through confirmations and provider validation. Typed outcomes produce standardized local events.

`CenterRecord` carries a stable ID, display cells, optional hierarchy, narrow action metadata and an attention flag. `CenterSnapshot` carries headers, rows, last successful read time and error. Search preserves destination module and entity identity. Favorites are cached in memory and persisted only on changes.

## Module registry and scheduling

`centerModules()` supplies IDs, translated titles/groups, required tool hints and on-demand flags. Tool availability does not imply configuration or a running service. Optional capabilities remain visible in settings. Tool checks include the packaged KopiaUI CLI.

Regular enabled-module refresh defaults to 30 seconds. App/package/autostart/VM/tool and image/volume/snapshot inventories are on demand. Global searches of at least two characters initialize uncached app/package catalogs. Metrics and Docker stats sample every two seconds only while their page/window is visible. Disabling a module prevents new collection. Derived pages do not enable disabled source collectors.

`CommandRunner` uses QProcess with closed stdin, stable command locale, timeout and bounded output. It deduplicates active contexts, caps each output stream at 2 MiB and retains at most 100 failure summaries without arguments/output. Truncated structured output is a failure. Logs can show retained stdout/stderr and the truncation marker. Shutdown disconnects completion callbacks before terminating owned collector processes; separately launched applications are not owned collectors.

## Providers and identities

### Services and containers

`ServiceScope` / `ServiceRef` separate system and user managers. Service rows retain scope throughout filtering, search, journal and control. Asynchronous bounded `ListUnits` calls use the appropriate DBus connection; asynchronous systemctl fallback and unit-file enumeration include unloaded installed services. Groups filter locally and preserve missing watched-unit placeholders. An unqualified legacy group entry may match either scope; `user:` / `system:` disambiguates it.

Control verbs and unit names are validated. Critical session units are read-only. Tables use single-row selection; sorting/filtering map back to source identity before any action.

Docker ps uses an explicit JSON projection, avoiding full-format fields such as sizes and unrelated labels. It supplies Compose project/service labels, health, images and ports without reading container environments. Hierarchy is project → service → container, with a standalone group. Only running unhealthy containers are attention items. Stats are separate visible-page reads. Images/volumes are inventory only; actions remain per container and require confirmation.

### Network

NetworkManager active-connection and device properties use asynchronous bounded DBus calls. An escaping-aware nmcli fallback handles names containing colons. Kernel links provide external tunnel presence and merge by interface. Neither source verifies connectivity. The legacy single-VPN parser type remains for source/parser compatibility; UI consumes a collection.

Read-only ip JSON, DNS, firewall-zone and ss collections do not edit configuration or scan the network. Panel URLs are user-configured, indexed by hashed entity ID, and validated both on save and open. Credentials, query parameters and fragments are rejected.

### Storage, SMART and backups

Mount discovery lists relevant local/network filesystems and rclone without traversing their contents or triggering automount paths. Configured fstab/profile entries remain distinct from mounted entries; all occupied targets prevent an unrelated configured row from becoming an unmount action. Core system mountpoints and non-absolute targets are rejected. New profiles omit inline password options; existing settings are preserved.

Disk inventory uses lsblk. Manual SMART first tries user access, then a narrow installed helper through pkexec. A batch shares authorization. The helper accepts validated disk paths/transports and only SMART read operations; it cannot execute arbitrary shell, mount or write commands. Policy generation honors the install prefix, while the application resolves the helper relative to bindir.

The existing opt-in SMART schedule keeps its minimum five-minute interval and busy guard. History uses a unique serial when available, otherwise path. Shared overview SMART state is invalidated when the disk identity at a path changes. Access failures stay unknown, distinct from reported disk failure.

Backup jobs read only whitelisted systemd outcome/exit/finish properties. Schedules match scope and timer activation target. A successful job or listed snapshot is never marked verified. Kopia only lists snapshots from an explicit existing config with closed stdin, keyring authentication disabled and credential persistence disabled. No repository is connected, initialized, restored or verified. Sync views report daemon/mount state, not completed transfers.

### Local inventories and launch

`SystemInventory` discovers XDG application IDs with user precedence, Hidden/NoDisplay masks, desktop visibility, TryExec and localized names/comments. Effective desktop entries are grouped by category. Native package inventories support Pacman, dpkg-query and RPM; Flatpak installation/ref identities remain distinct. Foreign Pacman packages are not assumed to be AUR.

Qt Concurrent performs bounded catalog/procfs/sysfs discovery outside the GUI thread. Metrics use CPU deltas, memory availability and available GPU counters; chart history is in-memory and bounded to 450 visible samples. Audio/node output and power metadata are projected through whitelists. VM views list VirtualBox registrations/running machines and QEMU process presence without reading process command lines or claiming guest health.

Catalog launches validate desktop paths and run `gio launch` asynchronously so errors are surfaced. A fixed allowlist opens specialist GUIs separately. No desktop Exec, profile, arbitrary user shell command or credentials are interpreted by the dashboard.

## Settings, events and diagnostics

Organization/application identity stays `LocalTools` / `Linux Service Dashboard`.

| Key family | Contract |
|---|---|
| `schema/version` | Additive schema version 2; future versions are not downgraded |
| `refresh/intervalSeconds` | Default 30, minimum 5 |
| `modules/<id>` | Default enabled; overview/settings remain available |
| `systemd/groups`, `systemd/group/*`, `activeGroup`, `watchedServices` | Preserve legacy groups/mirror; `systemd/showAll` distinguishes the all-services view |
| `mounts/*` | Existing saved profiles preserved |
| `smart/*` | Existing schedule/history preserved |
| `theme/preference` | Stable `System`, `Light`, `Dark`, `OLED` values independent of translated labels |
| `ui/language` | `System`, `pl`, `en`; restart to apply |
| `favorites/ids` | Stable module/entity pairs |
| `notifications/enabled` | Default false; attention increases rate limited to one notification per minute |
| `panels/<hash>` | Validated user-approved panel address |
| `tables/*`, `centerTables/*` | Existing layouts and new tree header states; ignore automatic pre-data header changes |
| `backups/kopiaConfig` | Explicit path to an existing repository configuration; never copied into diagnostics |

`EventStore` retains at most 1000 standardized records in application-local `events.json`, written atomically with owner-only file permissions. Records contain time/module/code, not command arguments, output or raw error text.

Diagnostic export is an explicit projection: app/Qt/kernel/platform version, generation time, schema version and module ID/enabled/tool availability/count/last-success/error flags. It never serializes records, QSettings values, logs, paths, account names, device identities or network addresses. Preview precedes saving; output is owner-readable/writable.

## Theme and desktop integration

`MainWindow::applyTheme` owns the application palette and stylesheet, including custom-painter colors. It retains the initial system palette for System mode. No page applies a private palette. Tables convey textual status; attention icons supplement it. The sidebar is grouped/collapsible and search navigates by identity.

Keep desktop id `io.github.Adiker.LinuxServiceDashboard` before showing windows. Runtime window/tray icons use embedded PNG resources via `serviceDashboardIcon()`; the tray attention badge starts with that same icon. Installed desktop and hicolor names stay unchanged. Qt chooses the compositor backend; code never forces QT_QPA_PLATFORM or layer-shell.

## Build, install and validation

CMake requires Qt Core/Widgets/DBus/Concurrent/Test/LinguistTools. `dashboard-core` is an object library shared by the application and UI tests. Translations are embedded in the executable. Build identity records version/revision and dirty state; source archives without Git metadata report unknown revision. `--version` exits before QApplication.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Repeat with Debug for release validation. Existing install rules place the binary, SMART helper, generated policy, desktop entry and icons under the selected prefix. `/usr/local` needs `LSD_INSTALL_POLKIT_POLICY_SYSTEM=ON` to put policy in the system actions directory. Test relocatable installs using DESTDIR before system installation; preserve the existing install/settings for rollback.

CTest targets:

- `provider-parser-tests`: baseline parsing fixtures.
- `command-runner-tests`: missing programs, timeout, request coalescing and output bounds.
- `center-provider-tests`: scopes/unloaded units, Compose, schedules/outcomes, escaped names, XDG masks/localization, units and package projections.
- `center-ui-tests`: temporary settings, migration/report privacy, evidence boundaries, sorted action identities, stable theme/language values, all pages in both languages and responsive asynchronous inventories. It does not execute destructive actions, launch real applications or run SMART checks.

Use offscreen/minimal startup checks and normal Wayland/XWayland sessions when available. UI test `LSD_CAPTURE_DIR` writes local review PNGs; `QT_SCALE_FACTOR` supports per-process scaling checks without changing desktop settings. Do not publish host-data screenshots without reviewing them.

## Limits

Structured task views require a systemctl version with JSON output; older versions report unavailable. Optional tools/permissions/daemons may limit data. Backups/sync/GPU configuration remain in their native tools; synchronization progress, repository verification and cloud account credentials are not implemented. Raw command/tool output remains in its original language. Destructive operations are intentionally excluded from automated live-system tests.
