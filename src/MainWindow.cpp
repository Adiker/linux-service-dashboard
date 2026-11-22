#include "MainWindow.h"

#include "AppIcon.h"
#include "core/ModuleSettings.h"
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
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Linux Service Dashboard"));
    setWindowIcon(serviceDashboardIcon());
    resize(1200, 800);
    m_center = new SystemCenter(this);
    buildUi();
    buildTray();
    reloadSettings();

    connect(&m_scheduler, &RefreshScheduler::refreshRequested, this, &MainWindow::refreshAll);
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
    m_sidebar->setMinimumWidth(210);
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
        if (!groups.contains(module.group))
            groups[module.group] = new QTreeWidgetItem(m_sidebar, {module.group});
        auto* entry = new QTreeWidgetItem(groups[module.group], {module.title});
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
    m_sidebar->expandAll();
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
    m_stack->setCurrentWidget(m_pages[module]);
    if (auto* page = qobject_cast<CenterPage*>(m_pages[module])) {
        page->render();
        if (!id.isEmpty())
            page->selectRecord(id);
    }
    if (module != "overview" && module != "settings")
        m_center->refresh(module);
}

void MainWindow::buildTray() {
    m_tray = new QSystemTrayIcon(serviceDashboardIcon(), this);
    auto* menu = new QMenu(this);
    auto* showAction = menu->addAction(QStringLiteral("Show Dashboard"));
    auto* refreshAction = menu->addAction(QStringLiteral("Refresh Now"));
    auto* aboutAction = menu->addAction(tr("About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About Linux Service Dashboard"),
                           tr("Linux Service Dashboard %1\nLocal system center, built with Qt %2.")
                               .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion())));
    });
    menu->addSeparator();
    auto* quitAction = menu->addAction(QStringLiteral("Quit"));
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
    m_tray->show();
}

void MainWindow::reloadSettings() {
    QSettings settings;
    m_scheduler.setIntervalSeconds(settings.value(QStringLiteral("refresh/intervalSeconds"), 30).toInt());
    m_scheduler.start();
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
    const bool systemDark = palette().color(QPalette::Window).lightness() < 128;
    const bool oled = preference == QStringLiteral("OLED");
    const bool dark = oled || preference == QStringLiteral("Dark") || (preference == QStringLiteral("System") && systemDark);

    const QString window = oled ? QStringLiteral("#000000") : (dark ? QStringLiteral("#161a20") : QStringLiteral("#f6f7f9"));
    const QString sidebar = oled ? QStringLiteral("#000000") : (dark ? QStringLiteral("#11161d") : QStringLiteral("#eef1f4"));
    const QString surface = oled ? QStringLiteral("#000000") : (dark ? QStringLiteral("#20262e") : QStringLiteral("#ffffff"));
    const QString surfaceAlt = oled ? QStringLiteral("#080808") : (dark ? QStringLiteral("#29313a") : QStringLiteral("#f1f4f7"));
    const QString border = oled ? QStringLiteral("#2a2a2a") : (dark ? QStringLiteral("#38424d") : QStringLiteral("#d9dee5"));
    const QString text = oled ? QStringLiteral("#f3f5f7") : (dark ? QStringLiteral("#eef2f5") : QStringLiteral("#202832"));
    const QString muted = oled ? QStringLiteral("#a7adb4") : (dark ? QStringLiteral("#aeb8c2") : QStringLiteral("#5f6b78"));
    const QString accent = oled ? QStringLiteral("#32d296") : (dark ? QStringLiteral("#5db6a3") : QStringLiteral("#2d7f86"));
    const QString accentSoft = oled ? QStringLiteral("#113428") : (dark ? QStringLiteral("#243f3f") : QStringLiteral("#dcefeb"));
    qApp->setStyleSheet(QStringLiteral(R"(
        QWidget {
            font-size: 10.5pt;
        }
        QMainWindow, QDialog, QMessageBox, QWidget#appRoot, QStackedWidget#contentStack {
            background: %1;
            color: %6;
        }
        QLabel, QCheckBox, QRadioButton {
            background: transparent;
            color: %6;
        }
        QTreeWidget#sidebar {
            background: %2;
            border: 0;
            padding: 16px 10px;
        }
        QTreeWidget#sidebar::item {
            border-radius: 6px;
            padding: 10px 12px;
            margin: 3px 0;
            color: %7;
            border: 1px solid transparent;
        }
        QTreeWidget#sidebar::item:hover {
            background: %4;
            color: %6;
        }
        QTreeWidget#sidebar::item:selected {
            background: %9;
            color: %6;
            border-color: %8;
            font-weight: 700;
        }
        QLabel#pageTitle {
            font-size: 21pt;
            font-weight: 700;
            padding: 14px 4px 12px 4px;
        }
        QLabel#sectionTitle {
            color: %7;
            font-weight: 700;
            padding: 2px 4px 0 4px;
        }
        QWidget#card, QFrame#settingsSection {
            background: %3;
            border: 1px solid %5;
            border-radius: 8px;
            padding: 14px;
        }
        QLabel#cardValue {
            font-size: 18pt;
            font-weight: 700;
        }
        QTableView, QTreeView {
            background: %3;
            alternate-background-color: %4;
            gridline-color: %5;
            selection-background-color: %9;
            selection-color: %6;
            border: 1px solid %5;
            border-radius: 6px;
            outline: 0;
        }
        QHeaderView::section {
            background: %4;
            color: %6;
            border: 0;
            border-bottom: 1px solid %5;
            padding: 8px;
            font-weight: 700;
        }
        QTableCornerButton::section {
            background: %4;
            border: 0;
            border-bottom: 1px solid %5;
            border-right: 1px solid %5;
        }
        QTableView::item {
            padding: 5px;
        }
        QTableView::item:selected {
            background: %9;
            color: %6;
        }
        QPushButton, QLineEdit, QSpinBox, QComboBox, QPlainTextEdit, QMenu {
            background: %3;
            color: %6;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 7px 9px;
        }
        QLineEdit, QSpinBox, QComboBox, QPlainTextEdit {
            selection-background-color: %8;
            selection-color: %1;
        }
        QPushButton:hover {
            background: %4;
            border-color: %8;
        }
        QPushButton:pressed {
            background: %9;
        }
        QPushButton:disabled, QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
            color: %7;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus {
            border-color: %8;
        }
        QCheckBox {
            spacing: 8px;
            padding: 4px 2px;
            font-weight: 400;
        }
        QCheckBox:hover {
            color: %6;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid %5;
            border-radius: 4px;
            background: %3;
        }
        QCheckBox::indicator:hover {
            border-color: %8;
        }
        QCheckBox::indicator:checked {
            background: %9;
            border-color: %8;
            image: url(:/qt-project.org/styles/commonstyle/images/standardbutton-apply-16.png);
        }
        QComboBox::drop-down {
            border: 0;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background: %3;
            color: %6;
            selection-background-color: %9;
            selection-color: %6;
            border: 1px solid %5;
            outline: 0;
        }
        QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
            background: %4;
            border: 0;
            width: 18px;
        }
        QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover {
            background: %9;
        }
        QScrollBar:vertical {
            background: %1;
            width: 12px;
            margin: 0;
        }
        QScrollBar:horizontal {
            background: %1;
            height: 12px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %5;
            border-radius: 6px;
            min-height: 28px;
        }
        QScrollBar::handle:horizontal {
            background: %5;
            border-radius: 6px;
            min-width: 28px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            height: 0;
            width: 0;
        }
        QMenu::item {
            padding: 7px 22px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background: %9;
        }
        QStatusBar, QStatusBar QLabel {
            background: %1;
            color: %7;
        }
        QToolTip {
            background: %3;
            color: %6;
            border: 1px solid %5;
        }
    )")
                            .arg(window)
                            .arg(sidebar)
                            .arg(surface)
                            .arg(surfaceAlt)
                            .arg(border)
                            .arg(text)
                            .arg(muted)
                            .arg(accent)
                            .arg(accentSoft));
}

void MainWindow::refreshAll() {
    m_center->refreshScheduled();
    m_overview->render();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}
