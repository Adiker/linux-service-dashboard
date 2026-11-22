#include "OverviewPage.h"

#include "../core/ServiceRegistry.h"
#include "../ui/ThemeManager.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

class OverviewCard : public QFrame {
    Q_OBJECT

public:
    OverviewCard(const QString &title, const QIcon &icon, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("card"));
        setProperty("clickable", true);
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(138);

        auto *layout = new QVBoxLayout(this);
        layout->setSpacing(8);

        auto *header = new QHBoxLayout;
        header->setSpacing(8);
        auto *iconLabel = new QLabel(this);
        iconLabel->setFixedSize(20, 20);
        iconLabel->setPixmap(icon.pixmap(20, 20));
        auto *titleLabel = new QLabel(title, this);
        titleLabel->setObjectName(QStringLiteral("cardHeader"));
        m_dot = new QLabel(this);
        m_dot->setObjectName(QStringLiteral("statusDot"));
        m_dot->setProperty("state", QStringLiteral("muted"));
        header->addWidget(iconLabel);
        header->addWidget(titleLabel);
        header->addStretch();
        header->addWidget(m_dot);
        layout->addLayout(header);

        m_value = new QLabel(QStringLiteral("Loading..."), this);
        m_value->setObjectName(QStringLiteral("cardValue"));
        m_value->setWordWrap(true);
        layout->addWidget(m_value);

        m_subtitle = new QLabel(this);
        m_subtitle->setObjectName(QStringLiteral("cardSubtitle"));
        m_subtitle->setWordWrap(true);
        layout->addWidget(m_subtitle);
        layout->addStretch();
    }

    void setSummary(const QString &value, const QString &subtitle, const QString &dotState)
    {
        m_value->setText(value);
        m_subtitle->setText(subtitle);
        m_dot->setProperty("state", dotState);
        m_dot->style()->unpolish(m_dot);
        m_dot->style()->polish(m_dot);
    }

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            emit clicked();
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    QLabel *m_dot = nullptr;
    QLabel *m_value = nullptr;
    QLabel *m_subtitle = nullptr;
};

OverviewPage::OverviewPage(ServiceRegistry *registry, QWidget *parent)
    : QWidget(parent)
    , m_registry(registry)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Overview"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *refreshButton = new QPushButton(
        ThemeManager::themedIcon(QStringLiteral("view-refresh"), QStyle::SP_BrowserReload),
        QStringLiteral("Refresh"),
        this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    auto *grid = new QGridLayout;
    grid->setSpacing(16);
    m_dockerCard = addCard(grid, 0, 0, QStringLiteral("Docker"),
                           ThemeManager::themedIcon(QStringLiteral("docker"), QStyle::SP_ComputerIcon),
                           TargetDocker);
    m_systemdCard = addCard(grid, 0, 1, QStringLiteral("Systemd"),
                            ThemeManager::themedIcon(QStringLiteral("preferences-system-services"), QStyle::SP_FileDialogDetailedView),
                            TargetSystemd);
    m_vpnCard = addCard(grid, 0, 2, QStringLiteral("VPN"),
                        ThemeManager::themedIcon(QStringLiteral("network-vpn"), QStyle::SP_DriveNetIcon),
                        TargetVpn);
    m_mountsCard = addCard(grid, 1, 0, QStringLiteral("Mounts"),
                           ThemeManager::themedIcon(QStringLiteral("drive-harddisk"), QStyle::SP_DirIcon),
                           TargetMounts);
    m_sensorsCard = addCard(grid, 1, 1, QStringLiteral("Sensors"),
                            ThemeManager::themedIcon(QStringLiteral("temperature-normal"), QStyle::SP_MessageBoxInformation),
                            TargetSensors);
    m_disksCard = addCard(grid, 1, 2, QStringLiteral("Disks / SMART"),
                          ThemeManager::themedIcon(QStringLiteral("drive-harddisk-solidstate"), QStyle::SP_DriveHDIcon),
                          TargetDisks);
    layout->addLayout(grid);
    layout->addStretch();

    connect(refreshButton, &QPushButton::clicked, this, &OverviewPage::refreshRequested);

    if (!m_registry) {
        return;
    }

    connect(m_registry->docker(), &DockerProvider::containersReady, this, [this](const QVector<DockerContainerRow> &rows, const QString &error) {
        if (!error.isEmpty()) {
            m_dockerCard->setSummary(QStringLiteral("Unavailable"), error, QStringLiteral("warn"));
            return;
        }
        int running = 0;
        for (const auto &row : rows) {
            if (row.state == QStringLiteral("running")) {
                ++running;
            }
        }
        const QString tone = rows.isEmpty() ? QStringLiteral("muted") : (running > 0 ? QStringLiteral("ok") : QStringLiteral("warn"));
        m_dockerCard->setSummary(QStringLiteral("%1 / %2 running").arg(running).arg(rows.size()),
                                 QStringLiteral("Click to manage containers"),
                                 tone);
    });

    connect(m_registry->systemd(), &SystemdServiceProvider::failedCountReady, this, [this](int count, const QString &error) {
        if (!error.isEmpty()) {
            m_systemdCard->setSummary(QStringLiteral("Unknown"), error, QStringLiteral("warn"));
            return;
        }
        m_systemdCard->setSummary(QStringLiteral("%1 failed").arg(count),
                                  count == 0 ? QStringLiteral("All systemd services healthy") : QStringLiteral("Click to inspect failed units"),
                                  count == 0 ? QStringLiteral("ok") : QStringLiteral("error"));
    });

    connect(m_registry->network(), &NetworkProvider::activeVpnsReady, this, [this](const QVector<VpnStatus> &activeVpns, const QString &error) {
        if (!error.isEmpty()) {
            m_vpnCard->setSummary(QStringLiteral("Unknown"), error, QStringLiteral("warn"));
            return;
        }
        if (activeVpns.isEmpty()) {
            m_vpnCard->setSummary(QStringLiteral("Disconnected"), QStringLiteral("No active VPN tunnels"), QStringLiteral("muted"));
            return;
        }
        QStringList names;
        for (const VpnStatus &vpn : activeVpns) {
            names.append(vpn.connectionName);
        }
        const QString titleText = activeVpns.size() == 1
            ? QStringLiteral("Connected")
            : QStringLiteral("%1 tunnels active").arg(activeVpns.size());
        m_vpnCard->setSummary(titleText, names.join(QStringLiteral(", ")), QStringLiteral("ok"));
    });

    connect(m_registry->mounts(), &MountProvider::mountsReady, this, [this](const QVector<MountRow> &rows, const QString &error) {
        if (!error.isEmpty()) {
            m_mountsCard->setSummary(QStringLiteral("Unknown"), error, QStringLiteral("warn"));
            return;
        }
        int mounted = 0;
        int configured = 0;
        for (const MountRow &row : rows) {
            if (row.status == QStringLiteral("Mounted")) {
                ++mounted;
            } else {
                ++configured;
            }
        }
        m_mountsCard->setSummary(QStringLiteral("%1 mounted").arg(mounted),
                                 configured > 0 ? QStringLiteral("%1 configured (unmounted)").arg(configured) : QStringLiteral("Network shares"),
                                 mounted > 0 ? QStringLiteral("ok") : QStringLiteral("muted"));
    });

    connect(m_registry->sensors(), &SensorProvider::sensorsReady, this, [this](const QVector<SensorRow> &rows, const QString &error) {
        if (!error.isEmpty()) {
            m_sensorsCard->setSummary(QStringLiteral("Unknown"), error, QStringLiteral("warn"));
            return;
        }
        if (rows.isEmpty()) {
            m_sensorsCard->setSummary(QStringLiteral("No readings"), QStringLiteral("Run sensors-detect if needed"), QStringLiteral("muted"));
            return;
        }
        double maxValue = -1000.0;
        QString maxLabel;
        bool hasCritical = false;
        bool hasWarning = false;
        for (const auto &row : rows) {
            if (row.status == QStringLiteral("Critical")) hasCritical = true;
            else if (row.status == QStringLiteral("Warning")) hasWarning = true;
            if (row.current > maxValue) {
                maxValue = row.current;
                maxLabel = row.label;
            }
        }
        const QString tone = hasCritical ? QStringLiteral("error") : (hasWarning ? QStringLiteral("warn") : QStringLiteral("ok"));
        m_sensorsCard->setSummary(QStringLiteral("%1 C").arg(maxValue, 0, 'f', 1),
                                  QStringLiteral("Peak: %1 (%2 readings)").arg(maxLabel).arg(rows.size()),
                                  tone);
    });

    connect(m_registry->smart(), &SmartProvider::disksReady, this, [this](const QVector<DiskRow> &rows, const QString &error) {
        m_diskCount = rows.size();
        m_disksError = error;
        updateDisksCard();
    });

    connect(m_registry->smart(), &SmartProvider::smartReady, this, [this](const QString &path, const DiskRow &smartRow, const QString &) {
        m_diskSmartHealth.insert(path, smartRow.smartHealth);
        m_lastSmartCheck = smartRow.lastCheck;
        updateDisksCard();
    });
}

OverviewCard *OverviewPage::addCard(QGridLayout *grid,
                                    int row,
                                    int column,
                                    const QString &title,
                                    const QIcon &icon,
                                    int targetPage)
{
    auto *card = new OverviewCard(title, icon, this);
    connect(card, &OverviewCard::clicked, this, [this, targetPage]() {
        emit navigateRequested(targetPage);
    });
    grid->addWidget(card, row, column);
    return card;
}

void OverviewPage::updateDisksCard()
{
    if (!m_disksError.isEmpty()) {
        m_disksCard->setSummary(QStringLiteral("Unknown"), m_disksError, QStringLiteral("warn"));
        return;
    }
    int passed = 0;
    int failed = 0;
    for (auto it = m_diskSmartHealth.constBegin(); it != m_diskSmartHealth.constEnd(); ++it) {
        const QString lower = it.value().toLower();
        if (lower.contains(QStringLiteral("passed")) || lower == QStringLiteral("ok")) {
            ++passed;
        } else if (lower.contains(QStringLiteral("fail")) || lower.contains(QStringLiteral("error")) || lower.contains(QStringLiteral("warn"))) {
            ++failed;
        }
    }

    QString subtitle;
    QString tone = QStringLiteral("muted");
    if (failed > 0) {
        subtitle = QStringLiteral("%1 warning/failed — checked %2").arg(failed).arg(m_lastSmartCheck);
        tone = QStringLiteral("error");
    } else if (passed > 0) {
        subtitle = QStringLiteral("%1/%2 SMART PASSED — %3").arg(passed).arg(m_diskCount).arg(m_lastSmartCheck);
        tone = QStringLiteral("ok");
    } else {
        subtitle = QStringLiteral("SMART check manual — click to inspect");
    }
    m_disksCard->setSummary(QStringLiteral("%1 physical disks").arg(m_diskCount), subtitle, tone);
}

void OverviewPage::setModuleVisibility(const ModuleSettings &modules)
{
    m_dockerCard->setVisible(modules.docker);
    m_systemdCard->setVisible(modules.systemd);
    m_vpnCard->setVisible(modules.vpn);
    m_mountsCard->setVisible(modules.mounts);
    m_sensorsCard->setVisible(modules.sensors);
    m_disksCard->setVisible(modules.smart);
}

void OverviewPage::refresh(const ModuleSettings &modules)
{
    setModuleVisibility(modules);
    if (!m_registry) {
        return;
    }
    if (modules.systemd) {
        m_registry->systemd()->refreshFailedCount();
    }
}

#include "OverviewPage.moc"
