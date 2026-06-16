# CLAUDE.md — linux-service-dashboard

This is the concise working guide for Claude in this repository. Mandatory agent rules live in `AGENTS.md`; treat that file as authoritative for git workflow, branch hygiene, PR expectations, and implementation guardrails.

## Project Snapshot

- **What:** Local Linux desktop dashboard for systemd services, Docker containers, VPN status, network mounts, sensors, and disk/SMART checks.
- **Stack:** C++20, Qt6 Widgets, CMake 3.21+.
- **Runtime commands:** `systemctl`, `journalctl`, `docker`, `nmcli`, `findmnt`, `sensors`, `lsblk`, `smartctl`.
- **Platform:** Linux only. KDE Plasma is the primary desktop target; the app should work on both Wayland and X11/XWayland.

## Read First

- `AGENTS.md` — mandatory workflow and sharp edges for agents.
- `ARCHITECTURE.md` — technical reference: project structure, module map, settings, platform integration, packaging, and checks.
- `README.md` — user-facing setup, features, dependencies, build, usage, and troubleshooting.
- `ROADMAP.md` — backlog and planned hardening work.

## Common Commands

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run:

```bash
build/linux-service-dashboard
```

Smoke-test Qt startup:

```bash
QT_QPA_PLATFORM=offscreen timeout 3s build/linux-service-dashboard
QT_QPA_PLATFORM=minimal timeout 3s build/linux-service-dashboard
```

## Claude Working Notes

- Do not duplicate architectural detail here. Add durable technical reference material to `ARCHITECTURE.md`.
- Put end-user behavior, install steps, and troubleshooting in `README.md`.
- Put agent-only workflow mistakes or non-obvious guardrails in `AGENTS.md`.
- Keep desktop icon changes aligned across `src/main.cpp`, `src/MainWindow.cpp`, `resources/resources.qrc`, and `resources/io.github.Adiker.LinuxServiceDashboard.desktop`.
- For UI theme changes, update all four preferences: `System`, `Light`, `Dark`, `OLED`.
