#include "DisksPage.h"

#include "../core/SmartHistoryStore.h"
#include "../core/SmartRefreshScheduler.h"
#include "../models/TableFilterProxyModel.h"
#include "../ui/StatusBadgeDelegate.h"
#include "../ui/ThemeManager.h"
#include "../utils/TableLayoutPersistence.h"
#include "../utils/TimeUtils.h"
#include "SmartHistoryDialog.h"
#include "StatusBanner.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

DisksPage::DisksPage(SmartProvider* provider, QWidget* parent)
    : QWidget(parent), m_provider(provider), m_smartScheduler(new SmartRefreshScheduler(this)) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("Disks / SMART"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(QStringLiteral("Filter disks..."));
    m_filter->setClearButtonEnabled(true);
    auto* refreshButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("view-refresh"), QStyle::SP_BrowserReload),
                                          QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_filter, 1);
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new DiskModel(this);
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
    m_table->setItemDelegateForColumn(5, new StatusBadgeDelegate(m_table));
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/disks/headerState"), /*persistSortOrder=*/true);
    layout->addWidget(m_table, 1);

    auto* actions = new QHBoxLayout;
    m_smartButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("drive-harddisk"), QStyle::SP_DriveHDIcon),
                                    QStringLiteral("Check SMART"), this);
    m_smartAllButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("drive-harddisk"), QStyle::SP_DriveHDIcon),
                                       QStringLiteral("Check All SMART"), this);
    m_historyButton = new QPushButton(ThemeManager::themedIcon(QStringLiteral("view-list-details"), QStyle::SP_FileDialogDetailedView),
                                      QStringLiteral("History"), this);
    actions->addWidget(m_smartButton);
    actions->addWidget(m_smartAllButton);
    actions->addWidget(m_historyButton);
    actions->addStretch();
    layout->addLayout(actions);
    m_status = new StatusBanner(this);
    layout->addWidget(m_status);

    connect(m_filter, &QLineEdit::textChanged, m_proxy, &TableFilterProxyModel::setSearchText);
    connect(refreshButton, &QPushButton::clicked, this, &DisksPage::refresh);
    connect(m_smartButton, &QPushButton::clicked, this, [this]() {
        const DiskRow row = selectedRow();
        if (row.path.isEmpty()) {
            m_status->setMessage(QStringLiteral("Select a disk to check SMART."), StatusBanner::Severity::Info);
            return;
        }
        m_status->setMessage(QStringLiteral("Checking SMART for %1...").arg(row.path), StatusBanner::Severity::Info);
        m_provider->checkSmart(row);
    });
    connect(m_smartAllButton, &QPushButton::clicked, this, [this]() {
        const QVector<DiskRow> rows = m_model->rows();
        if (!rows.isEmpty()) {
            m_status->setMessage(QStringLiteral("Checking SMART for %1 disks...").arg(rows.size()), StatusBanner::Severity::Info);
            m_provider->checkSmart(rows);
        }
    });
    connect(m_historyButton, &QPushButton::clicked, this, &DisksPage::openHistoryForSelected);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DisksPage::updateActionStates);
    connect(m_model, &QAbstractItemModel::modelReset, this, &DisksPage::updateActionStates);
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex&) { openHistoryForSelected(); });
    connect(m_table, &QWidget::customContextMenuRequested, this, &DisksPage::showContextMenu);

    connect(m_provider, &SmartProvider::disksReady, this, [this](const QVector<DiskRow>& rows, const QString& error) {
        m_model->setRows(rows);
        m_status->setResultMessage(QStringLiteral("%1 disks").arg(rows.size()), error);
        updateActionStates();
    });
    connect(m_provider, &SmartProvider::smartReady, this, [this](const QString& path, const DiskRow& smartRow, const QString& error) {
        m_model->updateSmart(path, smartRow);
        if (error.isEmpty()) {
            QString serial = smartRow.serial;
            if (serial.trimmed().isEmpty()) {
                for (const DiskRow& inventoryRow : m_model->rows()) {
                    if (inventoryRow.path == path) {
                        serial = inventoryRow.serial;
                        break;
                    }
                }
            }
            SmartHistoryStore::appendEntry(SmartHistoryEntry{path, historyKeySerial(serial), currentTimestamp(), smartRow.smartHealth,
                                                             smartRow.temperature, smartRow.reallocated, smartRow.pending});
            m_status->setMessage(QStringLiteral("SMART check complete for %1").arg(path), StatusBanner::Severity::Ok);
        } else {
            m_status->setResultMessage(QString(), error);
        }
    });
    connect(m_smartScheduler, &SmartRefreshScheduler::smartRefreshRequested, this, &DisksPage::runScheduledSmartChecks);

    m_smartScheduler->reloadFromSettings();
    updateActionStates();
}

DiskRow DisksPage::selectedRow() const {
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) {
        return {};
    }
    return m_model->rowAt(m_proxy->mapToSource(index).row());
}

void DisksPage::updateActionStates() {
    const bool hasSelection = !selectedRow().path.isEmpty();
    m_smartButton->setEnabled(hasSelection);
    m_historyButton->setEnabled(hasSelection);
    m_smartAllButton->setEnabled(!m_model->rows().isEmpty());
}

void DisksPage::openHistoryForSelected() {
    const DiskRow row = selectedRow();
    if (row.path.isEmpty()) {
        m_status->setMessage(QStringLiteral("Select a disk to view SMART history."), StatusBanner::Severity::Info);
        return;
    }
    auto* dialog = new SmartHistoryDialog(historyKeySerial(row.serial), row.path, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void DisksPage::showContextMenu(const QPoint& pos) {
    const QModelIndex index = m_table->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    m_table->selectRow(index.row());
    const DiskRow row = selectedRow();
    if (row.path.isEmpty()) {
        return;
    }

    QMenu menu(this);
    menu.addAction(QIcon::fromTheme(QStringLiteral("drive-harddisk")), QStringLiteral("Check SMART"), this,
                   [this]() { m_smartButton->click(); });
    menu.addAction(QIcon::fromTheme(QStringLiteral("view-list-details")), QStringLiteral("History"), this,
                   &DisksPage::openHistoryForSelected);
    menu.addSeparator();
    menu.addAction(QStringLiteral("Copy device path"), this, [path = row.path]() {
        if (QClipboard* cb = QApplication::clipboard()) {
            cb->setText(path);
        }
    });
    if (!row.serial.isEmpty()) {
        menu.addAction(QStringLiteral("Copy serial"), this, [serial = row.serial]() {
            if (QClipboard* cb = QApplication::clipboard()) {
                cb->setText(serial);
            }
        });
    }
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

QString DisksPage::historyKeySerial(const QString& serial) const {
    const QString trimmed = serial.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    int count = 0;
    for (const DiskRow& row : m_model->rows()) {
        if (row.serial.trimmed() == trimmed) {
            ++count;
        }
    }
    return count > 1 ? QString() : trimmed;
}

void DisksPage::reloadSmartSchedule() {
    m_smartScheduler->reloadFromSettings();
}

void DisksPage::runScheduledSmartChecks() {
    if (m_provider->isSmartCheckBusy()) {
        m_status->setMessage(QStringLiteral("SMART check already in progress; skipping scheduled refresh."), StatusBanner::Severity::Info);
        return;
    }
    const QVector<DiskRow> rows = m_model->rows();
    if (!rows.isEmpty()) {
        m_status->setMessage(QStringLiteral("Checking SMART for %1 disks...").arg(rows.size()), StatusBanner::Severity::Info);
        m_provider->checkSmart(rows);
    }
}

void DisksPage::refresh() {
    m_status->setMessage(QStringLiteral("Refreshing..."), StatusBanner::Severity::Info);
    m_provider->refreshDisks();
}
