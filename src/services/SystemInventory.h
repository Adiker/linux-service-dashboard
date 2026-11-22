#pragma once
#include "../core/CenterTypes.h"
namespace SystemInventory {
QStringList dataRoots();
CenterSnapshot applications(const QStringList& roots, const QString& locale, const QString& desktop);
CenterSnapshot local(const QString& module);
QStringList headers(const QString& module);
QVector<CenterRecord> parse(const QString& module, const QString& tag, const QString& output, QString* error);
} // namespace SystemInventory
