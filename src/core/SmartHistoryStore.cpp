#include "SmartHistoryStore.h"

#include <QSettings>
#include <QUrl>

namespace SmartHistoryStore {

namespace {

QString encodeComponent(const QString &value)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

QString diskIdentityKey(const QString &serial, const QString &path)
{
    const QString trimmedSerial = serial.trimmed();
    if (!trimmedSerial.isEmpty() && trimmedSerial != QStringLiteral("-")) {
        return QStringLiteral("smart/history/serial/%1").arg(encodeComponent(trimmedSerial));
    }
    return QStringLiteral("smart/history/path/%1").arg(encodeComponent(path));
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
    const QString key = diskIdentityKey(entry.serial, entry.path);
    QStringList history = settings.value(key).toStringList();
    history.prepend(QStringLiteral("%1|%2|%3|%4|%5")
                        .arg(entry.timestamp, entry.health, entry.temperature, entry.reallocated, entry.pending));
    while (history.size() > 100) {
        history.removeLast();
    }
    settings.setValue(key, history);
}

QVector<SmartHistoryEntry> entriesForDisk(const QString &serial, const QString &path, int maxEntries)
{
    QSettings settings;
    const QStringList history = settings.value(diskIdentityKey(serial, path)).toStringList();
    QVector<SmartHistoryEntry> entries;
    entries.reserve(qMin(maxEntries, history.size()));
    for (const QString &encoded : history) {
        SmartHistoryEntry entry = decodeEntry(path, encoded);
        entry.serial = serial;
        entries.append(entry);
        if (entries.size() >= maxEntries) {
            break;
        }
    }
    return entries;
}

} // namespace SmartHistoryStore
