#pragma once

#include <QString>
#include <QStringList>

namespace ServiceGroupSettings {

QStringList groupNames();
QString activeGroup();
void setActiveGroup(const QString& group);
QStringList servicesForGroup(const QString& group);
void setServicesForGroup(const QString& group, const QStringList& services);
void renameGroup(const QString& from, const QString& to);
void ensureDefaultGroup();

} // namespace ServiceGroupSettings
