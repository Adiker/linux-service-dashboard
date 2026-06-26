#include "TableLayoutPersistence.h"

#include <QHeaderView>
#include <QSettings>
#include <QTableView>

namespace TableLayoutPersistence {

void restore(QTableView* table, const QString& settingsKey) {
    if (!table || !table->horizontalHeader()) {
        return;
    }
    QSettings settings;
    const QByteArray state = settings.value(settingsKey).toByteArray();
    if (!state.isEmpty()) {
        table->horizontalHeader()->restoreState(state);
    }
}

void save(QTableView* table, const QString& settingsKey) {
    if (!table || !table->horizontalHeader()) {
        return;
    }
    QSettings settings;
    settings.setValue(settingsKey, table->horizontalHeader()->saveState());
}

void bind(QTableView* table, const QString& settingsKey, bool persistSortOrder) {
    if (!table || !table->horizontalHeader()) {
        return;
    }
    restore(table, settingsKey);
    QHeaderView* header = table->horizontalHeader();
    if (!persistSortOrder) {
        // This table does not reorder rows (no sorting proxy), so drop any sort
        // indicator that a previously saved state may have restored; otherwise a
        // stale arrow would point at a column the rows are not actually sorted by.
        header->setSortIndicator(-1, Qt::AscendingOrder);
    }
    const auto persist = [table, settingsKey]() { save(table, settingsKey); };
    QObject::connect(header, &QHeaderView::sectionResized, table, persist);
    if (persistSortOrder) {
        QObject::connect(header, &QHeaderView::sortIndicatorChanged, table, persist);
    }
}

} // namespace TableLayoutPersistence
