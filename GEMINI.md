# GEMINI.md — linux-service-dashboard

This is the concise working guide for Gemini / Antigravity agents in this repository. It summarizes the rules that matter during implementation; the full project reference lives in `ARCHITECTURE.md`, Claude-specific quick-start guidance lives in `CLAUDE.md`, and sharp agent-specific constraints live in `AGENTS.md`.

## Project Overview

- **What:** Qt6 desktop dashboard for local Linux service and hardware status.
- **Stack:** C++20, Qt6 Widgets, CMake 3.21+.
- **Platform:** Linux desktop, with KDE Plasma Wayland and X11/XWayland as explicit targets.
- **Integration:** System tray, installable `.desktop` file, embedded Qt resource icon, QSettings-backed preferences.

## Non-Negotiable Rules

- Do not commit directly to `main` unless the user explicitly asks.
- Do not force-push, delete branches, or rewrite published history without explicit consent.
- Do not shell out directly from widget code; add or adjust provider methods in `src/services/`.
- Destructive operations must keep confirmation dialogs.
- Do not force a Qt platform plugin. Wayland/X11 selection belongs to Qt and the desktop session.
- Keep `QGuiApplication::setDesktopFileName("io.github.Adiker.LinuxServiceDashboard")`; it is required for KDE Plasma Wayland taskbar icon matching.
- Keep tray and taskbar icons backed by `serviceDashboardIcon()`, the desktop id, and the installed hicolor icon name.
- Do not split Light/Dark/OLED styling across many widgets. Prefer `MainWindow::applyTheme()`.

## Architecture Map

- `src/main.cpp`: QApplication setup, metadata, desktop file name, global app icon.
- `src/MainWindow.*`: main navigation, tray menu, theme application, refresh fan-out.
- `src/widgets/`: pages and dialogs.
- `src/services/`: command providers and parsing.
- `src/models/`: Qt table models.
- `src/core/CommandRunner.*`: async command execution.
- `src/utils/`: formatting/status helpers.
- `resources/`: embedded icon QRC and installable desktop/icon assets.

## Development Workflow

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run:

```bash
build/linux-service-dashboard
```

Smoke-test startup:

```bash
QT_QPA_PLATFORM=offscreen timeout 3s build/linux-service-dashboard
QT_QPA_PLATFORM=minimal timeout 3s build/linux-service-dashboard
```

## Risk Areas

- systemd and Docker control actions: keep confirmation, surface details, refresh after completion.
- Mount unmount: never auto-unmount from refresh logic.
- SMART: keep checks manual unless a future change adds explicit rate limiting and user opt-in.
- Desktop icon matching: app id, desktop filename, installed icon name, and fallback theme icon must stay in sync.
- Theme contrast: verify OLED does not hide borders, disabled text, selections, or table headers.
