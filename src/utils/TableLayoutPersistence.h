#pragma once

#include <QString>

class QTableView;

namespace TableLayoutPersistence {

void restore(QTableView* table, const QString& settingsKey);
void save(QTableView* table, const QString& settingsKey);

// Persist column sizes/order for a table. Pass persistSortOrder = true only for
// tables whose view sorts through a QSortFilterProxyModel; tables bound directly
// to a non-sorting source model must not persist the sort indicator, since it
// would reappear on restart without the rows actually being reordered.
void bind(QTableView* table, const QString& settingsKey, bool persistSortOrder = false);

} // namespace TableLayoutPersistence
