#include "TableFilterProxyModel.h"

#include <QAbstractItemModel>

TableFilterProxyModel::TableFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void TableFilterProxyModel::setSearchText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (m_searchText == trimmed) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
    m_searchText = trimmed;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    m_searchText = trimmed;
    invalidateFilter();
#endif
}

void TableFilterProxyModel::setRowPredicate(RowPredicate predicate)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
    m_predicate = std::move(predicate);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    m_predicate = std::move(predicate);
    invalidateFilter();
#endif
}

bool TableFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *model = sourceModel();
    if (!model) {
        return true;
    }

    if (m_predicate && !m_predicate(sourceRow, model)) {
        return false;
    }

    if (m_searchText.isEmpty()) {
        return true;
    }

    const int cols = model->columnCount(sourceParent);
    for (int col = 0; col < cols; ++col) {
        const QString text = model->index(sourceRow, col, sourceParent).data(Qt::DisplayRole).toString();
        if (text.contains(m_searchText, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool TableFilterProxyModel::lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const
{
    const QVariant leftUser = sourceModel()->data(sourceLeft, Qt::UserRole);
    const QVariant rightUser = sourceModel()->data(sourceRight, Qt::UserRole);
    if (leftUser.isValid() && rightUser.isValid()) {
        if (leftUser.typeId() == QMetaType::Double || rightUser.typeId() == QMetaType::Double) {
            return leftUser.toDouble() < rightUser.toDouble();
        }
        if (leftUser.typeId() == QMetaType::LongLong || rightUser.typeId() == QMetaType::LongLong) {
            return leftUser.toLongLong() < rightUser.toLongLong();
        }
        if (leftUser.typeId() == QMetaType::Int || rightUser.typeId() == QMetaType::Int) {
            return leftUser.toInt() < rightUser.toInt();
        }
    }
    return QString::localeAwareCompare(
               sourceModel()->data(sourceLeft, Qt::DisplayRole).toString().toLower(),
               sourceModel()->data(sourceRight, Qt::DisplayRole).toString().toLower())
        < 0;
}
