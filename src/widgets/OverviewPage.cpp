#include "OverviewPage.h"

#include "../core/ModuleSettings.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

OverviewPage::OverviewPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Overview"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    auto *grid = new QGridLayout;
    grid->setSpacing(16);
    m_dockerValue = addCard(grid, 0, 0, QStringLiteral("Docker"));
    m_systemdValue = addCard(grid, 0, 1, QStringLiteral("Systemd"));
    m_vpnValue = addCard(grid, 0, 2, QStringLiteral("VPN"));
    m_mountsValue = addCard(grid, 1, 0, QStringLiteral("Mounts"));
    m_sensorsValue = addCard(grid, 1, 1, QStringLiteral("Sensors"));
    m_disksValue = addCard(grid, 1, 2, QStringLiteral("Disks / SMART"));
    layout->addLayout(grid);
    layout->addStretch();

    connect(refreshButton, &QPushButton::clicked, this, [this]() { refresh(); });
    connect(&m_docker, &DockerProvider::containersReady, this, [this](const QVector<DockerContainerRow> &rows, const QString &error) {
        if (!error.isEmpty()) {
            m_dockerValue->setText(QStringLiteral("Warning\n%1").arg(error));
            return;
        }
        int running = 0;
        for (const auto &row : rows) {
            if (row.state == QStringLiteral("running")) ++running;
        }
        m_dockerValue->setText(QStringLiteral("%1 / %2 running").arg(running).arg(rows.size()));
    });
    connect(&m_systemd, &SystemdServiceProvider::failedCountReady, this, [this](int count, const QString &error) {
        m_systemdValue->setText(error.isEmpty() ? QStringLiteral("%1 failed services").arg(count) : QStringLiteral("Unknown\n%1").arg(error));
    });
    connect(&m_network, &NetworkProvider::vpnStatusReady, this, [this](const VpnStatus &status, const QString &error) {
        m_vpnValue->setText(error.isEmpty()
            ? QStringLiteral("%1\n%2").arg(status.connected ? QStringLiteral("Connected") : QStringLiteral("Disconnected"), status.connectionName)
            : QStringLiteral("Unknown\n%1").arg(error));
    });
    connect(&m_mounts, &MountProvider::mountsReady, this, [this](const QVector<MountRow> &rows, const QString &error) {
        m_mountsValue->setText(error.isEmpty() ? QStringLiteral("%1 network mounts").arg(rows.size()) : QStringLiteral("Unknown\n%1").arg(error));
    });
    connect(&m_sensors, &SensorProvider::sensorsReady, this, [this](const QVector<SensorRow> &rows, const QString &error) {
        if (!error.isEmpty()) {
            m_sensorsValue->setText(QStringLiteral("Unknown\n%1").arg(error));
            return;
        }
        double maxValue = 0.0;
        QString label = QStringLiteral("No readings");
        for (const auto &row : rows) {
            if (row.current > maxValue) {
                maxValue = row.current;
                label = QStringLiteral("%1 %2 C").arg(row.label).arg(maxValue, 0, 'f', 1);
            }
        }
        m_sensorsValue->setText(label);
    });
    connect(&m_smart, &SmartProvider::disksReady, this, [this](const QVector<DiskRow> &rows, const QString &error) {
        m_disksValue->setText(error.isEmpty() ? QStringLiteral("%1 physical disks\nSMART manual").arg(rows.size()) : QStringLiteral("Unknown\n%1").arg(error));
    });
}

QLabel *OverviewPage::addCard(QGridLayout *grid, int row, int column, const QString &title)
{
    auto *card = new QWidget(this);
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    auto *titleLabel = new QLabel(title, card);
    auto *valueLabel = new QLabel(QStringLiteral("Loading..."), card);
    valueLabel->setObjectName(QStringLiteral("cardValue"));
    valueLabel->setWordWrap(true);
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    layout->addStretch();
    grid->addWidget(card, row, column);
    return valueLabel;
}

void OverviewPage::setCardVisible(QLabel *value, bool visible)
{
    if (!value) {
        return;
    }
    if (QWidget *card = value->parentWidget()) {
        card->setVisible(visible);
    }
}

void OverviewPage::refresh(const ModuleSettings &modules)
{
    setCardVisible(m_dockerValue, modules.docker);
    setCardVisible(m_systemdValue, modules.systemd);
    setCardVisible(m_vpnValue, modules.vpn);
    setCardVisible(m_mountsValue, modules.mounts);
    setCardVisible(m_sensorsValue, modules.sensors);
    setCardVisible(m_disksValue, modules.smart);

    if (modules.docker) {
        m_docker.refreshContainers();
    }
    if (modules.systemd) {
        m_systemd.refreshFailedCount();
    }
    if (modules.vpn) {
        m_network.refreshVpnStatus();
    }
    if (modules.mounts) {
        m_mounts.refreshMounts();
    }
    if (modules.sensors) {
        m_sensors.refreshSensors();
    }
    if (modules.smart) {
        m_smart.refreshDisks();
    }
}
