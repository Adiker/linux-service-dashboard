#include "DiskModel.h"

#include <QBrush>

#include <utility>

DiskModel::DiskModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int DiskModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int DiskModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 10;
}

QVariant DiskModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const auto &row = m_rows.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return row.path;
        case 1: return row.model;
        case 2: return row.serial;
        case 3: return row.size;
        case 4: return row.transport;
        case 5: return row.smartHealth;
        case 6: return row.temperature;
        case 7: return row.reallocated;
        case 8: return row.pending;
        case 9: return row.lastCheck;
        default: return {};
        }
    }
    if (role == Qt::ForegroundRole && index.column() == 5) {
        const QString health = row.smartHealth.toLower();
        if (health.contains(QStringLiteral("passed")) || health.contains(QStringLiteral("ok"))) return QBrush(QColor(QStringLiteral("#8fd694")));
        if (health.contains(QStringLiteral("fail")) || health.contains(QStringLiteral("error"))) return QBrush(QColor(QStringLiteral("#ff8a80")));
        return QBrush(QColor(QStringLiteral("#aeb7c2")));
    }
    return {};
}

QVariant DiskModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    static const QStringList headers{
        QStringLiteral("Device path"),
        QStringLiteral("Model"),
        QStringLiteral("Serial"),
        QStringLiteral("Size"),
        QStringLiteral("Transport"),
        QStringLiteral("SMART health"),
        QStringLiteral("Temp"),
        QStringLiteral("Reallocated"),
        QStringLiteral("Pending"),
        QStringLiteral("Last check"),
    };
    return headers.value(section);
}

void DiskModel::setRows(const QVector<DiskRow> &rows)
{
    beginResetModel();
    // Refreshing the inventory re-runs lsblk, which carries no SMART data. Carry
    // previously fetched SMART values over to disks that are still present so a
    // refresh does not blank the table and force the user to re-authenticate.
    QVector<DiskRow> merged = rows;
    for (DiskRow &row : merged) {
        for (const DiskRow &previous : std::as_const(m_rows)) {
            const bool sameSerial = !row.serial.isEmpty() && row.serial == previous.serial;
            const bool samePathWithoutSerial = row.serial.isEmpty() && row.path == previous.path;
            if (sameSerial || samePathWithoutSerial) {
                row.smartHealth = previous.smartHealth;
                row.temperature = previous.temperature;
                row.reallocated = previous.reallocated;
                row.pending = previous.pending;
                row.lastCheck = previous.lastCheck;
                break;
            }
        }
    }
    m_rows = merged;
    endResetModel();
}

void DiskModel::updateSmart(const QString &path, const DiskRow &smartRow)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].path == path) {
            m_rows[i].smartHealth = smartRow.smartHealth;
            m_rows[i].temperature = smartRow.temperature;
            m_rows[i].reallocated = smartRow.reallocated;
            m_rows[i].pending = smartRow.pending;
            m_rows[i].lastCheck = smartRow.lastCheck;
            emit dataChanged(index(i, 5), index(i, 9));
            return;
        }
    }
}

DiskRow DiskModel::rowAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row) : DiskRow{};
}

QVector<DiskRow> DiskModel::rows() const
{
    return m_rows;
}
