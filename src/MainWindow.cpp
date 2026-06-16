#include "MainWindow.h"

#include "AppIcon.h"

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
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QSystemTrayIcon>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Linux Service Dashboard"));
    setWindowIcon(serviceDashboardIcon());
    resize(1200, 800);
    buildUi();
    buildTray();
    reloadSettings();

    connect(&m_scheduler, &RefreshScheduler::refreshRequested, this, &MainWindow::refreshAll);
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_sidebar = new QListWidget(central);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(230);
    const QStringList items{
        QStringLiteral("Overview"),
        QStringLiteral("Systemd Services"),
        QStringLiteral("Docker"),
        QStringLiteral("VPN"),
        QStringLiteral("Mounts"),
        QStringLiteral("Sensors"),
        QStringLiteral("Disks / SMART"),
        QStringLiteral("Settings"),
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
    for (int i = 0; i < items.size(); ++i) {
        auto *entry = new QListWidgetItem(QIcon::fromTheme(icons.value(i)), items.at(i), m_sidebar);
        entry->setSizeHint(QSize(0, 42));
    }
    layout->addWidget(m_sidebar);

    m_stack = new QStackedWidget(central);
    m_stack->setObjectName(QStringLiteral("contentStack"));
    m_overview = new OverviewPage(m_stack);
    m_systemd = new SystemdPage(m_stack);
    m_docker = new DockerPage(m_stack);
    m_vpn = new VpnPage(m_stack);
    m_mounts = new MountsPage(m_stack);
    m_sensors = new SensorsPage(m_stack);
    m_disks = new DisksPage(m_stack);
    m_settings = new SettingsPage(m_stack);

    for (QWidget *page : {static_cast<QWidget *>(m_overview), static_cast<QWidget *>(m_systemd), static_cast<QWidget *>(m_docker),
                          static_cast<QWidget *>(m_vpn), static_cast<QWidget *>(m_mounts), static_cast<QWidget *>(m_sensors),
                          static_cast<QWidget *>(m_disks), static_cast<QWidget *>(m_settings)}) {
        m_stack->addWidget(page);
    }
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);

    connect(m_sidebar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_settings, &SettingsPage::settingsChanged, this, &MainWindow::reloadSettings);
    m_sidebar->setCurrentRow(0);
}

void MainWindow::buildTray()
{
    m_tray = new QSystemTrayIcon(serviceDashboardIcon(), this);
    auto *menu = new QMenu(this);
    auto *showAction = menu->addAction(QStringLiteral("Show Dashboard"));
    auto *refreshAction = menu->addAction(QStringLiteral("Refresh Now"));
    menu->addSeparator();
    auto *quitAction = menu->addAction(QStringLiteral("Quit"));
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

void MainWindow::reloadSettings()
{
    QSettings settings;
    m_scheduler.setIntervalSeconds(settings.value(QStringLiteral("refresh/intervalSeconds"), 30).toInt());
    m_scheduler.start();
    applyTheme();
    refreshAll();
}

void MainWindow::applyTheme()
{
    QSettings settings;
    const QString preference = settings.value(QStringLiteral("theme/preference"), QStringLiteral("System")).toString();
    const bool systemDark = palette().color(QPalette::Window).lightness() < 128;
    const bool oled = preference == QStringLiteral("OLED");
    const bool dark = oled || preference == QStringLiteral("Dark") || (preference == QStringLiteral("System") && systemDark);

    const QString window = oled ? QStringLiteral("#000000") : (dark ? QStringLiteral("#161a20") : QStringLiteral("#f6f7f9"));
    const QString sidebar = oled ? QStringLiteral("#040404") : (dark ? QStringLiteral("#11161d") : QStringLiteral("#eef1f4"));
    const QString surface = oled ? QStringLiteral("#090909") : (dark ? QStringLiteral("#20262e") : QStringLiteral("#ffffff"));
    const QString surfaceAlt = oled ? QStringLiteral("#121212") : (dark ? QStringLiteral("#29313a") : QStringLiteral("#f1f4f7"));
    const QString border = oled ? QStringLiteral("#2a2a2a") : (dark ? QStringLiteral("#38424d") : QStringLiteral("#d9dee5"));
    const QString text = oled ? QStringLiteral("#f3f5f7") : (dark ? QStringLiteral("#eef2f5") : QStringLiteral("#202832"));
    const QString muted = oled ? QStringLiteral("#a7adb4") : (dark ? QStringLiteral("#aeb8c2") : QStringLiteral("#5f6b78"));
    const QString accent = oled ? QStringLiteral("#32d296") : (dark ? QStringLiteral("#5db6a3") : QStringLiteral("#2d7f86"));
    const QString accentSoft = oled ? QStringLiteral("#113428") : (dark ? QStringLiteral("#243f3f") : QStringLiteral("#dcefeb"));
    qApp->setStyleSheet(QStringLiteral(R"(
        QWidget {
            font-size: 10.5pt;
        }
        QMainWindow, QWidget#appRoot, QStackedWidget#contentStack {
            background: %1;
            color: %6;
        }
        QLabel, QCheckBox, QRadioButton {
            background: transparent;
            color: %6;
        }
        QListWidget#sidebar {
            background: %2;
            border: 0;
            padding: 16px 10px;
        }
        QListWidget#sidebar::item {
            border-radius: 6px;
            padding: 10px 12px;
            margin: 3px 0;
            color: %7;
            border: 1px solid transparent;
        }
        QListWidget#sidebar::item:hover {
            background: %4;
            color: %6;
        }
        QListWidget#sidebar::item:selected {
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
        QWidget#card, QGroupBox {
            background: %3;
            border: 1px solid %5;
            border-radius: 8px;
            padding: 14px;
        }
        QGroupBox {
            margin-top: 14px;
            font-weight: 700;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 7px;
            color: %7;
            background: %1;
        }
        QLabel#cardValue {
            font-size: 18pt;
            font-weight: 700;
        }
        QTableView {
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
        QScrollBar::handle:vertical {
            background: %5;
            border-radius: 6px;
            min-height: 28px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
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

void MainWindow::refreshAll()
{
    m_overview->refresh();
    m_systemd->refresh();
    m_docker->refresh();
    m_vpn->refresh();
    m_mounts->refresh();
    m_sensors->refresh();
    m_disks->refresh();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}
