# AGENTS.md — linux-service-dashboard

Comprehensive project docs are in `ARCHITECTURE.md`. This file covers the sharp edges an agent would otherwise guess wrong.

## Git Workflow (mandatory)

- Never commit directly to `main` unless explicitly asked by the user.
- Always create a branch from the latest `origin/main` when a GitHub remote already exists.
- Use branch prefixes: `feature/`, `fix/`, `refactor/`, `docs/`, `chore/`.
- Push the branch and open a PR to `main` unless the user explicitly asks for direct publication.
- Never force-push to `main`.
- Never delete branches without explicit consent.
- Never rewrite published history without explicit consent.
- Before opening a PR, run relevant build/tests if the change warrants it.
- For risky areas (systemd, Docker CLI control, mount/unmount actions, SMART, CMake packaging, Wayland/X11 icon handling), add a short risk/rollback note in the PR description.

## Documentation

- For every user-visible or operational change, check whether docs need updating.
- Update `README.md` for end-user behavior, setup, configuration, desktop integration, and troubleshooting.
- Update `ARCHITECTURE.md` for architecture, build/test recipes, packaging, platform behavior, and maintainer workflows.
- Update `CLAUDE.md` only for Claude-specific quick-start guidance.
- Update `GEMINI.md` only for Gemini / Antigravity-specific quick-start guidance.
- Update `AGENTS.md` only for agent-specific guardrails or mistakes future agents are likely to make.
- If no docs update is needed, mention that explicitly in the PR description.

## Build & Run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
build/linux-service-dashboard
```

The app uses command-line tools from the user's session (`systemctl`, `docker`, `nmcli`, `findmnt`, `sensors`, `lsblk`, `smartctl`) and reports permission/tool errors in the UI.

## Environment — Wayland and X11

The application is a regular Qt Widgets desktop app and should stay compositor-neutral:

- Do not force `QT_QPA_PLATFORM` in code.
- Do not set `QT_WAYLAND_SHELL_INTEGRATION=layer-shell`; this app does not need layer-shell surfaces.
- Keep `QGuiApplication::setDesktopFileName("io.github.Adiker.LinuxServiceDashboard")` before showing windows. KDE Plasma Wayland uses this app id to match the `.desktop` file and taskbar icon.
- Keep the tray and window icon sourced from `serviceDashboardIcon()` so taskbar, window, and tray stay aligned.
- The installable desktop file is `resources/io.github.Adiker.LinuxServiceDashboard.desktop`, and the installed icon name must remain `io.github.Adiker.LinuxServiceDashboard`.
- When checking platform behavior locally, run at least one normal desktop-session smoke test on Wayland and one with `QT_QPA_PLATFORM=xcb` on X11/XWayland if the display server is available.

## UI and Themes

- Theme preference is stored in QSettings key `theme/preference`.
- Supported values: `System`, `Light`, `Dark`, `OLED`.
- Keep the stylesheet centralized in `MainWindow::applyTheme()` unless a widget has a real local rendering need.
- Avoid one-off palette changes inside pages; they can break Light/Dark/OLED contrast.
- Overview cards and VPN panel use object name `card`; page titles use `pageTitle`.
- Tables should remain single-row selection, sortable where useful, and visually governed by the central stylesheet.

## Provider Boundaries

- Providers in `src/services/` should own shell command execution and parsing.
- UI pages should not shell out directly; call provider methods and handle provider signals.
- The app intentionally does not ask for sudo passwords. Permission errors must be surfaced as status text/dialog details.
- SMART checks are manual from the disks page; avoid frequent automatic SMART polling.
- Destructive actions (`systemctl stop/restart`, Docker stop/restart, unmount) must keep explicit confirmation dialogs.

## Icon / QRC

The icon is available both as an installed hicolor theme icon and as embedded PNG fallbacks through `resources/resources.qrc`.

- Runtime path: `QIcon::fromTheme("io.github.Adiker.LinuxServiceDashboard")` first, then embedded PNG fallbacks.
- Packaging installs PNG sizes under `share/icons/hicolor/*/apps/io.github.Adiker.LinuxServiceDashboard.png` and the source SVG under `share/icons/hicolor/scalable/apps/`.
- Do not add post-build icon copy commands; the runtime icon is already embedded in the binary.

## Tests

There is no dedicated unit-test target yet. For code changes, run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

For desktop integration changes, also smoke-test startup with available Qt platform plugins, for example:

```bash
QT_QPA_PLATFORM=offscreen timeout 3s build/linux-service-dashboard
QT_QPA_PLATFORM=minimal timeout 3s build/linux-service-dashboard
```
