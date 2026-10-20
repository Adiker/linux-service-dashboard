#pragma once
class QTableView;
class QVBoxLayout;
namespace TableTools {
void install(QTableView* table, QVBoxLayout* layout, bool addFilter = true);
int sourceRow(QTableView* table);
} // namespace TableTools
