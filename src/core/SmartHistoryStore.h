#pragma once

#include <QString>
#include <QVector>

struct SmartHistoryEntry {
    QString path;
    QString timestamp;
    QString health;
    QString temperature;
    QString reallocated;
    QString pending;
};

namespace SmartHistoryStore {

void appendEntry(const SmartHistoryEntry &entry);
QVector<SmartHistoryEntry> entriesForDisk(const QString &path, int maxEntries = 100);

} // namespace SmartHistoryStore
