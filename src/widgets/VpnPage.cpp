#include "VpnPage.h"

#include "../ui/StatusBadgeDelegate.h"
#include "../ui/ThemeManager.h"
#include "ConfirmActionDialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

VpnPage::VpnPage(NetworkProvider* provider, QWidget* parent) : QWidget(parent), m_provider(provider) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("VPN"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto* refreshButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("view-refresh"), QStyle::SP_BrowserReload),
                                          QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("card"));
    auto* form = new QFormLayout(panel);
    m_connected = new QLabel(panel);
    m_name = new QLabel(panel);
    m_device = new QLabel(panel);
    m_state = new QLabel(panel);
    m_lastRefresh = new QLabel(panel);
    m_error = new QLabel(panel);
    m_error->setWordWrap(true);
    form->addRow(QStringLiteral("Active tunnels"), m_connected);
    form->addRow(QStringLiteral("Connection(s)"), m_name);
    form->addRow(QStringLiteral("Device/interface"), m_device);
    form->addRow(QStringLiteral("State"), m_state);
    form->addRow(QStringLiteral("Last refresh"), m_lastRefresh);
    form->addRow(QStringLiteral("Details"), m_error);
    layout->addWidget(panel);

    auto* profilesLabel = new QLabel(QStringLiteral("Configured VPN connections"), this);
    profilesLabel->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(profilesLabel);

    m_profilesTable = new QTableWidget(this);
    m_profilesTable->setColumnCount(4);
    m_profilesTable->setHorizontalHeaderLabels({
        QStringLiteral("Name"),
        QStringLiteral("Type"),
        QStringLiteral("Device"),
        QStringLiteral("State"),
    });
    m_profilesTable->horizontalHeader()->setStretchLastSection(true);
    m_profilesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_profilesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profilesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_profilesTable->setAlternatingRowColors(true);
    m_profilesTable->verticalHeader()->hide();
    m_profilesTable->setItemDelegateForColumn(3, new StatusBadgeDelegate(m_profilesTable));
    layout->addWidget(m_profilesTable, 1);

    auto* actions = new QHBoxLayout;
    m_connectButton = new QPushButton(QStringLiteral("Connect"), this);
    m_disconnectButton = new QPushButton(QStringLiteral("Disconnect"), this);
    actions->addWidget(m_connectButton);
    actions->addWidget(m_disconnectButton);
    actions->addStretch();
    layout->addLayout(actions);

    connect(refreshButton, &QPushButton::clicked, this, &VpnPage::refresh);
    connect(m_profilesTable, &QTableWidget::itemSelectionChanged, this, &VpnPage::updateActionStates);
    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        const VpnProfile profile = selectedProfile();
        if (!profile.name.isEmpty()) {
            m_error->setText(QStringLiteral("Connecting %1...").arg(profile.name));
            m_provider->connectVpn(profile.name);
        }
    });
    connect(m_disconnectButton, &QPushButton::clicked, this, [this]() {
        const VpnProfile profile = selectedProfile();
        if (profile.name.isEmpty()) {
            return;
        }
        if (ConfirmActionDialog::confirm(this, QStringLiteral("Disconnect VPN"), QStringLiteral("Disconnect VPN %1?").arg(profile.name))) {
            m_error->setText(QStringLiteral("Disconnecting %1...").arg(profile.name));
            m_provider->disconnectVpn(profile.name);
        }
    });

    connect(m_provider, &NetworkProvider::activeVpnsReady, this, [this](const QVector<VpnStatus>& activeVpns, const QString& error) {
        if (activeVpns.isEmpty()) {
            m_connected->setText(QStringLiteral("No (0)"));
            m_name->setText(QStringLiteral("None"));
            m_device->setText(QStringLiteral("-"));
            m_state->setText(QStringLiteral("Disconnected"));
        } else {
            QStringList names;
            QStringList devices;
            QStringList states;
            for (const VpnStatus& vpn : activeVpns) {
                names.append(vpn.connectionName);
                devices.append(vpn.device);
                states.append(vpn.state);
            }
            m_connected->setText(QStringLiteral("Yes (%1)").arg(activeVpns.size()));
            m_name->setText(names.join(QStringLiteral(", ")));
            m_device->setText(devices.join(QStringLiteral(", ")));
            m_state->setText(states.join(QStringLiteral(", ")));
            m_lastRefresh->setText(activeVpns.first().lastRefresh);
        }
        m_error->setText(error.isEmpty() ? QStringLiteral("OK") : error);
    });
    connect(m_provider, &NetworkProvider::vpnStatusReady, this, [this](const VpnStatus& status, const QString&) {
        if (!status.lastRefresh.isEmpty()) {
            m_lastRefresh->setText(status.lastRefresh);
        }
    });
    connect(m_provider, &NetworkProvider::vpnProfilesReady, this, [this](const QVector<VpnProfile>& profiles, const QString& error) {
        m_profiles = profiles;
        m_profilesTable->setRowCount(profiles.size());
        for (int i = 0; i < profiles.size(); ++i) {
            const VpnProfile& profile = profiles.at(i);
            m_profilesTable->setItem(i, 0, new QTableWidgetItem(profile.name));
            m_profilesTable->setItem(i, 1, new QTableWidgetItem(profile.type));
            m_profilesTable->setItem(i, 2, new QTableWidgetItem(profile.device));
            m_profilesTable->setItem(i, 3, new QTableWidgetItem(profile.state));
        }
        if (!error.isEmpty()) {
            m_error->setText(error);
        }
        updateActionStates();
    });
    connect(m_provider, &NetworkProvider::actionFinished, this, [this](const QString& message, const QString& details) {
        QMessageBox::information(this, QStringLiteral("VPN"), details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
        refresh();
    });

    updateActionStates();
}

VpnProfile VpnPage::selectedProfile() const {
    const int row = m_profilesTable->currentRow();
    if (row < 0 || row >= m_profiles.size()) {
        return {};
    }
    return m_profiles.at(row);
}

void VpnPage::updateActionStates() {
    const VpnProfile profile = selectedProfile();
    const bool hasSelection = !profile.name.isEmpty();
    m_connectButton->setEnabled(hasSelection && !profile.active);
    m_disconnectButton->setEnabled(hasSelection && profile.active);
}

void VpnPage::refresh() {
    m_error->setText(QStringLiteral("Refreshing..."));
    m_provider->refreshVpnStatus();
    m_provider->refreshVpnProfiles();
}
