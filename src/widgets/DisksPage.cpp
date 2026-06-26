#include "DisksPage.h"

#include "../core/SmartHistoryStore.h"
#include "../core/SmartRefreshScheduler.h"
#include "../utils/TableLayoutPersistence.h"
#include "../utils/TimeUtils.h"

#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>

DisksPage::DisksPage(QWidget* parent) : QWidget(parent), m_smartScheduler(new SmartRefreshScheduler(this)) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("Disks / SMART"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto* refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    m_model = new DiskModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setSortingEnabled(true);
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/disks/headerState"));
    layout->addWidget(m_table, 1);

    auto* actions = new QHBoxLayout;
    auto* smartButton = new QPushButton(QIcon::fromTheme(QStringLiteral("drive-harddisk")), QStringLiteral("Check SMART"), this);
    auto* smartAllButton = new QPushButton(QIcon::fromTheme(QStringLiteral("drive-harddisk")), QStringLiteral("Check All SMART"), this);
    auto* historyButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-list-details")), QStringLiteral("History"), this);
    actions->addWidget(smartButton);
    actions->addWidget(smartAllButton);
    actions->addWidget(historyButton);
    actions->addStretch();
    m_status = new QLabel(this);
    actions->addWidget(m_status);
    layout->addLayout(actions);

    connect(refreshButton, &QPushButton::clicked, this, &DisksPage::refresh);
    connect(smartButton, &QPushButton::clicked, this, [this]() {
        const DiskRow row = selectedRow();
        if (row.path.isEmpty()) {
            m_status->setText(QStringLiteral("Select a disk to check SMART."));
            return;
        }
        m_status->setText(QStringLiteral("Checking SMART for %1...").arg(row.path));
        m_provider.checkSmart(row);
    });
    connect(smartAllButton, &QPushButton::clicked, this, [this]() {
        // Manual Check All must honor the explicit user request even mid-check:
        // checkSmart() queues paths when a batch is already running. The busy guard
        // only belongs on the scheduled (automatic) path.
        const QVector<DiskRow> rows = m_model->rows();
        if (!rows.isEmpty()) {
            m_status->setText(QStringLiteral("Checking SMART for %1 disks...").arg(rows.size()));
            m_provider.checkSmart(rows);
        }
    });
    connect(historyButton, &QPushButton::clicked, this, [this]() {
        const DiskRow row = selectedRow();
        if (row.path.isEmpty()) {
            m_status->setText(QStringLiteral("Select a disk to view SMART history."));
            return;
        }
        auto* dialog = new QDialog(this);
        dialog->setWindowTitle(QStringLiteral("SMART history - %1").arg(row.path));
        dialog->resize(720, 420);
        auto* dialogLayout = new QVBoxLayout(dialog);
        auto* table = new QTableWidget(dialog);
        table->setColumnCount(5);
        table->setHorizontalHeaderLabels({QStringLiteral("Time"), QStringLiteral("Health"), QStringLiteral("Temp"),
                                          QStringLiteral("Reallocated"), QStringLiteral("Pending")});
        table->horizontalHeader()->setStretchLastSection(true);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->verticalHeader()->hide();
        const QVector<SmartHistoryEntry> entries = SmartHistoryStore::entriesForDisk(historyKeySerial(row.serial), row.path);
        table->setRowCount(entries.size());
        for (int i = 0; i < entries.size(); ++i) {
            const SmartHistoryEntry& entry = entries.at(i);
            table->setItem(i, 0, new QTableWidgetItem(entry.timestamp));
            table->setItem(i, 1, new QTableWidgetItem(entry.health));
            table->setItem(i, 2, new QTableWidgetItem(entry.temperature));
            table->setItem(i, 3, new QTableWidgetItem(entry.reallocated));
            table->setItem(i, 4, new QTableWidgetItem(entry.pending));
        }
        dialogLayout->addWidget(table);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    connect(&m_provider, &SmartProvider::disksReady, this, [this](const QVector<DiskRow>& rows, const QString& error) {
        m_model->setRows(rows);
        m_status->setText(error.isEmpty() ? QStringLiteral("%1 disks").arg(rows.size()) : error);
    });
    connect(&m_provider, &SmartProvider::smartReady, this, [this](const QString& path, const DiskRow& smartRow, const QString& error) {
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
            m_status->setText(QStringLiteral("SMART check complete for %1").arg(path));
        } else {
            m_status->setText(error);
        }
    });
    connect(m_smartScheduler, &SmartRefreshScheduler::smartRefreshRequested, this, &DisksPage::runScheduledSmartChecks);

    m_smartScheduler->reloadFromSettings();
    refresh();
}

DiskRow DisksPage::selectedRow() const {
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid())
        return {};
    return m_model->rowAt(index.row());
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
    // A serial reported by more than one visible disk cannot identify a single
    // drive, so fall back to the device path to avoid merging their histories.
    return count > 1 ? QString() : trimmed;
}

void DisksPage::reloadSmartSchedule() {
    m_smartScheduler->reloadFromSettings();
}

void DisksPage::runScheduledSmartChecks() {
    if (m_provider.isSmartCheckBusy()) {
        m_status->setText(QStringLiteral("SMART check already in progress; skipping scheduled refresh."));
        return;
    }
    const QVector<DiskRow> rows = m_model->rows();
    if (!rows.isEmpty()) {
        m_status->setText(QStringLiteral("Checking SMART for %1 disks...").arg(rows.size()));
        m_provider.checkSmart(rows);
    }
}

void DisksPage::refresh() {
    m_status->setText(QStringLiteral("Refreshing..."));
    m_provider.refreshDisks();
}
