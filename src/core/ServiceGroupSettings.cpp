#include "ServiceGroupSettings.h"

#include <QSettings>

namespace ServiceGroupSettings {

namespace {

const QString kDefaultGroup = QStringLiteral("Default");
const QStringList kDefaultServices{
    QStringLiteral("docker.service"),
    QStringLiteral("NetworkManager.service"),
    QStringLiteral("sshd.service"),
    QStringLiteral("postgresql.service"),
    QStringLiteral("redis.service"),
};

QString groupKey(const QString &group)
{
    return QStringLiteral("systemd/group/%1").arg(group);
}

} // namespace

void ensureDefaultGroup()
{
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

QStringList groupNames()
{
    ensureDefaultGroup();
    QSettings settings;
    return settings.value(QStringLiteral("systemd/groups")).toStringList();
}

QString activeGroup()
{
    ensureDefaultGroup();
    QSettings settings;
    return settings.value(QStringLiteral("systemd/activeGroup"), kDefaultGroup).toString();
}

void setActiveGroup(const QString &group)
{
    QSettings settings;
    settings.setValue(QStringLiteral("systemd/activeGroup"), group);
}

QStringList servicesForGroup(const QString &group)
{
    ensureDefaultGroup();
    QSettings settings;
    return settings.value(groupKey(group), kDefaultServices).toStringList();
}

void setServicesForGroup(const QString &group, const QStringList &services)
{
    QSettings settings;
    QStringList groups = settings.value(QStringLiteral("systemd/groups")).toStringList();
    if (!groups.contains(group)) {
        groups.append(group);
        settings.setValue(QStringLiteral("systemd/groups"), groups);
    }
    settings.setValue(groupKey(group), services);
    settings.setValue(QStringLiteral("systemd/watchedServices"), services);
}

} // namespace ServiceGroupSettings
