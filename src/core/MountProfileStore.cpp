#include "MountProfileStore.h"

#include <QSettings>
#include <QUrl>

namespace MountProfileStore {

namespace {

QString encodedProfileGroup(const QString &name)
{
    // Percent-encode profile names so names containing '/' (such as the default
    // target path "/mnt/nas") do not create nested QSettings subgroups or break removal.
    return QStringLiteral("mounts/profiles/%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(name)));
}

QString legacyProfileGroup(const QString &name)
{
    return QStringLiteral("mounts/profiles/%1").arg(name);
}

MountRow readProfileWithPrefix(const QSettings &settings, const QString &groupPrefix, const QString &name)
{
    const QString prefix = groupPrefix + QLatin1Char('/');
    MountRow row;
    row.source = settings.value(prefix + QStringLiteral("source")).toString();
    row.target = settings.value(prefix + QStringLiteral("target")).toString();
    row.filesystemType = settings.value(prefix + QStringLiteral("filesystemType")).toString();
    row.options = settings.value(prefix + QStringLiteral("options")).toString();
    row.status = QStringLiteral("Profile: %1").arg(name);
    return row;
}

MountRow profileToRow(const QSettings &settings, const QString &name)
{
    const QString encoded = encodedProfileGroup(name);
    if (settings.contains(encoded + QStringLiteral("/target")) || settings.contains(encoded + QStringLiteral("/source"))) {
        return readProfileWithPrefix(settings, encoded, name);
    }
    return readProfileWithPrefix(settings, legacyProfileGroup(name), name);
}

} // namespace

QStringList profileNames()
{
    QSettings settings;
    return settings.value(QStringLiteral("mounts/profileNames")).toStringList();
}

QVector<MountRow> loadProfiles()
{
    QSettings settings;
    QVector<MountRow> rows;
    for (const QString &name : profileNames()) {
        rows.append(profileToRow(settings, name));
    }
    return rows;
}

void saveProfile(const MountRow &row, const QString &name)
{
    QSettings settings;
    QStringList names = settings.value(QStringLiteral("mounts/profileNames")).toStringList();
    if (!names.contains(name)) {
        names.append(name);
        settings.setValue(QStringLiteral("mounts/profileNames"), names);
    }
    const QString encoded = encodedProfileGroup(name);
    const QString legacy = legacyProfileGroup(name);
    if (legacy != encoded) {
        settings.remove(legacy);
    }
    const QString prefix = encoded + QLatin1Char('/');
    settings.setValue(prefix + QStringLiteral("source"), row.source);
    settings.setValue(prefix + QStringLiteral("target"), row.target);
    settings.setValue(prefix + QStringLiteral("filesystemType"), row.filesystemType);
    settings.setValue(prefix + QStringLiteral("options"), row.options);
}

void removeProfile(const QString &name)
{
    QSettings settings;
    QStringList names = settings.value(QStringLiteral("mounts/profileNames")).toStringList();
    names.removeAll(name);
    settings.setValue(QStringLiteral("mounts/profileNames"), names);
    settings.remove(encodedProfileGroup(name));
    const QString legacy = legacyProfileGroup(name);
    if (legacy != encodedProfileGroup(name)) {
        settings.remove(legacy);
    }
}

} // namespace MountProfileStore
