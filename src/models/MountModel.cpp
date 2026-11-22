#include "MountModel.h"

MountModel::MountModel(QObject* parent) : QAbstractTableModel(parent) {}

int MountModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

int MountModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 5;
}

QVariant MountModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const auto& row = m_rows.at(index.row());
    if (role == Qt::UserRole)
        return row.target;
    if (role != Qt::DisplayRole)
        return {};
    switch (index.column()) {
    case 0:
        return row.target;
    case 1:
        return row.source;
    case 2:
        return row.filesystemType;
    case 3:
        return row.options;
    case 4:
        return row.status == "Mounted" ? tr("Mounted") : row.status == "Configured (fstab)" ? tr("Configured (fstab)") : row.status;
    default:
        return {};
    }
}

QVariant MountModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal) {
        return {};
    }
    static const QStringList headers{
        tr("Target"), tr("Source"), tr("Filesystem type"), tr("Options"), tr("Status"),
    };
    return headers.value(section);
}

void MountModel::setRows(const QVector<MountRow>& rows) {
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

MountRow MountModel::rowAt(int row) const {
    return row >= 0 && row < m_rows.size() ? m_rows.at(row) : MountRow{};
}
