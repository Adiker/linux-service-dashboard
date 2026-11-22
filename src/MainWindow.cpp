#include "MainWindow.h"

#include "AppIcon.h"
#include "core/ModuleSettings.h"
#include "ui/ThemeManager.h"
#include "widgets/DiagnosticsDialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Linux Service Dashboard"));
    setWindowIcon(serviceDashboardIcon());
    resize(1200, 800);
    m_modules = loadModuleSettings();
    m_registry = new ServiceRegistry(this);
    m_alertMonitor = new AlertMonitor(this);
    m_registry->bindAlertMonitor(m_alertMonitor);
    buildUi();
    buildTray();
    restoreWindowGeometry();
    reloadSettings();

    connect(&m_scheduler, &RefreshScheduler::refreshRequested, this, &MainWindow::refreshAll);
    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::saveWindowGeometry);
    connect(m_alertMonitor, &AlertMonitor::summaryChanged, this, [this](const QString& tooltip) {
        if (m_tray) {
            m_tray->setToolTip(tooltip);
        }
    });
    connect(m_alertMonitor, &AlertMonitor::alertRaised, this, [this](const QString& title, const QString& message, AlertSeverity severity) {
        if (!m_tray || !m_tray->isVisible()) {
            return;
        }
        const auto icon = severity == AlertSeverity::Critical
                              ? QSystemTrayIcon::Critical
                              : (severity == AlertSeverity::Warning ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information);
        m_tray->showMessage(title, message, icon, 6000);
    });
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_sidebar = new QListWidget(central);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(230);
    const QStringList items{
        QStringLiteral("Overview"), QStringLiteral("Systemd Services"), QStringLiteral("Docker"),        QStringLiteral("VPN"),
        QStringLiteral("Mounts"),   QStringLiteral("Sensors"),          QStringLiteral("Disks / SMART"), QStringLiteral("Settings"),
    };
    const QStringList icons{
        QStringLiteral("view-dashboard"),
        QStringLiteral("preferences-system-services"),
        QStringLiteral("docker"),
        QStringLiteral("network-vpn"),
        QStringLiteral("drive-harddisk"),
        QStringLiteral("temperature-normal"),
        QStringLiteral("drive-harddisk-solidstate"),
        QStringLiteral("settings-configure"),
    };
    const QStyle::StandardPixmap fallbacks[]{
        QStyle::SP_DesktopIcon, QStyle::SP_FileDialogDetailedView, QStyle::SP_ComputerIcon, QStyle::SP_DriveNetIcon,
        QStyle::SP_DirIcon,     QStyle::SP_MessageBoxInformation,  QStyle::SP_DriveHDIcon,  QStyle::SP_FileDialogContentsView,
    };
    for (int i = 0; i < items.size(); ++i) {
        auto* entry = new QListWidgetItem(ThemeManager::themedIcon(icons.value(i), fallbacks[i]), items.at(i), m_sidebar);
        entry->setSizeHint(QSize(0, 42));
    }
    layout->addWidget(m_sidebar);

    m_stack = new QStackedWidget(central);
    m_stack->setObjectName(QStringLiteral("contentStack"));
    m_overview = new OverviewPage(m_registry, m_stack);
    m_systemd = new SystemdPage(m_registry->systemd(), m_stack);
    m_docker = new DockerPage(m_registry->docker(), m_stack);
    m_vpn = new VpnPage(m_registry->network(), m_stack);
    m_mounts = new MountsPage(m_registry->mounts(), m_stack);
    m_sensors = new SensorsPage(m_registry->sensors(), m_stack);
    m_disks = new DisksPage(m_registry->smart(), m_stack);
    m_settings = new SettingsPage(m_stack);

    for (QWidget* page : {static_cast<QWidget*>(m_overview), static_cast<QWidget*>(m_systemd), static_cast<QWidget*>(m_docker),
                          static_cast<QWidget*>(m_vpn), static_cast<QWidget*>(m_mounts), static_cast<QWidget*>(m_sensors),
                          static_cast<QWidget*>(m_disks), static_cast<QWidget*>(m_settings)}) {
        m_stack->addWidget(page);
    }
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);

    connect(m_sidebar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_overview, &OverviewPage::refreshRequested, this, &MainWindow::refreshAll);
    connect(m_overview, &OverviewPage::navigateRequested, this, [this](int pageIndex) {
        if (QListWidgetItem* item = m_sidebar->item(pageIndex); item && !item->isHidden()) {
            m_sidebar->setCurrentRow(pageIndex);
        }
    });
    connect(m_settings, &SettingsPage::settingsChanged, this, &MainWindow::reloadSettings);
    connect(m_settings, &SettingsPage::settingsChanged, m_systemd, &SystemdPage::reloadGroupSelector);
    m_sidebar->setCurrentRow(0);

    m_diagnosticsButton = new QPushButton(QStringLiteral("Diagnostics (0)"), this);
    m_diagnosticsButton->setObjectName(QStringLiteral("statusBarButton"));
    m_diagnosticsButton->setFlat(true);
    statusBar()->addPermanentWidget(m_diagnosticsButton);
    connect(m_diagnosticsButton, &QPushButton::clicked, this, &MainWindow::openDiagnostics);
    connect(m_registry, &ServiceRegistry::failedCommandRecorded, this, &MainWindow::updateDiagnosticsBadge);
    connect(m_registry, &ServiceRegistry::failedCommandsCleared, this, &MainWindow::updateDiagnosticsBadge);
    updateDiagnosticsBadge();
}

void MainWindow::buildTray() {
    m_tray = new QSystemTrayIcon(serviceDashboardIcon(), this);
    auto* menu = new QMenu(this);
    auto* showAction = menu->addAction(QStringLiteral("Show Dashboard"));
    auto* refreshAction = menu->addAction(QStringLiteral("Refresh Now"));
    auto* diagnosticsAction = menu->addAction(QStringLiteral("Diagnostics..."));
    menu->addSeparator();
    auto* quitAction = menu->addAction(QStringLiteral("Quit"));
    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshAll);
    connect(diagnosticsAction, &QAction::triggered, this, &MainWindow::openDiagnostics);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            showNormal();
            raise();
            activateWindow();
        }
    });
    m_tray->setContextMenu(menu);
    m_tray->setToolTip(QStringLiteral("Linux Service Dashboard"));
    m_tray->show();
}

void MainWindow::reloadSettings() {
    QSettings settings;
    m_scheduler.setIntervalSeconds(settings.value(QStringLiteral("refresh/intervalSeconds"), 30).toInt());
    m_scheduler.start();
    m_modules = loadModuleSettings();
    m_closeToTray = settings.value(QStringLiteral("window/closeToTray"), true).toBool();
    if (m_alertMonitor) {
        m_alertMonitor->setEnabled(settings.value(QStringLiteral("notifications/enabled"), true).toBool());
    }
    applyModuleVisibility();
    applyTheme();
    m_disks->reloadSmartSchedule();
    refreshAll();
}

void MainWindow::applyModuleVisibility() {
    QListWidgetItem* items[] = {
        m_sidebar->item(PageOverview), m_sidebar->item(PageSystemd), m_sidebar->item(PageDocker), m_sidebar->item(PageVpn),
        m_sidebar->item(PageMounts),   m_sidebar->item(PageSensors), m_sidebar->item(PageDisks),  m_sidebar->item(PageSettings),
    };
    const bool enabled[] = {true, m_modules.systemd, m_modules.docker, m_modules.vpn, m_modules.mounts, m_modules.sensors, m_modules.smart,
                            true};
    for (int i = 0; i < 8; ++i) {
        if (items[i]) {
            items[i]->setHidden(!enabled[i]);
        }
    }
    if (m_sidebar->currentItem() && m_sidebar->currentItem()->isHidden()) {
        m_sidebar->setCurrentRow(PageOverview);
    }
}

void MainWindow::applyTheme() {
    QSettings settings;
    const QString preference = settings.value(QStringLiteral("theme/preference"), QStringLiteral("System")).toString();
    ThemeManager::applyToApplication(preference, palette());
    for (QWidget* widget : findChildren<QWidget*>()) {
        widget->update();
    }
}

void MainWindow::refreshAll() {
    m_overview->refresh(m_modules);
    if (m_modules.systemd) {
        m_systemd->refresh();
    }
    if (m_modules.docker) {
        m_docker->refresh();
    }
    if (m_modules.vpn) {
        m_vpn->refresh();
    }
    if (m_modules.mounts) {
        m_mounts->refresh();
    }
    if (m_modules.sensors) {
        m_sensors->refresh();
    }
    if (m_modules.smart) {
        m_disks->refresh();
    }
}

void MainWindow::openDiagnostics() {
    auto* dialog = new DiagnosticsDialog(m_registry, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::updateDiagnosticsBadge() {
    if (!m_diagnosticsButton || !m_registry) {
        return;
    }
    const int count = m_registry->failedCommands().size();
    m_diagnosticsButton->setText(QStringLiteral("Diagnostics (%1)").arg(count));
}

bool MainWindow::shouldStartMinimized() const {
    QSettings settings;
    return settings.value(QStringLiteral("window/startMinimizedToTray"), false).toBool() && QSystemTrayIcon::isSystemTrayAvailable() &&
           m_tray && m_tray->isVisible();
}

void MainWindow::saveWindowGeometry() {
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
}

void MainWindow::restoreWindowGeometry() {
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveWindowGeometry();
    if (m_closeToTray && m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
    qApp->quit();
}
