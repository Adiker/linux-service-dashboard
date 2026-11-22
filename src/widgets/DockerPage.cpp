#include "DockerPage.h"

#include "../models/TableFilterProxyModel.h"
#include "../ui/StatusBadgeDelegate.h"
#include "../ui/ThemeManager.h"
#include "../utils/TableLayoutPersistence.h"
#include "ConfirmActionDialog.h"
#include "LogViewerDialog.h"
#include "StatusBanner.h"

#include <QApplication>
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

DockerPage::DockerPage(DockerProvider* provider, QWidget* parent) : QWidget(parent), m_provider(provider) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("Docker"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(QStringLiteral("Filter containers..."));
    m_filter->setClearButtonEnabled(true);
    m_stateFilter = new QComboBox(this);
    m_stateFilter->addItems({QStringLiteral("All"), QStringLiteral("Running"), QStringLiteral("Stopped")});
    auto* refreshButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("view-refresh"), QStyle::SP_BrowserReload),
                                          QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_filter, 1);
    header->addWidget(m_stateFilter);
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new DockerContainerModel(this);
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
    m_table->setItemDelegateForColumn(3, new StatusBadgeDelegate(m_table));
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/docker/headerState"), /*persistSortOrder=*/true);
    layout->addWidget(m_table, 1);

    auto* actions = new QHBoxLayout;
    m_startButton = new QPushButton(QStringLiteral("Start"), this);
    m_stopButton = new QPushButton(QStringLiteral("Stop"), this);
    m_restartButton = new QPushButton(QStringLiteral("Restart"), this);
    m_logsButton =
        new QPushButton(ThemeManager::themedIcon(QStringLiteral("text-x-generic"), QStyle::SP_FileIcon), QStringLiteral("View logs"), this);
    m_inspectButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("document-properties"), QStyle::SP_FileDialogDetailedView),
                                      QStringLiteral("Inspect JSON"), this);
    actions->addWidget(m_startButton);
    actions->addWidget(m_stopButton);
    actions->addWidget(m_restartButton);
    actions->addWidget(m_logsButton);
    actions->addWidget(m_inspectButton);
    actions->addStretch();
    layout->addLayout(actions);
    m_status = new StatusBanner(this);
    layout->addWidget(m_status);

    connect(m_startButton, &QPushButton::clicked, this, [this]() { runAction(QStringLiteral("start")); });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() { runAction(QStringLiteral("stop")); });
    connect(m_restartButton, &QPushButton::clicked, this, [this]() { runAction(QStringLiteral("restart")); });
    connect(m_filter, &QLineEdit::textChanged, m_proxy, &TableFilterProxyModel::setSearchText);
    connect(m_stateFilter, &QComboBox::currentTextChanged, this, [this](const QString& state) {
        if (state == QStringLiteral("All")) {
            m_proxy->setRowPredicate({});
        } else if (state == QStringLiteral("Running")) {
            m_proxy->setRowPredicate([](int sourceRow, const QAbstractItemModel* model) {
                return model->index(sourceRow, 3).data(Qt::DisplayRole).toString() == QStringLiteral("running");
            });
        } else {
            m_proxy->setRowPredicate([](int sourceRow, const QAbstractItemModel* model) {
                return model->index(sourceRow, 3).data(Qt::DisplayRole).toString() != QStringLiteral("running");
            });
        }
    });
    connect(refreshButton, &QPushButton::clicked, this, &DockerPage::refresh);
    connect(m_logsButton, &QPushButton::clicked, this, [this]() {
        const auto row = selectedRow();
        if (!row.id.isEmpty()) {
            m_provider->containerLogs(row.id);
        }
    });
    connect(m_inspectButton, &QPushButton::clicked, this, [this]() {
        const auto row = selectedRow();
        if (!row.id.isEmpty()) {
            m_provider->inspectContainer(row.id);
        }
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DockerPage::updateActionStates);
    connect(m_model, &QAbstractItemModel::modelReset, this, &DockerPage::updateActionStates);
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex&) {
        const auto row = selectedRow();
        if (!row.id.isEmpty()) {
            m_provider->containerLogs(row.id);
        }
    });
    connect(m_table, &QWidget::customContextMenuRequested, this, &DockerPage::showContextMenu);

    connect(m_provider, &DockerProvider::containersReady, this, [this](const QVector<DockerContainerRow>& rows, const QString& error) {
        m_model->setRows(rows);
        m_status->setResultMessage(QStringLiteral("%1 containers").arg(rows.size()), error);
        updateActionStates();
    });
    connect(m_provider, &DockerProvider::textReady, this, [this](const QString& title, const QString& text) {
        if (auto existing = m_textDialogs.value(title); existing) {
            existing->setContent(text);
            existing->raise();
            existing->activateWindow();
            return;
        }
        LogViewerDialog::RefreshCallback refreshCb;
        const QString logsPrefix = QStringLiteral("docker logs ");
        const QString inspectPrefix = QStringLiteral("docker inspect ");
        if (title.startsWith(logsPrefix)) {
            const QString id = title.mid(logsPrefix.size());
            refreshCb = [this, id](int tailLines) { m_provider->containerLogs(id, tailLines); };
        } else if (title.startsWith(inspectPrefix)) {
            const QString id = title.mid(inspectPrefix.size());
            refreshCb = [this, id](int) { m_provider->inspectContainer(id); };
        }
        auto* dialog = new LogViewerDialog(title, text, this, refreshCb);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        m_textDialogs.insert(title, dialog);
        dialog->show();
    });
    connect(m_provider, &DockerProvider::actionFinished, this, [this](const QString& message, const QString& details) {
        QMessageBox::information(this, QStringLiteral("Docker"), details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
        refresh();
    });

    updateActionStates();
}

DockerContainerRow DockerPage::selectedRow() const {
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) {
        return {};
    }
    return m_model->rowAt(m_proxy->mapToSource(index).row());
}

void DockerPage::updateActionStates() {
    const bool hasSelection = !selectedRow().id.isEmpty();
    m_startButton->setEnabled(hasSelection);
    m_stopButton->setEnabled(hasSelection);
    m_restartButton->setEnabled(hasSelection);
    m_logsButton->setEnabled(hasSelection);
    m_inspectButton->setEnabled(hasSelection);
}

void DockerPage::showContextMenu(const QPoint& pos) {
    const QModelIndex index = m_table->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    m_table->selectRow(index.row());
    const DockerContainerRow row = selectedRow();
    if (row.id.isEmpty()) {
        return;
    }

    QMenu menu(this);
    menu.addAction(QStringLiteral("Start"), this, [this]() { runAction(QStringLiteral("start")); });
    menu.addAction(QStringLiteral("Stop"), this, [this]() { runAction(QStringLiteral("stop")); });
    menu.addAction(QStringLiteral("Restart"), this, [this]() { runAction(QStringLiteral("restart")); });
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("text-x-generic")), QStringLiteral("View logs"), this,
                   [this, id = row.id]() { m_provider->containerLogs(id); });
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), QStringLiteral("Inspect JSON"), this,
                   [this, id = row.id]() { m_provider->inspectContainer(id); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("Copy container name"), this, [name = row.name]() {
        if (QClipboard* cb = QApplication::clipboard()) {
            cb->setText(name);
        }
    });
    menu.addAction(QStringLiteral("Copy container ID"), this, [id = row.id]() {
        if (QClipboard* cb = QApplication::clipboard()) {
            cb->setText(id);
        }
    });
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void DockerPage::runAction(const QString& action) {
    const auto row = selectedRow();
    if (row.id.isEmpty()) {
        return;
    }
    if (ConfirmActionDialog::confirm(this, QStringLiteral("Confirm Docker action"),
                                     QStringLiteral("Run docker %1 %2?").arg(action, row.name))) {
        m_provider->controlContainer(row.id, action);
    }
}

void DockerPage::refresh() {
    m_status->setMessage(QStringLiteral("Refreshing..."), StatusBanner::Severity::Info);
    m_provider->refreshContainers();
}
