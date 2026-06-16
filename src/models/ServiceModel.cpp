#include "ServiceModel.h"

#include <QBrush>

ServiceModel::ServiceModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ServiceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int ServiceModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 6;
}

QVariant ServiceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const auto &row = m_rows.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return row.unit;
        case 1: return row.loadState;
        case 2: return row.activeState;
        case 3: return row.subState;
        case 4: return row.description;
        case 5: return row.lastRefresh;
        default: return {};
        }
    }
    if (role == Qt::ForegroundRole && index.column() == 2) {
        if (row.activeState == QStringLiteral("active")) return QBrush(QColor(QStringLiteral("#8fd694")));
        if (row.activeState == QStringLiteral("failed")) return QBrush(QColor(QStringLiteral("#ff8a80")));
        return QBrush(QColor(QStringLiteral("#f0c36d")));
    }
    return {};
}

QVariant ServiceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    static const QStringList headers{
        QStringLiteral("Unit"),
        QStringLiteral("Load state"),
        QStringLiteral("Active state"),
        QStringLiteral("Sub state"),
        QStringLiteral("Description"),
        QStringLiteral("Last refresh"),
    };
    return headers.value(section);
}

void ServiceModel::setRows(const QVector<ServiceRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

ServiceRow ServiceModel::rowAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row) : ServiceRow{};
}
