#include "MainWindow.h"

#include "AppIcon.h"
#include "core/ModuleSettings.h"
#include "ui/ThemeManager.h"
#include "widgets/LogViewerDialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTableView>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_systemPalette = QApplication::palette();
    setWindowTitle(QStringLiteral("Linux Service Dashboard"));
    setWindowIcon(serviceDashboardIcon());
    const auto available =
        QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->availableGeometry().size() : QSize(1200, 800);
    resize(qMin(1200, int(available.width() * .9)), qMin(800, int(available.height() * .9)));
    setMinimumSize(760, 540);
    m_center = new SystemCenter(this);
    m_alertMonitor = new AlertMonitor(this);
    connect(&m_center->systemd, &SystemdServiceProvider::servicesReady, m_alertMonitor,
            [this](const QVector<ServiceRow>& rows, const QString& error) {
                if (error.isEmpty()) {
                    m_alertMonitor->processSystemdServices(rows);
                }
            });
    connect(&m_center->sensors, &SensorProvider::sensorsReady, m_alertMonitor,
            [this](const QVector<SensorRow>& rows, const QString& error) {
                if (error.isEmpty()) {
                    m_alertMonitor->processSensors(rows);
                }
            });
    connect(&m_center->smart, &SmartProvider::smartReady, m_alertMonitor,
            [this](const QString& path, const DiskRow& smartRow, const QString& error) {
                if (error.isEmpty()) {
                    m_alertMonitor->processSmartResult(path, smartRow);
                }
            });
    connect(&m_center->network, &NetworkProvider::vpnStatusReady, m_alertMonitor, [this](const VpnStatus& status, const QString& error) {
        if (error.isEmpty()) {
            m_alertMonitor->processVpnStatus(status);
        }
    });
    buildUi();
    buildTray();
    restoreWindowGeometry();
    reloadSettings();

    connect(&m_scheduler, &RefreshScheduler::refreshRequested, this, &MainWindow::refreshAll);
    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::saveWindowGeometry);
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
    central->setObjectName("appRoot");
    auto* outer = new QVBoxLayout(central);
    outer->setContentsMargins(8, 8, 8, 8);
    auto* search = new QLineEdit;
    search->setPlaceholderText(tr("Search pages, services, containers and software… (Ctrl+K)"));
    search->setObjectName("globalSearch");
    outer->addWidget(search);
    auto* layout = new QHBoxLayout;
    outer->addLayout(layout, 1);
    m_sidebar = new QTreeWidget;
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setHeaderHidden(true);
    m_sidebar->setIndentation(12);
    m_sidebar->setWordWrap(true);
    m_sidebar->setTextElideMode(Qt::ElideNone);
    m_sidebar->setMinimumWidth(230);
    m_sidebar->setMaximumWidth(300);
    layout->addWidget(m_sidebar);
    m_stack = new QStackedWidget;
    m_stack->setObjectName("contentStack");
    layout->addWidget(m_stack, 1);
    m_overview = new OverviewPage(m_center, m_stack);
    m_systemd = new SystemdPage(m_center->systemd, m_stack);
    m_docker = new DockerPage(m_center, m_stack);
    m_vpn = new VpnPage(m_center, m_stack);
    m_mounts = new MountsPage(m_center->mounts, m_stack);
    m_sensors = new SensorsPage(m_center->sensors, m_stack);
    m_disks = new DisksPage(m_center->smart, m_stack);
    m_settings = new SettingsPage;
    auto* settingsScroll = new QScrollArea;
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setWidget(m_settings);
    m_pages = {{"overview", m_overview}, {"systemd", m_systemd}, {"docker", m_docker}, {"vpn", m_vpn},
               {"mounts", m_mounts},     {"sensors", m_sensors}, {"smart", m_disks},   {"settings", settingsScroll}};
    QHash<QString, QTreeWidgetItem*> groups;
    for (const auto& module : centerModules()) {
        QTreeWidgetItem* entry = nullptr;
        if (module.id == "overview")
            entry = new QTreeWidgetItem(m_sidebar, {module.title});
        else {
            if (!groups.contains(module.group)) {
                auto* group = new QTreeWidgetItem(m_sidebar, {module.group});
                group->setToolTip(0, module.group);
                auto font = group->font(0);
                font.setBold(true);
                group->setFont(0, font);
                groups[module.group] = group;
            }
            entry = new QTreeWidgetItem(groups[module.group], {module.title});
        }
        entry->setToolTip(0, module.title);
        entry->setData(0, Qt::UserRole, module.id);
        if (!m_pages.contains(module.id))
            m_pages[module.id] = new CenterPage(module.id, m_center, m_stack);
        m_stack->addWidget(m_pages[module.id]);
        if (auto* page = qobject_cast<CenterPage*>(m_pages[module.id]))
            connect(page, &CenterPage::navigate, this, &MainWindow::navigate);
    }
    m_searchPage = new CenterPage("search", m_center, m_stack);
    m_stack->addWidget(m_searchPage);
    connect(m_searchPage, &CenterPage::navigate, this, &MainWindow::navigate);
    m_sidebar->collapseAll();
    connect(m_sidebar, &QTreeWidget::currentItemChanged, this, [this](auto* item) {
        if (!item)
            return;
        if (item->data(0, Qt::UserRole).toString().isEmpty()) {
            if (item->childCount())
                m_sidebar->setCurrentItem(item->child(0));
            return;
        }
        navigate(item->data(0, Qt::UserRole).toString());
    });
    auto* debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(200);
    connect(search, &QLineEdit::textChanged, debounce, [debounce]() { debounce->start(); });
    connect(debounce, &QTimer::timeout, this, [this, search]() {
        if (search->text().trimmed().isEmpty()) {
            navigate("overview");
            return;
        }
        if (search->text().trimmed().size() >= 2)
            for (const auto& id : {QString("apps"), QString("packages")})
                if (!m_center->snapshot(id).lastSuccess.isValid())
                    m_center->refresh(id);
        m_searchPage->setSearch(search->text());
        m_stack->setCurrentWidget(m_searchPage);
    });
    auto* shortcut = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(shortcut, &QShortcut::activated, search, qOverload<>(&QLineEdit::setFocus));
    connect(m_settings, &SettingsPage::settingsChanged, this, &MainWindow::reloadSettings);
    connect(m_settings, &SettingsPage::settingsChanged, m_systemd, &SystemdPage::reloadGroupSelector);
    connect(m_center, &SystemCenter::textReady, this, [this](const auto& title, const auto& text) {
        auto* dialog = new LogViewerDialog(title, text, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    m_metricTimer = new QTimer(this);
    m_metricTimer->setInterval(2000);
    connect(m_metricTimer, &QTimer::timeout, this, [this]() {
        if (isVisible() && m_stack->currentWidget() == m_pages.value("metrics"))
            m_center->refresh("metrics");
        if (isVisible() && m_stack->currentWidget() == m_pages.value("docker") && centerModuleEnabled("docker"))
            m_center->inventory.refresh("dockerStats");
    });
    m_metricTimer->start();
    setCentralWidget(central);
    navigate("overview");
}
void MainWindow::navigate(const QString& module, const QString& id) {
    if (!m_pages.contains(module) || !centerModuleEnabled(module))
        return;
    QTreeWidgetItemIterator nav(m_sidebar);
    while (*nav) {
        if ((*nav)->data(0, Qt::UserRole).toString() == module) {
            for (int group = 0; group < m_sidebar->topLevelItemCount(); ++group) {
                auto* item = m_sidebar->topLevelItem(group);
                if (item != (*nav)->parent())
                    item->setExpanded(false);
            }
            const QSignalBlocker block(m_sidebar);
            m_sidebar->setCurrentItem(*nav);
            if ((*nav)->parent())
                (*nav)->parent()->setExpanded(true);
            break;
        }
        ++nav;
    }
    if (module == "systemd" && !id.isEmpty())
        m_systemd->selectRecord(id);
    m_stack->setCurrentWidget(m_pages[module]);
    if (auto* page = qobject_cast<CenterPage*>(m_pages[module])) {
        page->render();
        if (!id.isEmpty())
            page->selectRecord(id);
    }
    if (!id.isEmpty())
        if (auto* table = m_pages[module]->findChild<QTableView*>()) {
            for (int row = 0; row < table->model()->rowCount(); ++row)
                if (table->model()->index(row, 0).data(Qt::UserRole).toString() == id) {
                    const auto index = table->model()->index(row, 0);
                    table->setCurrentIndex(index);
                    table->scrollTo(index);
                    break;
                }
        }
    if (module != "overview" && module != "settings")
        m_center->refresh(module);
}

void MainWindow::buildTray() {
    m_tray = new QSystemTrayIcon(serviceDashboardIcon(), this);
    auto* menu = new QMenu(this);
    auto* showAction = menu->addAction(tr("Show Dashboard"));
    auto* refreshAction = menu->addAction(tr("Refresh Now"));
    auto* aboutAction = menu->addAction(tr("About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About Linux Service Dashboard"),
                           tr("Linux Service Dashboard %1\nLocal system center, built with Qt %2.")
                               .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion())));
    });
    menu->addSeparator();
    auto* quitAction = menu->addAction(tr("Quit"));
    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshAll);
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
    connect(m_center, &SystemCenter::changed, this, [this](const QString&) {
        const int count = m_center->attentionCount();
        m_tray->setToolTip(tr("Linux Service Dashboard · %1 items require attention").arg(count));
        auto pixmap = serviceDashboardIcon().pixmap(32, 32);
        if (count > 0) {
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#b42318"));
            painter.drawEllipse(QRectF(14, 14, 18, 18));
            painter.setPen(Qt::white);
            auto font = painter.font();
            font.setPixelSize(10);
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(QRectF(14, 14, 18, 18), Qt::AlignCenter, count > 9 ? "9+" : QString::number(count));
        }
        m_tray->setIcon(QIcon(pixmap));
        const auto now = QDateTime::currentSecsSinceEpoch();
        if (count > m_lastAttention && now - m_lastNotification >= 60 && QSettings().value("notifications/enabled", false).toBool()) {
            m_lastNotification = now;
            m_tray->showMessage(tr("System needs attention"), tr("%1 items require attention. Open the dashboard for details.").arg(count),
                                QSystemTrayIcon::Warning);
        }
        m_lastAttention = count;
    });
    m_tray->show();
}

void MainWindow::reloadSettings() {
    QSettings settings;
    m_scheduler.setIntervalSeconds(settings.value(QStringLiteral("refresh/intervalSeconds"), 30).toInt());
    m_scheduler.start();
    m_closeToTray = settings.value(QStringLiteral("window/closeToTray"), true).toBool();
    if (m_alertMonitor) {
        m_alertMonitor->setEnabled(settings.value(QStringLiteral("notifications/enabled"), false).toBool());
    }
    applyModuleVisibility();
    applyTheme();
    m_disks->reloadSmartSchedule();
    refreshAll();
}

void MainWindow::applyModuleVisibility() {
    QTreeWidgetItemIterator it(m_sidebar);
    while (*it) {
        const auto id = (*it)->data(0, Qt::UserRole).toString();
        if (!id.isEmpty())
            (*it)->setHidden(!centerModuleEnabled(id));
        ++it;
    }
    for (int i = 0; i < m_sidebar->topLevelItemCount(); ++i) {
        auto* group = m_sidebar->topLevelItem(i);
        if (!group->data(0, Qt::UserRole).toString().isEmpty())
            continue;
        bool any = false;
        for (int j = 0; j < group->childCount(); ++j)
            any = any || !group->child(j)->isHidden();
        group->setHidden(!any);
    }
    for (auto it = m_pages.cbegin(); it != m_pages.cend(); ++it)
        if (it.value() == m_stack->currentWidget() && !centerModuleEnabled(it.key()))
            navigate("overview");
}

void MainWindow::applyTheme() {
    QSettings settings;
    const QString preference = settings.value(QStringLiteral("theme/preference"), QStringLiteral("System")).toString();
    ThemeManager::applyToApplication(preference, m_systemPalette);
    for (QWidget* widget : findChildren<QWidget*>()) {
        widget->update();
    }
}

void MainWindow::refreshAll() {
    m_center->refreshScheduled();
    m_overview->render();
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
