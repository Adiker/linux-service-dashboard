#pragma once

#include "core/RefreshScheduler.h"
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
class QStackedWidget;
class QSystemTrayIcon;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

public slots:
    void refreshAll();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void buildTray();
    void applyTheme();
    QIcon appIcon() const;
    void reloadSettings();

    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    RefreshScheduler m_scheduler;

    OverviewPage *m_overview = nullptr;
    SystemdPage *m_systemd = nullptr;
    DockerPage *m_docker = nullptr;
    VpnPage *m_vpn = nullptr;
    MountsPage *m_mounts = nullptr;
    SensorsPage *m_sensors = nullptr;
    DisksPage *m_disks = nullptr;
    SettingsPage *m_settings = nullptr;
};
