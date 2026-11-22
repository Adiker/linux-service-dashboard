#pragma once

#include "core/AlertMonitor.h"
#include "core/RefreshScheduler.h"
#include "core/ServiceRegistry.h"
#include "widgets/DisksPage.h"
#include "widgets/DockerPage.h"
#include "widgets/MountsPage.h"
#include "widgets/OverviewPage.h"
#include "widgets/SensorsPage.h"
#include "widgets/SettingsPage.h"
#include "widgets/SystemdPage.h"
#include "widgets/VpnPage.h"

#include <QMainWindow>

class QListWidget;
class QPushButton;
class QStackedWidget;
class QSystemTrayIcon;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    [[nodiscard]] bool shouldStartMinimized() const;

public slots:
    void refreshAll();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void buildTray();
    void applyTheme();
    void reloadSettings();
    void applyModuleVisibility();
    void openDiagnostics();
    void updateDiagnosticsBadge();
    void saveWindowGeometry();
    void restoreWindowGeometry();

    enum PageIndex {
        PageOverview = 0,
        PageSystemd,
        PageDocker,
        PageVpn,
        PageMounts,
        PageSensors,
        PageDisks,
        PageSettings,
    };

    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QPushButton *m_diagnosticsButton = nullptr;
    RefreshScheduler m_scheduler;
    ServiceRegistry *m_registry = nullptr;
    AlertMonitor *m_alertMonitor = nullptr;
    bool m_closeToTray = true;

    OverviewPage *m_overview = nullptr;
    SystemdPage *m_systemd = nullptr;
    DockerPage *m_docker = nullptr;
    VpnPage *m_vpn = nullptr;
    MountsPage *m_mounts = nullptr;
    SensorsPage *m_sensors = nullptr;
    DisksPage *m_disks = nullptr;
    SettingsPage *m_settings = nullptr;
    ModuleSettings m_modules;
};
