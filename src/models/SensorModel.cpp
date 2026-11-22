#include "SensorModel.h"

#include <QBrush>

SensorModel::SensorModel(QObject* parent) : QAbstractTableModel(parent) {}

int SensorModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

int SensorModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 7;
}

QVariant SensorModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const auto& row = m_rows.at(index.row());
    if (role == Qt::UserRole)
        return row.chip + "/" + row.label;
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return row.chip;
        case 1:
            return row.label;
        case 2:
            return QString::number(row.current, 'f', 1);
        case 3:
            return row.high > 0.0 ? QString::number(row.high, 'f', 1) : QStringLiteral("-");
        case 4:
            return row.critical > 0.0 ? QString::number(row.critical, 'f', 1) : QStringLiteral("-");
        case 5:
            return row.status;
        case 6:
            return row.unit;
        default:
            return {};
        }
    }

    return {};
}

QVariant SensorModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    static const QStringList headers{
        tr("Chip/device"), tr("Sensor label"), tr("Current"), tr("High"), tr("Critical"), tr("Status"), tr("Unit"),
    };
    return headers.value(section);
}

void SensorModel::setRows(const QVector<SensorRow>& rows) {
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

QVector<SensorRow> SensorModel::rows() const {
    return m_rows;
}
