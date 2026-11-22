# System center delivery roadmap

The MVP already includes parser tests, module toggles, service groups, DBus providers,
mount profiles, table persistence, and opt-in SMART history/scheduling. These are baseline features.

1. Foundation: identifiable builds and current documentation.
2. Navigation/data: grouped modules, shared snapshots, search, Polish/English UI.
3. Services/containers: both systemd scopes, tasks, Compose hierarchy and logs.
4. Data protection: backup outcomes, schedules, synchronization, local/cloud mounts.
5. Network/server views: interfaces, tunnels, read-only firewall/ports, server shortcuts.
6. Hardware/software: metrics/devices, XDG applications, package and VM inventories.
7. Release: diagnostics, bounded events, optional notifications, regression and platform checks.

Each increment targets main in a separate PR. Dependent increments include preceding
commits until those PRs merge; review the increment commit separately in that case.
No automatic privileged configuration, package removal, volume pruning, or backup restore.
