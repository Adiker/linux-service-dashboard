#include "SensorModel.h"

#include <QBrush>

SensorModel::SensorModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int SensorModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int SensorModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 6;
}

QVariant SensorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const auto &row = m_rows.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return row.chip;
        case 1: return row.label;
        case 2: return QString::number(row.current, 'f', 1);
        case 3: return row.high > 0.0 ? QString::number(row.high, 'f', 1) : QStringLiteral("-");
        case 4: return row.critical > 0.0 ? QString::number(row.critical, 'f', 1) : QStringLiteral("-");
        case 5: return row.status;
        default: return {};
        }
    }
    if (role == Qt::ForegroundRole && index.column() == 5) {
        if (row.status == QStringLiteral("Critical")) return QBrush(QColor(QStringLiteral("#ff8a80")));
        if (row.status == QStringLiteral("Warning")) return QBrush(QColor(QStringLiteral("#f0c36d")));
        if (row.status == QStringLiteral("OK")) return QBrush(QColor(QStringLiteral("#8fd694")));
    }
    return {};
}

QVariant SensorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    static const QStringList headers{
        QStringLiteral("Chip/device"),
        QStringLiteral("Sensor label"),
        QStringLiteral("Current"),
        QStringLiteral("High"),
        QStringLiteral("Critical"),
        QStringLiteral("Status"),
    };
    return headers.value(section);
}

void SensorModel::setRows(const QVector<SensorRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

QVector<SensorRow> SensorModel::rows() const
{
    return m_rows;
}
