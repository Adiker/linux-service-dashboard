#include "SmartHistoryStore.h"

#include <QSettings>
#include <QUrl>

namespace SmartHistoryStore {

namespace {

QString encodePath(const QString &path)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(path));
}

QString historyKey(const QString &path)
{
    return QStringLiteral("smart/history/%1").arg(encodePath(path));
}

SmartHistoryEntry decodeEntry(const QString &path, const QString &encoded)
{
    const QStringList parts = encoded.split('|');
    SmartHistoryEntry entry;
    entry.path = path;
    entry.timestamp = parts.value(0);
    entry.health = parts.value(1);
    entry.temperature = parts.value(2);
    entry.reallocated = parts.value(3);
    entry.pending = parts.value(4);
    return entry;
}

} // namespace

void appendEntry(const SmartHistoryEntry &entry)
{
    QSettings settings;
    const QString key = historyKey(entry.path);
    QStringList history = settings.value(key).toStringList();
    history.prepend(QStringLiteral("%1|%2|%3|%4|%5")
                        .arg(entry.timestamp, entry.health, entry.temperature, entry.reallocated, entry.pending));
    while (history.size() > 100) {
        history.removeLast();
    }
    settings.setValue(key, history);
}

QVector<SmartHistoryEntry> entriesForDisk(const QString &path, int maxEntries)
{
    QSettings settings;
    const QStringList history = settings.value(historyKey(path)).toStringList();
    QVector<SmartHistoryEntry> entries;
    entries.reserve(qMin(maxEntries, history.size()));
    for (const QString &encoded : history) {
        entries.append(decodeEntry(path, encoded));
        if (entries.size() >= maxEntries) {
            break;
        }
    }
    return entries;
}

} // namespace SmartHistoryStore
