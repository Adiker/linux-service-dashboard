#include "ServiceGroupSettings.h"

#include <QSettings>
#include <QUrl>

namespace ServiceGroupSettings {

namespace {

const QString kDefaultGroup = QStringLiteral("Default");
const QStringList kDefaultServices{
    QStringLiteral("docker.service"),     QStringLiteral("NetworkManager.service"), QStringLiteral("sshd.service"),
    QStringLiteral("postgresql.service"), QStringLiteral("redis.service"),
};

QString groupKey(const QString& group) {
    // Percent-encode the name so a group containing '/' (e.g. "prod/web") does not
    // create nested QSettings subgroups and collide with other group keys.
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(group));
    return QStringLiteral("systemd/group/%1").arg(encoded);
}

} // namespace

void ensureDefaultGroup() {
    QSettings settings;
    QStringList groups = settings.value(QStringLiteral("systemd/groups")).toStringList();
    if (groups.isEmpty()) {
        groups = {kDefaultGroup};
        settings.setValue(QStringLiteral("systemd/groups"), groups);
    }
    if (!settings.contains(groupKey(kDefaultGroup))) {
        const QStringList legacy = settings.value(QStringLiteral("systemd/watchedServices"), kDefaultServices).toStringList();
        settings.setValue(groupKey(kDefaultGroup), legacy);
    }
    if (!settings.contains(QStringLiteral("systemd/activeGroup"))) {
        settings.setValue(QStringLiteral("systemd/activeGroup"), kDefaultGroup);
    }
}

QStringList groupNames() {
    ensureDefaultGroup();
    QSettings settings;
    return settings.value(QStringLiteral("systemd/groups")).toStringList();
}

QString activeGroup() {
    ensureDefaultGroup();
    QSettings settings;
    return settings.value(QStringLiteral("systemd/activeGroup"), kDefaultGroup).toString();
}

void setActiveGroup(const QString& group) {
    QSettings settings;
    settings.setValue(QStringLiteral("systemd/activeGroup"), group);
}

QStringList servicesForGroup(const QString& group) {
    ensureDefaultGroup();
    QSettings settings;
    return settings.value(groupKey(group), kDefaultServices).toStringList();
}

void setServicesForGroup(const QString& group, const QStringList& services) {
    QSettings settings;
    QStringList groups = settings.value(QStringLiteral("systemd/groups")).toStringList();
    if (!groups.contains(group)) {
        groups.append(group);
        settings.setValue(QStringLiteral("systemd/groups"), groups);
    }
    settings.setValue(groupKey(group), services);
    settings.setValue(QStringLiteral("systemd/watchedServices"), services);
}

void renameGroup(const QString& from, const QString& to) {
    if (from.isEmpty() || to.isEmpty() || from == to) {
        return;
    }
    QSettings settings;
    const QStringList services = servicesForGroup(from);
    QStringList groups = settings.value(QStringLiteral("systemd/groups")).toStringList();
    const int index = groups.indexOf(from);
    if (index >= 0) {
        groups[index] = to;
    } else {
        groups.append(to);
    }
    groups.removeAll(from);
    settings.setValue(QStringLiteral("systemd/groups"), groups);
    settings.remove(groupKey(from));
    settings.setValue(groupKey(to), services);
    if (activeGroup() == from) {
        settings.setValue(QStringLiteral("systemd/activeGroup"), to);
    }
}

} // namespace ServiceGroupSettings
