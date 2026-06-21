#pragma once

#include "../models/MountModel.h"

#include <QString>
#include <QVector>

namespace FstabParser {

QVector<MountRow> parseFile(const QString &path, QString *error);

} // namespace FstabParser
