#pragma once

#include "core/RefreshScheduler.h"
#include "core/SystemCenter.h"
#include "widgets/DisksPage.h"
#include "widgets/DockerPage.h"
#include "widgets/MountsPage.h"
#include "widgets/OverviewPage.h"
#include "widgets/SensorsPage.h"
#include "widgets/SettingsPage.h"
#include "widgets/SystemdPage.h"
#include "widgets/VpnPage.h"

#include <QHash>
#include <QMainWindow>

class QTreeWidget;
class QLineEdit;
class QTimer;
class QStackedWidget;
class QSystemTrayIcon;

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(QWidget* parent = nullptr);

  public slots:
    void refreshAll();

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    void buildUi();
    void buildTray();
    void applyTheme();
    void reloadSettings();
    void applyModuleVisibility();

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

    QTreeWidget* m_sidebar = nullptr;
    QStackedWidget* m_stack = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    RefreshScheduler m_scheduler;

    OverviewPage* m_overview = nullptr;
    SystemdPage* m_systemd = nullptr;
    DockerPage* m_docker = nullptr;
    VpnPage* m_vpn = nullptr;
    MountsPage* m_mounts = nullptr;
    SensorsPage* m_sensors = nullptr;
    DisksPage* m_disks = nullptr;
    SettingsPage* m_settings = nullptr;
    SystemCenter* m_center = nullptr;
    CenterPage* m_searchPage = nullptr;
    QHash<QString, QWidget*> m_pages;
    QTimer* m_metricTimer = nullptr;
    void navigate(const QString& module, const QString& id = {});
};
