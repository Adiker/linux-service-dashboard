#include "SystemdPage.h"

#include "../core/ServiceGroupSettings.h"
#include "../utils/TableLayoutPersistence.h"
#include "../utils/TableTools.h"
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

SystemdPage::SystemdPage(SystemdServiceProvider& provider, QWidget* parent) : QWidget(parent), m_provider(provider) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("Systemd Services"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter by service name"));
    auto* refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh"), this);
    layout->addWidget(title);
    header->addWidget(m_filter, 1);
    m_groupSelector = new QComboBox(this);
    m_groupSelector->setMinimumWidth(160);
    for (const QString& group : ServiceGroupSettings::groupNames()) {
        m_groupSelector->addItem(group);
    }
    m_groupSelector->insertItem(0, tr("All services"));
    m_groupSelector->setCurrentIndex(0);
    if (!QSettings().value("systemd/showAll", true).toBool())
        m_groupSelector->setCurrentText(ServiceGroupSettings::activeGroup());
    header->addWidget(m_groupSelector);
    m_scopeSelector = new QComboBox(this);
    m_scopeSelector->addItems({tr("All scopes"), tr("System"), tr("User")});
    header->addWidget(m_scopeSelector);
    connect(m_scopeSelector, &QComboBox::currentIndexChanged, this, [this]() { applyFilters(); });
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new ServiceModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setSortingEnabled(true);
    m_table->setColumnWidth(0, 250);
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/systemd/headerState"), /*persistSortOrder=*/true);
    layout->addWidget(m_table, 1);

    auto* actions = new QHBoxLayout;
    for (const QString& action : {QStringLiteral("start"), QStringLiteral("stop"), QStringLiteral("restart")}) {
        auto* button = new QPushButton(action == "start" ? tr("Start") : action == "stop" ? tr("Stop") : tr("Restart"), this);
        connect(button, &QPushButton::clicked, this, [this, action]() { runAction(action); });
        actions->addWidget(button);
    }
    auto* logsButton = new QPushButton(QIcon::fromTheme(QStringLiteral("text-x-generic")), tr("View logs"), this);
    actions->addWidget(logsButton);
    actions->addStretch();
    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    actions->addWidget(m_status);
    layout->addLayout(actions);

    connect(m_filter, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);
    connect(m_groupSelector, &QComboBox::currentTextChanged, this, [this](const QString& group) {
        QSettings().setValue("systemd/showAll", m_groupSelector->currentIndex() == 0);
        if (m_groupSelector->currentIndex() > 0)
            ServiceGroupSettings::setActiveGroup(group);
        applyFilters();
    });
    connect(refreshButton, &QPushButton::clicked, this, &SystemdPage::refresh);
    connect(logsButton, &QPushButton::clicked, this, [this]() {
        const ServiceRow row = selectedRow();
        if (!row.unit.isEmpty()) {
            m_provider.serviceLogs(row.unit, row.scope);
        }
    });
    connect(&m_provider, &SystemdServiceProvider::servicesReady, this, [this](const QVector<ServiceRow>& rows, const QString& error) {
        if (error.isEmpty()) {
            m_rows = rows;
            applyFilters();
        }
        m_status->setText(error.isEmpty() ? tr("%1 services").arg(rows.size()) : error);
    });
    connect(&m_provider, &SystemdServiceProvider::actionFinished, this, [this](const QString& message, const QString& details) {
        QMessageBox::information(this, QStringLiteral("systemd"), details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
        refresh();
    });
    TableTools::install(m_table, layout, false);
}

QStringList SystemdPage::watchedServices() const {
    return ServiceGroupSettings::servicesForGroup(m_groupSelector->currentText());
}

ServiceRow SystemdPage::selectedRow() const {
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid())
        return {};
    return m_model->rowAt(m_proxy->mapToSource(index).row());
}

void SystemdPage::runAction(const QString& action) {
    const ServiceRow row = selectedRow();
    if (row.unit.isEmpty())
        return;
    if (SystemdServiceProvider::isProtectedUnit(row.unit)) {
        m_status->setText(tr("Read-only service"));
        return;
    }
    if (ConfirmActionDialog::confirm(
            this, tr("Confirm systemd action"),
            tr("Run systemctl %1 %2 %3?").arg(row.scope == ServiceScope::User ? "--user" : "--system", action, row.unit))) {
        m_provider.controlService(row.unit, action, row.scope);
    }
}

void SystemdPage::refresh() {
    m_status->setText(tr("Refreshing..."));
    m_provider.refreshServices({});
}

void SystemdPage::reloadGroupSelector() {
    const QString current = m_groupSelector->currentText();
    m_groupSelector->blockSignals(true);
    m_groupSelector->clear();
    m_groupSelector->addItem(tr("All services"));
    for (const QString& group : ServiceGroupSettings::groupNames()) {
        m_groupSelector->addItem(group);
    }
    const int index =
        QSettings().value("systemd/showAll", true).toBool() ? 0 : m_groupSelector->findText(ServiceGroupSettings::activeGroup());
    if (index >= 0) {
        m_groupSelector->setCurrentIndex(index);
    } else {
        const int previous = m_groupSelector->findText(current);
        m_groupSelector->setCurrentIndex(previous >= 0 ? previous : 0);
    }
    m_groupSelector->blockSignals(false);
    refresh();
}

void SystemdPage::applyFilters() {
    QVector<ServiceRow> out;
    const auto watched = watchedServices();
    for (const auto& r : m_rows) {
        if (m_scopeSelector->currentIndex() == 1 && r.scope != ServiceScope::System)
            continue;
        if (m_scopeSelector->currentIndex() == 2 && r.scope != ServiceScope::User)
            continue;
        if (m_groupSelector->currentIndex() > 0 && !watched.isEmpty() && !watched.contains(r.unit) &&
            !watched.contains((r.scope == ServiceScope::User ? "user:" : "system:") + r.unit))
            continue;
        out.append(r);
    }
    if (m_groupSelector->currentIndex() > 0)
        for (const auto& entry : watched) {
            const bool explicitUser = entry.startsWith("user:"), explicitSystem = entry.startsWith("system:");
            const auto unit = explicitUser || explicitSystem ? entry.section(':', 1) : entry;
            bool found = false;
            for (const auto& row : m_rows)
                if (row.unit == unit && (!explicitUser || row.scope == ServiceScope::User) &&
                    (!explicitSystem || row.scope == ServiceScope::System)) {
                    found = true;
                    break;
                }
            const auto scope = explicitUser ? ServiceScope::User : ServiceScope::System;
            if (!found &&
                (m_scopeSelector->currentIndex() == 0 || (m_scopeSelector->currentIndex() == 1 && scope == ServiceScope::System) ||
                 (m_scopeSelector->currentIndex() == 2 && scope == ServiceScope::User)))
                out.append({unit, "unavailable", "unknown", "-", tr("Unit not found"), "-", scope, "-"});
        }
    m_model->setRows(out);
}

void SystemdPage::selectRecord(const QString&) {
    m_groupSelector->setCurrentIndex(0);
    m_scopeSelector->setCurrentIndex(0);
    m_filter->clear();
    applyFilters();
}
