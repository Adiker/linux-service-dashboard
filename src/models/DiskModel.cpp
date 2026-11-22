#include "DiskModel.h"

#include <QFontDatabase>
#include <QRegularExpression>
#include <utility>

namespace {

double parseHumanSizeBytes(const QString& sizeText) {
    static const QRegularExpression re(QStringLiteral(R"(^\s*(\d+(?:[.,]\d+)?)\s*([KMGTPE]?)i?B?\s*$)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(sizeText);
    if (!match.hasMatch()) {
        return -1.0;
    }
    QString numberStr = match.captured(1);
    numberStr.replace(QLatin1Char(','), QLatin1Char('.'));
    double value = numberStr.toDouble();
    const QString unit = match.captured(2).toUpper();
    if (unit == QStringLiteral("K"))
        value *= 1024.0;
    else if (unit == QStringLiteral("M"))
        value *= 1024.0 * 1024.0;
    else if (unit == QStringLiteral("G"))
        value *= 1024.0 * 1024.0 * 1024.0;
    else if (unit == QStringLiteral("T"))
        value *= 1024.0 * 1024.0 * 1024.0 * 1024.0;
    else if (unit == QStringLiteral("P"))
        value *= 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
    return value;
}

double parseNumericLeading(const QString& text, double fallback) {
    static const QRegularExpression re(QStringLiteral(R"((-?\d+(?:\.\d+)?))"));
    const QRegularExpressionMatch match = re.match(text);
    return match.hasMatch() ? match.captured(1).toDouble() : fallback;
}

} // namespace

DiskModel::DiskModel(QObject* parent) : QAbstractTableModel(parent) {}

int DiskModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

int DiskModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 11;
}

QVariant DiskModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const auto& row = m_rows.at(index.row());
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
            return row.smartHealth;
        case 6:
            return row.temperature;
        case 7:
            return row.reallocated;
        case 8:
            return row.pending;
        case 9:
            return row.powerOnHours;
        case 10:
            return row.lastCheck;
        default:
            return {};
        }
    }
    if (role == Qt::UserRole) {
        switch (index.column()) {
        case 3:
            return parseHumanSizeBytes(row.size);
        case 6:
            return parseNumericLeading(row.temperature, -999.0);
        case 7:
            return parseNumericLeading(row.reallocated, -1.0);
        case 8:
            return parseNumericLeading(row.pending, -1.0);
        case 9:
            return parseNumericLeading(row.powerOnHours, -1.0);
        default:
            return {};
        }
    }
    if (role == Qt::FontRole && (index.column() == 0 || index.column() == 2)) {
        return QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }
    return {};
}

QVariant DiskModel::headerData(int section, Qt::Orientation orientation, int role) const {
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
        QStringLiteral("Reallocated / Wear"),
        QStringLiteral("Pending / Media Err"),
        QStringLiteral("Power-on"),
        QStringLiteral("Last check"),
    };
    return headers.value(section);
}

void DiskModel::setRows(const QVector<DiskRow>& rows) {
    beginResetModel();
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
            if (row.path == previous.path) {
                match = &previous;
                break;
            }
        }
        if (!match && !row.serial.isEmpty() && serialOccurrences(merged, row.serial) == 1 && serialOccurrences(m_rows, row.serial) == 1) {
            for (const DiskRow& previous : std::as_const(m_rows)) {
                if (row.serial == previous.serial) {
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
            row.powerOnHours = match->powerOnHours;
            row.lastCheck = match->lastCheck;
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
            m_rows[i].powerOnHours = smartRow.powerOnHours;
            m_rows[i].lastCheck = smartRow.lastCheck;
            emit dataChanged(index(i, 5), index(i, 10));
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
