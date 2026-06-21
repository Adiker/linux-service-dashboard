#pragma once

#include <QString>

class QTableView;

namespace TableLayoutPersistence {

void restore(QTableView *table, const QString &settingsKey);
void save(QTableView *table, const QString &settingsKey);
void bind(QTableView *table, const QString &settingsKey);

} // namespace TableLayoutPersistence
