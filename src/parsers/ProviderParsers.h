#pragma once

#include "../models/DockerContainerModel.h"
#include "../models/MountModel.h"
#include "../models/SensorModel.h"
#include "../models/ServiceModel.h"
#include "../services/NetworkProvider.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace ProviderParsers {

QVector<ServiceRow> parseSystemdListUnits(const QString &output,
                                            const QStringList &watchedServices,
                                            const QString &timestamp,
                                            QString *error);

int parseSystemdFailedCount(const QString &output, QString *error);

QVector<DockerContainerRow> parseDockerContainerLines(const QString &output, QString *error);

VpnStatus parseNmcliActiveVpnConnections(const QString &output, const QString &timestamp, QString *error);

QVector<MountRow> parseFindmntJson(const QString &output, QString *error);

QVector<SensorRow> parseSensorsJson(const QString &output, QString *error);

struct FstabEntry {
    QString device;
    QString mountPoint;
    QString filesystemType;
    QString options;
    int dump = 0;
    int pass = 0;
};

QVector<FstabEntry> parseFstab(const QString &content, QString *error);

} // namespace ProviderParsers
