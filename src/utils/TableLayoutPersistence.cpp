#include "TableLayoutPersistence.h"

#include <QHeaderView>
#include <QSettings>
#include <QTableView>

namespace TableLayoutPersistence {

void restore(QTableView *table, const QString &settingsKey)
{
    if (!table || !table->horizontalHeader()) {
        return;
    }
    QSettings settings;
    const QByteArray state = settings.value(settingsKey).toByteArray();
    if (!state.isEmpty()) {
        table->horizontalHeader()->restoreState(state);
    }
}

void save(QTableView *table, const QString &settingsKey)
{
    if (!table || !table->horizontalHeader()) {
        return;
    }
    QSettings settings;
    settings.setValue(settingsKey, table->horizontalHeader()->saveState());
}

void bind(QTableView *table, const QString &settingsKey)
{
    if (!table || !table->horizontalHeader()) {
        return;
    }
    restore(table, settingsKey);
    QHeaderView *header = table->horizontalHeader();
    const auto persist = [table, settingsKey]() { save(table, settingsKey); };
    QObject::connect(header, &QHeaderView::sectionResized, table, persist);
    QObject::connect(header, &QHeaderView::sortIndicatorChanged, table, persist);
}

} // namespace TableLayoutPersistence
