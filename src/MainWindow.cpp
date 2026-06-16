#include "MainWindow.h"

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
    setWindowIcon(appIcon());
    resize(1200, 800);
    buildUi();
    buildTray();
    reloadSettings();

    connect(&m_scheduler, &RefreshScheduler::refreshRequested, this, &MainWindow::refreshAll);
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
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
    m_tray = new QSystemTrayIcon(appIcon(), this);
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

    const QString window = oled ? QStringLiteral("#000000") : (dark ? QStringLiteral("#171b21") : QStringLiteral("#f5f7fa"));
    const QString sidebar = oled ? QStringLiteral("#050505") : (dark ? QStringLiteral("#10141a") : QStringLiteral("#e9edf2"));
    const QString surface = oled ? QStringLiteral("#080808") : (dark ? QStringLiteral("#222831") : QStringLiteral("#ffffff"));
    const QString surfaceAlt = oled ? QStringLiteral("#101010") : (dark ? QStringLiteral("#29313b") : QStringLiteral("#f0f4f8"));
    const QString border = oled ? QStringLiteral("#242424") : (dark ? QStringLiteral("#3a4552") : QStringLiteral("#d2d9e2"));
    const QString text = oled ? QStringLiteral("#f4f7f8") : (dark ? QStringLiteral("#edf2f7") : QStringLiteral("#18212b"));
    const QString muted = oled ? QStringLiteral("#a5adb5") : (dark ? QStringLiteral("#b6c0cb") : QStringLiteral("#53606e"));
    const QString accent = oled ? QStringLiteral("#00f5a0") : (dark ? QStringLiteral("#44c2a8") : QStringLiteral("#166e7a"));
    const QString accentSoft = oled ? QStringLiteral("#003d2b") : (dark ? QStringLiteral("#214b4a") : QStringLiteral("#d8f0ec"));
    const QString danger = oled ? QStringLiteral("#ff5f7a") : QStringLiteral("#d95667");
    const QString warning = oled ? QStringLiteral("#ffd166") : QStringLiteral("#b98516");

    qApp->setStyleSheet(QStringLiteral(R"(
        QWidget {
            font-size: 10.5pt;
        }
        QMainWindow, QWidget {
            background: %1;
            color: %6;
        }
        QListWidget#sidebar {
            background: %2;
            border: 0;
            padding: 14px 10px;
        }
        QListWidget#sidebar::item {
            border-radius: 6px;
            padding: 10px 12px;
            margin: 3px 0;
            color: %7;
        }
        QListWidget#sidebar::item:hover {
            background: %4;
            color: %6;
        }
        QListWidget#sidebar::item:selected {
            background: %9;
            color: %6;
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
            padding: 12px;
        }
        QGroupBox {
            margin-top: 14px;
            font-weight: 700;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 6px;
            color: %7;
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
        QPushButton, QLineEdit, QSpinBox, QComboBox, QPlainTextEdit, QMenu {
            background: %3;
            color: %6;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 7px 9px;
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
        QComboBox::drop-down {
            border: 0;
            width: 24px;
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
        QToolTip {
            background: %3;
            color: %6;
            border: 1px solid %5;
        }
    )")
        .arg(window, sidebar, surface, surfaceAlt, border, text, muted, accent, accentSoft, danger, warning));
}

QIcon MainWindow::appIcon() const
{
    return QIcon::fromTheme(QStringLiteral("io.github.Adiker.LinuxServiceDashboard"),
                            QIcon(QStringLiteral(":/icons/linux-service-dashboard.svg")));
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
