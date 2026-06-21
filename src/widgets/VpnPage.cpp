#include "VpnPage.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

VpnPage::VpnPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("VPN"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("card"));
    auto *form = new QFormLayout(panel);
    m_connected = new QLabel(panel);
    m_name = new QLabel(panel);
    m_device = new QLabel(panel);
    m_state = new QLabel(panel);
    m_lastRefresh = new QLabel(panel);
    m_error = new QLabel(panel);
    m_error->setWordWrap(true);
    form->addRow(QStringLiteral("Connected VPN"), m_connected);
    form->addRow(QStringLiteral("Connection name"), m_name);
    form->addRow(QStringLiteral("Device/interface"), m_device);
    form->addRow(QStringLiteral("State"), m_state);
    form->addRow(QStringLiteral("Last refresh"), m_lastRefresh);
    form->addRow(QStringLiteral("Details"), m_error);
    layout->addWidget(panel);
    layout->addStretch();

    connect(refreshButton, &QPushButton::clicked, this, &VpnPage::refresh);
    connect(&m_provider, &NetworkProvider::vpnStatusReady, this, [this](const VpnStatus &status, const QString &error) {
        m_connected->setText(status.connected ? QStringLiteral("Yes") : QStringLiteral("No"));
        m_name->setText(status.connectionName);
        m_device->setText(status.device);
        m_state->setText(status.state);
        m_lastRefresh->setText(status.lastRefresh);
        m_error->setText(error.isEmpty() ? QStringLiteral("OK") : error);
    });
}

void VpnPage::refresh()
{
    m_error->setText(QStringLiteral("Refreshing..."));
    m_provider.refreshVpnStatus();
}
