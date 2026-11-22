#include "DiskModel.h"

#include <QBrush>
#include <utility>

DiskModel::DiskModel(QObject* parent) : QAbstractTableModel(parent) {}

int DiskModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

int DiskModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 10;
}

QVariant DiskModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const auto& row = m_rows.at(index.row());
    if (role == Qt::ToolTipRole && index.column() == 5)
        return row.smartDetail.isEmpty() && row.smartHealth == "Not checked" ? tr("Use Check SMART or Check All SMART to read disk health.")
                                                                             : row.smartDetail;
    if (role == Qt::UserRole)
        return row.path;
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return row.path;
        case 1:
            return row.model;
        case 2:
            return row.serial;
        case 3:
            return row.size;
        case 4:
            return row.transport;
        case 5:
            if (row.smartHealth == "Not checked")
                return tr("Not checked");
            if (row.smartHealth == "Unknown")
                return tr("Unknown");
            if (row.smartHealth == "Permission denied")
                return tr("Permission denied");
            if (row.smartHealth == "Authorization failed")
                return tr("Authorization failed");
            if (row.smartHealth == "Check failed")
                return tr("Check failed");
            if (row.smartHealth == "Health unavailable")
                return tr("Health unavailable");
            return row.smartHealth;
        case 6:
            return row.temperature;
        case 7:
            return row.reallocated;
        case 8:
            return row.pending;
        case 9:
            return row.lastCheck == "Never" ? tr("Never") : row.lastCheck;
        default:
            return {};
        }
    }

    return {};
}

QVariant DiskModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    static const QStringList headers{
        tr("Device path"),  tr("Model"), tr("Serial"),      tr("Size"),    tr("Transport"),
        tr("SMART health"), tr("Temp"),  tr("Reallocated"), tr("Pending"), tr("Last check"),
    };
    return headers.value(section);
}

void DiskModel::setRows(const QVector<DiskRow>& rows) {
    beginResetModel();
    // Refreshing the inventory re-runs lsblk, which carries no SMART data. Carry
    // previously fetched SMART values over to disks that are still present so a
    // refresh does not blank the table and force the user to re-authenticate.
    //
    // Match by device path only while serial/model/transport still identify the
    // same disk. Only fall back to the serial when it is non-empty
    // and unique in both the old and new inventories; disks that report a shared
    // or duplicated serial must never inherit another device's SMART data.
    const auto serialOccurrences = [](const QVector<DiskRow>& list, const QString& serial) {
        int count = 0;
        for (const DiskRow& row : list) {
            if (!row.serial.isEmpty() && row.serial == serial) {
                ++count;
            }
        }
        return count;
    };

    QVector<DiskRow> merged = rows;
    for (DiskRow& row : merged) {
        const DiskRow* match = nullptr;
        for (const DiskRow& previous : std::as_const(m_rows)) {
            if (row.path == previous.path && row.serial == previous.serial && row.model == previous.model &&
                row.transport == previous.transport) {
                match = &previous;
                break;
            }
        }
        if (!match && !row.serial.isEmpty() && serialOccurrences(merged, row.serial) == 1 && serialOccurrences(m_rows, row.serial) == 1) {
            for (const DiskRow& previous : std::as_const(m_rows)) {
                if (row.serial == previous.serial && row.model == previous.model && row.transport == previous.transport) {
                    match = &previous;
                    break;
                }
            }
        }
        if (match) {
            row.smartHealth = match->smartHealth;
            row.temperature = match->temperature;
            row.reallocated = match->reallocated;
            row.pending = match->pending;
            row.lastCheck = match->lastCheck;
            row.smartDetail = match->smartDetail;
        }
    }
    m_rows = merged;
    endResetModel();
}

void DiskModel::updateSmart(const QString& path, const DiskRow& smartRow) {
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].path == path) {
            m_rows[i].smartHealth = smartRow.smartHealth;
            m_rows[i].temperature = smartRow.temperature;
            m_rows[i].reallocated = smartRow.reallocated;
            m_rows[i].pending = smartRow.pending;
            m_rows[i].lastCheck = smartRow.lastCheck;
            m_rows[i].smartDetail = smartRow.smartDetail;
            emit dataChanged(index(i, 5), index(i, 9));
            return;
        }
    }
}

DiskRow DiskModel::rowAt(int row) const {
    return row >= 0 && row < m_rows.size() ? m_rows.at(row) : DiskRow{};
}

QVector<DiskRow> DiskModel::rows() const {
    return m_rows;
}
