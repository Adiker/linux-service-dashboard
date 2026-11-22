#pragma once

#include <QSortFilterProxyModel>
#include <QString>
#include <functional>

class TableFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

  public:
    using RowPredicate = std::function<bool(int sourceRow, const QAbstractItemModel* model)>;

    explicit TableFilterProxyModel(QObject* parent = nullptr);

    void setSearchText(const QString& text);
    void setRowPredicate(RowPredicate predicate);

  protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;

  private:
    QString m_searchText;
    RowPredicate m_predicate;
};
