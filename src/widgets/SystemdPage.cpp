#include "SystemdPage.h"

#include "../core/ServiceGroupSettings.h"

#include "ConfirmActionDialog.h"
#include "LogViewerDialog.h"

#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

SystemdPage::SystemdPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("Systemd Services"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(QStringLiteral("Filter by service name"));
    auto *refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_filter, 1);
    m_groupSelector = new QComboBox(this);
    m_groupSelector->setMinimumWidth(160);
    for (const QString &group : ServiceGroupSettings::groupNames()) {
        m_groupSelector->addItem(group);
    }
    m_groupSelector->setCurrentText(ServiceGroupSettings::activeGroup());
    header->addWidget(m_groupSelector);
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new ServiceModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(0);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setSortingEnabled(true);
    layout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    for (const QString &action : {QStringLiteral("start"), QStringLiteral("stop"), QStringLiteral("restart")}) {
        auto *button = new QPushButton(action.left(1).toUpper() + action.mid(1), this);
        connect(button, &QPushButton::clicked, this, [this, action]() { runAction(action); });
        actions->addWidget(button);
    }
    auto *logsButton = new QPushButton(QIcon::fromTheme(QStringLiteral("text-x-generic")), QStringLiteral("View logs"), this);
    actions->addWidget(logsButton);
    actions->addStretch();
    m_status = new QLabel(this);
    actions->addWidget(m_status);
    layout->addLayout(actions);

    connect(m_filter, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);
    connect(m_groupSelector, &QComboBox::currentTextChanged, this, [this](const QString &group) {
        ServiceGroupSettings::setActiveGroup(group);
        refresh();
    });
    connect(refreshButton, &QPushButton::clicked, this, &SystemdPage::refresh);
    connect(logsButton, &QPushButton::clicked, this, [this]() {
        const ServiceRow row = selectedRow();
        if (!row.unit.isEmpty()) {
            m_provider.serviceLogs(row.unit);
        }
    });
    connect(&m_provider, &SystemdServiceProvider::servicesReady, this, [this](const QVector<ServiceRow> &rows, const QString &error) {
        m_model->setRows(rows);
        m_status->setText(error.isEmpty() ? QStringLiteral("%1 services").arg(rows.size()) : error);
    });
    connect(&m_provider, &SystemdServiceProvider::logsReady, this, [this](const QString &title, const QString &text) {
        auto *dialog = new LogViewerDialog(title, text, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    connect(&m_provider, &SystemdServiceProvider::actionFinished, this, [this](const QString &message, const QString &details) {
        QMessageBox::information(this, QStringLiteral("systemd"), details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
        refresh();
    });

    refresh();
}

QStringList SystemdPage::watchedServices() const
{
    return ServiceGroupSettings::servicesForGroup(ServiceGroupSettings::activeGroup());
}

ServiceRow SystemdPage::selectedRow() const
{
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) return {};
    return m_model->rowAt(m_proxy->mapToSource(index).row());
}

void SystemdPage::runAction(const QString &action)
{
    const ServiceRow row = selectedRow();
    if (row.unit.isEmpty()) return;
    if (ConfirmActionDialog::confirm(this, QStringLiteral("Confirm systemd action"), QStringLiteral("Run systemctl %1 %2?").arg(action, row.unit))) {
        m_provider.controlService(row.unit, action);
    }
}

void SystemdPage::refresh()
{
    m_status->setText(QStringLiteral("Refreshing..."));
    m_provider.refreshServices(watchedServices());
}
