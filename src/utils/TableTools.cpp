#include "TableTools.h"

#include <QCoreApplication>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>
void TableTools::install(QTableView* table, QVBoxLayout* layout, bool addFilter) {
    if (addFilter) {
        auto* proxy = new QSortFilterProxyModel(table);
        proxy->setSourceModel(table->model());
        proxy->setFilterKeyColumn(-1);
        proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
        table->setModel(proxy);
        auto* filter = new QLineEdit;
        filter->setPlaceholderText(QCoreApplication::translate("TableTools", "Search this view…"));
        layout->insertWidget(layout->indexOf(table), filter);
        QObject::connect(filter, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);
    }
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    auto* details = new QPlainTextEdit;
    details->setReadOnly(true);
    details->setMaximumHeight(95);
    details->setPlaceholderText(QCoreApplication::translate("TableTools", "Select an item to see all fields"));
    layout->insertWidget(layout->indexOf(table) + 1, details);
    QObject::connect(table->selectionModel(), &QItemSelectionModel::currentChanged, details, [table, details]() {
        const auto index = table->currentIndex();
        QStringList lines;
        if (index.isValid())
            for (int c = 0; c < table->model()->columnCount(); ++c)
                lines << table->model()->headerData(c, Qt::Horizontal).toString() + ": " +
                             table->model()->index(index.row(), c).data().toString();
        details->setPlainText(lines.join('\n'));
    });
}
int TableTools::sourceRow(QTableView* table) {
    auto index = table->currentIndex();
    while (const auto* proxy = qobject_cast<const QAbstractProxyModel*>(index.model()))
        index = proxy->mapToSource(index);
    return index.isValid() ? index.row() : -1;
}
