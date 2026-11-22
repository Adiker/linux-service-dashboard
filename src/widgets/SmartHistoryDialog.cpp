#include "SmartHistoryDialog.h"

#include "../core/SmartHistoryStore.h"
#include "ConfirmActionDialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString csvEscape(const QString& field) {
    QString escaped = field;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

} // namespace

SmartHistoryDialog::SmartHistoryDialog(const QString& serialKey, const QString& devicePath, QWidget* parent)
    : QDialog(parent), m_serialKey(serialKey), m_devicePath(devicePath) {
    setWindowTitle(QStringLiteral("SMART history — %1").arg(devicePath));
    resize(780, 460);

    auto* layout = new QVBoxLayout(this);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Health"),
        QStringLiteral("Temp"),
        QStringLiteral("Reallocated / Wear"),
        QStringLiteral("Pending / Media Err"),
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    layout->addWidget(m_table, 1);

    auto* bottomRow = new QHBoxLayout;
    auto* exportButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")), QStringLiteral("Export CSV..."), this);
    auto* clearButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-clear")), QStringLiteral("Clear history"), this);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    bottomRow->addWidget(exportButton);
    bottomRow->addWidget(clearButton);
    bottomRow->addStretch();
    bottomRow->addWidget(buttons);
    layout->addLayout(bottomRow);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(exportButton, &QPushButton::clicked, this, &SmartHistoryDialog::exportCsv);
    connect(clearButton, &QPushButton::clicked, this, &SmartHistoryDialog::clearHistory);

    reload();
}

void SmartHistoryDialog::reload() {
    const QVector<SmartHistoryEntry> entries = SmartHistoryStore::entriesForDisk(m_serialKey, m_devicePath, 100);
    m_table->setRowCount(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        const SmartHistoryEntry& entry = entries.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(entry.timestamp));
        m_table->setItem(i, 1, new QTableWidgetItem(entry.health));
        m_table->setItem(i, 2, new QTableWidgetItem(entry.temperature));
        m_table->setItem(i, 3, new QTableWidgetItem(entry.reallocated));
        m_table->setItem(i, 4, new QTableWidgetItem(entry.pending));
    }
}

void SmartHistoryDialog::exportCsv() {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export SMART history"), QStringLiteral("smart-history.csv"),
                                                      QStringLiteral("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Export SMART history"), QStringLiteral("Could not open %1 for writing.").arg(path));
        return;
    }
    QStringList lines{QStringLiteral("Time,Health,Temp,ReallocatedOrWear,PendingOrMediaErrors")};
    const QVector<SmartHistoryEntry> entries = SmartHistoryStore::entriesForDisk(m_serialKey, m_devicePath, 100);
    for (const SmartHistoryEntry& entry : entries) {
        lines.append(QStringList{
            csvEscape(entry.timestamp),
            csvEscape(entry.health),
            csvEscape(entry.temperature),
            csvEscape(entry.reallocated),
            csvEscape(entry.pending),
        }
                         .join(QLatin1Char(',')));
    }
    file.write(lines.join(QLatin1Char('\n')).toUtf8());
    file.write("\n");
}

void SmartHistoryDialog::clearHistory() {
    if (!ConfirmActionDialog::confirm(this, QStringLiteral("Clear SMART history"),
                                      QStringLiteral("Remove all saved SMART history entries for %1?").arg(m_devicePath))) {
        return;
    }
    SmartHistoryStore::clearEntriesForDisk(m_serialKey, m_devicePath);
    reload();
}
