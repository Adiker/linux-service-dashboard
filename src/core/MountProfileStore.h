#pragma once

#include "../models/MountModel.h"

#include <QVector>

namespace MountProfileStore {

QVector<MountRow> loadProfiles();
void saveProfile(const MountRow &row, const QString &name);
void removeProfile(const QString &name);
QStringList profileNames();

} // namespace MountProfileStore
