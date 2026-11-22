#include "SystemdPage.h"

#include "../core/ServiceGroupSettings.h"
#include "../models/TableFilterProxyModel.h"
#include "../ui/StatusBadgeDelegate.h"
#include "../ui/ThemeManager.h"
#include "../utils/TableLayoutPersistence.h"
#include "ConfirmActionDialog.h"
#include "LogViewerDialog.h"
#include "StatusBanner.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

SystemdPage::SystemdPage(SystemdServiceProvider* provider, QWidget* parent) : QWidget(parent), m_provider(provider) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("Systemd Services"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(QStringLiteral("Filter services..."));
    m_filter->setClearButtonEnabled(true);
    m_stateFilter = new QComboBox(this);
    m_stateFilter->addItems({QStringLiteral("All states"), QStringLiteral("Active"), QStringLiteral("Failed"), QStringLiteral("Inactive")});
    m_groupSelector = new QComboBox(this);
    m_groupSelector->setMinimumWidth(160);
    for (const QString& group : ServiceGroupSettings::groupNames()) {
        m_groupSelector->addItem(group);
    }
    m_groupSelector->setCurrentText(ServiceGroupSettings::activeGroup());
    m_allServicesCheck = new QCheckBox(QStringLiteral("All services"), this);
    auto* refreshButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("view-refresh"), QStyle::SP_BrowserReload),
                                          QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_filter, 1);
    header->addWidget(m_stateFilter);
    header->addWidget(m_groupSelector);
    header->addWidget(m_allServicesCheck);
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new ServiceModel(this);
    m_proxy = new TableFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setSortingEnabled(true);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setItemDelegateForColumn(2, new StatusBadgeDelegate(m_table));
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/systemd/headerState"), /*persistSortOrder=*/true);
    layout->addWidget(m_table, 1);

    auto* actions = new QHBoxLayout;
    m_startButton = new QPushButton(QStringLiteral("Start"), this);
    m_stopButton = new QPushButton(QStringLiteral("Stop"), this);
    m_restartButton = new QPushButton(QStringLiteral("Restart"), this);
    m_logsButton =
        new QPushButton(ThemeManager::themedIcon(QStringLiteral("text-x-generic"), QStyle::SP_FileIcon), QStringLiteral("View logs"), this);
    actions->addWidget(m_startButton);
    actions->addWidget(m_stopButton);
    actions->addWidget(m_restartButton);
    actions->addWidget(m_logsButton);
    actions->addStretch();
    layout->addLayout(actions);
    m_status = new StatusBanner(this);
    layout->addWidget(m_status);

    connect(m_startButton, &QPushButton::clicked, this, [this]() { runAction(QStringLiteral("start")); });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() { runAction(QStringLiteral("stop")); });
    connect(m_restartButton, &QPushButton::clicked, this, [this]() { runAction(QStringLiteral("restart")); });
    connect(m_filter, &QLineEdit::textChanged, m_proxy, &TableFilterProxyModel::setSearchText);
    connect(m_stateFilter, &QComboBox::currentTextChanged, this, [this](const QString& state) {
        if (state == QStringLiteral("All states")) {
            m_proxy->setRowPredicate({});
            return;
        }
        const QString expected = state.toLower();
        m_proxy->setRowPredicate([expected](int sourceRow, const QAbstractItemModel* model) {
            return model->index(sourceRow, 2).data(Qt::DisplayRole).toString().toLower() == expected;
        });
    });
    connect(m_groupSelector, &QComboBox::currentTextChanged, this, [this](const QString& group) {
        ServiceGroupSettings::setActiveGroup(group);
        refresh();
    });
    connect(m_allServicesCheck, &QCheckBox::toggled, this, [this](bool showAll) {
        m_groupSelector->setEnabled(!showAll);
        refresh();
    });
    connect(refreshButton, &QPushButton::clicked, this, &SystemdPage::refresh);
    connect(m_logsButton, &QPushButton::clicked, this, [this]() {
        const ServiceRow row = selectedRow();
        if (!row.unit.isEmpty()) {
            m_provider->serviceLogs(row.unit);
        }
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SystemdPage::updateActionStates);
    connect(m_model, &QAbstractItemModel::modelReset, this, &SystemdPage::updateActionStates);
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex&) {
        const ServiceRow row = selectedRow();
        if (!row.unit.isEmpty()) {
            m_provider->serviceLogs(row.unit);
        }
    });
    connect(m_table, &QWidget::customContextMenuRequested, this, &SystemdPage::showContextMenu);

    connect(m_provider, &SystemdServiceProvider::servicesReady, this, [this](const QVector<ServiceRow>& rows, const QString& error) {
        m_model->setRows(rows);
        m_status->setResultMessage(QStringLiteral("%1 services").arg(rows.size()), error);
        updateActionStates();
    });
    connect(m_provider, &SystemdServiceProvider::logsReady, this, [this](const QString& title, const QString& text) {
        if (auto existing = m_logDialogs.value(title); existing) {
            existing->setContent(text);
            existing->raise();
            existing->activateWindow();
            return;
        }
        const QString prefix = QStringLiteral("journalctl -u ");
        LogViewerDialog::RefreshCallback refreshCb;
        if (title.startsWith(prefix)) {
            const QString unit = title.mid(prefix.size());
            refreshCb = [this, unit](int tailLines) { m_provider->serviceLogs(unit, tailLines); };
        }
        auto* dialog = new LogViewerDialog(title, text, this, refreshCb);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        m_logDialogs.insert(title, dialog);
        dialog->show();
    });
    connect(m_provider, &SystemdServiceProvider::actionFinished, this, [this](const QString& message, const QString& details) {
        QMessageBox::information(this, QStringLiteral("systemd"), details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
        refresh();
        m_provider->refreshFailedCount();
    });

    updateActionStates();
}

QStringList SystemdPage::watchedServices() const {
    if (m_allServicesCheck && m_allServicesCheck->isChecked()) {
        return {};
    }
    return ServiceGroupSettings::servicesForGroup(ServiceGroupSettings::activeGroup());
}

ServiceRow SystemdPage::selectedRow() const {
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) {
        return {};
    }
    return m_model->rowAt(m_proxy->mapToSource(index).row());
}

void SystemdPage::updateActionStates() {
    const bool hasSelection = !selectedRow().unit.isEmpty();
    m_startButton->setEnabled(hasSelection);
    m_stopButton->setEnabled(hasSelection);
    m_restartButton->setEnabled(hasSelection);
    m_logsButton->setEnabled(hasSelection);
}

void SystemdPage::showContextMenu(const QPoint& pos) {
    const QModelIndex index = m_table->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    m_table->selectRow(index.row());
    const ServiceRow row = selectedRow();
    if (row.unit.isEmpty()) {
        return;
    }

    QMenu menu(this);
    menu.addAction(QStringLiteral("Start"), this, [this]() { runAction(QStringLiteral("start")); });
    menu.addAction(QStringLiteral("Stop"), this, [this]() { runAction(QStringLiteral("stop")); });
    menu.addAction(QStringLiteral("Restart"), this, [this]() { runAction(QStringLiteral("restart")); });
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("text-x-generic")), QStringLiteral("View logs"), this,
                   [this, unit = row.unit]() { m_provider->serviceLogs(unit); });
    menu.addAction(QStringLiteral("Copy unit name"), this, [unit = row.unit]() {
        if (QClipboard* cb = QApplication::clipboard()) {
            cb->setText(unit);
        }
    });
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void SystemdPage::runAction(const QString& action) {
    const ServiceRow row = selectedRow();
    if (row.unit.isEmpty()) {
        return;
    }
    if (ConfirmActionDialog::confirm(this, QStringLiteral("Confirm systemd action"),
                                     QStringLiteral("Run systemctl %1 %2?").arg(action, row.unit))) {
        m_provider->controlService(row.unit, action);
    }
}

void SystemdPage::refresh() {
    m_status->setMessage(QStringLiteral("Refreshing..."), StatusBanner::Severity::Info);
    m_provider->refreshServices(watchedServices());
}

void SystemdPage::reloadGroupSelector() {
    const QString current = m_groupSelector->currentText();
    m_groupSelector->blockSignals(true);
    m_groupSelector->clear();
    for (const QString& group : ServiceGroupSettings::groupNames()) {
        m_groupSelector->addItem(group);
    }
    const int index = m_groupSelector->findText(ServiceGroupSettings::activeGroup());
    if (index >= 0) {
        m_groupSelector->setCurrentIndex(index);
    } else {
        const int previous = m_groupSelector->findText(current);
        m_groupSelector->setCurrentIndex(previous >= 0 ? previous : 0);
    }
    m_groupSelector->blockSignals(false);
    refresh();
}
