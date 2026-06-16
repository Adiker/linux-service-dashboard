#include "DockerContainerModel.h"

#include <QBrush>

DockerContainerModel::DockerContainerModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int DockerContainerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int DockerContainerModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 7;
}

QVariant DockerContainerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const auto &row = m_rows.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return row.name;
        case 1: return row.image;
        case 2: return row.status;
        case 3: return row.state;
        case 4: return row.ports;
        case 5: return row.created;
        case 6: return row.id;
        default: return {};
        }
    }
    if (role == Qt::ForegroundRole && index.column() == 3) {
        return QBrush(row.state == QStringLiteral("running") ? QColor(QStringLiteral("#8fd694")) : QColor(QStringLiteral("#f0c36d")));
    }
    return {};
}

QVariant DockerContainerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    static const QStringList headers{
        QStringLiteral("Name"),
        QStringLiteral("Image"),
        QStringLiteral("Status"),
        QStringLiteral("State"),
        QStringLiteral("Ports"),
        QStringLiteral("Created"),
        QStringLiteral("ID"),
    };
    return headers.value(section);
}

void DockerContainerModel::setRows(const QVector<DockerContainerRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

DockerContainerRow DockerContainerModel::rowAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row) : DockerContainerRow{};
}
