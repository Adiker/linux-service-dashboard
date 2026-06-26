#include "MountProfileStore.h"

#include <QSettings>

namespace MountProfileStore {

namespace {

MountRow profileToRow(const QSettings& settings, const QString& name) {
    const QString prefix = QStringLiteral("mounts/profiles/%1/").arg(name);
    MountRow row;
    row.source = settings.value(prefix + QStringLiteral("source")).toString();
    row.target = settings.value(prefix + QStringLiteral("target")).toString();
    row.filesystemType = settings.value(prefix + QStringLiteral("filesystemType")).toString();
    row.options = settings.value(prefix + QStringLiteral("options")).toString();
    row.status = QStringLiteral("Profile: %1").arg(name);
    return row;
}

} // namespace

QStringList profileNames() {
    QSettings settings;
    return settings.value(QStringLiteral("mounts/profileNames")).toStringList();
}

QVector<MountRow> loadProfiles() {
    QSettings settings;
    QVector<MountRow> rows;
    for (const QString& name : profileNames()) {
        rows.append(profileToRow(settings, name));
    }
    return rows;
}

void saveProfile(const MountRow& row, const QString& name) {
    QSettings settings;
    QStringList names = settings.value(QStringLiteral("mounts/profileNames")).toStringList();
    if (!names.contains(name)) {
        names.append(name);
        settings.setValue(QStringLiteral("mounts/profileNames"), names);
    }
    const QString prefix = QStringLiteral("mounts/profiles/%1/").arg(name);
    settings.setValue(prefix + QStringLiteral("source"), row.source);
    settings.setValue(prefix + QStringLiteral("target"), row.target);
    settings.setValue(prefix + QStringLiteral("filesystemType"), row.filesystemType);
    settings.setValue(prefix + QStringLiteral("options"), row.options);
}

void removeProfile(const QString& name) {
    QSettings settings;
    QStringList names = settings.value(QStringLiteral("mounts/profileNames")).toStringList();
    names.removeAll(name);
    settings.setValue(QStringLiteral("mounts/profileNames"), names);
    settings.remove(QStringLiteral("mounts/profiles/%1").arg(name));
}

} // namespace MountProfileStore
