#include "DisksPage.h"

#include "../core/SmartHistoryStore.h"
#include "../core/SmartRefreshScheduler.h"
#include "../ui/StatusBadgeDelegate.h"
#include "../utils/TableLayoutPersistence.h"
#include "../utils/TableTools.h"
#include "../utils/TimeUtils.h"
#include "SmartHistoryDialog.h"

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

DisksPage::DisksPage(SmartProvider& provider, QWidget* parent)
    : QWidget(parent), m_provider(provider), m_smartScheduler(new SmartRefreshScheduler(this)) {
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("Disks / SMART"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto* refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh"), this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(refreshButton);
    layout->addLayout(header);

    auto* smartHint =
        new QLabel(tr("SMART is read manually. Use Check SMART or Check All SMART; Refresh only updates the disk list."), this);
    smartHint->setWordWrap(true);
    smartHint->setObjectName(QStringLiteral("smartHint"));
    layout->addWidget(smartHint);

    m_model = new DiskModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setSortingEnabled(true);
    m_table->setColumnWidth(0, 250);
    m_table->setItemDelegateForColumn(5, new StatusBadgeDelegate(m_table));
    TableLayoutPersistence::bind(m_table, QStringLiteral("tables/disks/headerState"));
    layout->addWidget(m_table, 1);

    auto* actions = new QHBoxLayout;
    auto* smartButton = new QPushButton(QIcon::fromTheme(QStringLiteral("drive-harddisk")), tr("Check SMART"), this);
    auto* smartAllButton = new QPushButton(QIcon::fromTheme(QStringLiteral("drive-harddisk")), tr("Check All SMART"), this);
    auto* historyButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-list-details")), tr("History"), this);
    actions->addWidget(smartButton);
    actions->addWidget(smartAllButton);
    actions->addWidget(historyButton);
    actions->addStretch();
    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    actions->addWidget(m_status);
    layout->addLayout(actions);

    connect(refreshButton, &QPushButton::clicked, this, &DisksPage::refresh);
    connect(smartButton, &QPushButton::clicked, this, [this]() {
        const DiskRow row = selectedRow();
        if (row.path.isEmpty()) {
            m_status->setText(tr("Select a disk to check SMART."));
            return;
        }
        m_status->setText(tr("Checking SMART for %1...").arg(row.path));
        m_provider.checkSmart(row);
    });
    connect(smartAllButton, &QPushButton::clicked, this, [this]() {
        // Manual Check All must honor the explicit user request even mid-check:
        // checkSmart() queues paths when a batch is already running. The busy guard
        // only belongs on the scheduled (automatic) path.
        const QVector<DiskRow> rows = m_model->rows();
        if (!rows.isEmpty()) {
            m_status->setText(tr("Checking SMART for %1 disks...").arg(rows.size()));
            m_provider.checkSmart(rows);
        }
    });
    connect(historyButton, &QPushButton::clicked, this, [this]() {
        const DiskRow row = selectedRow();
        if (row.path.isEmpty()) {
            m_status->setText(tr("Select a disk to view SMART history."));
            return;
        }
        auto* dialog = new SmartHistoryDialog(historyKeySerial(row.serial), row.path, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    connect(&m_provider, &SmartProvider::disksReady, this, [this](const QVector<DiskRow>& rows, const QString& error) {
        if (error.isEmpty())
            m_model->setRows(rows);
        m_status->setText(error.isEmpty() ? tr("%1 disks").arg(rows.size()) : error);
    });
    connect(&m_provider, &SmartProvider::smartReady, this, [this](const QString& path, const DiskRow& smartRow, const QString& error) {
        auto displayed = smartRow;
        if (!error.isEmpty())
            displayed.smartDetail = error;
        m_model->updateSmart(path, displayed);
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
            m_status->setText(tr("SMART check complete for %1").arg(path));
        } else {
            m_status->setText(error);
        }
    });
    connect(m_smartScheduler, &SmartRefreshScheduler::smartRefreshRequested, this, &DisksPage::runScheduledSmartChecks);

    m_smartScheduler->reloadFromSettings();
    refresh();
    TableTools::install(m_table, layout, true);
}

DiskRow DisksPage::selectedRow() const {
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid())
        return {};
    return m_model->rowAt(TableTools::sourceRow(m_table));
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
        m_status->setText(tr("SMART check already in progress; skipping scheduled refresh."));
        return;
    }
    const QVector<DiskRow> rows = m_model->rows();
    if (!rows.isEmpty()) {
        m_status->setText(tr("Checking SMART for %1 disks...").arg(rows.size()));
        m_provider.checkSmart(rows);
    }
}

void DisksPage::refresh() {
    m_status->setText(tr("Refreshing..."));
    m_provider.refreshDisks();
}
