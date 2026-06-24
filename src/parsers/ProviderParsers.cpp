#include "ProviderParsers.h"

#include "../utils/JsonUtils.h"
#include "../utils/StatusUtils.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace ProviderParsers {

namespace {

bool isVpnLikeConnectionType(const QString &type)
{
    return type == QStringLiteral("vpn")
        || type == QStringLiteral("tun")
        || type == QStringLiteral("wireguard")
        || type == QStringLiteral("ppp");
}

void collectMounts(const QJsonArray &array, QVector<MountRow> *rows)
{
    static const QSet<QString> interestingTypes{QStringLiteral("cifs"), QStringLiteral("nfs"), QStringLiteral("nfs4"), QStringLiteral("sshfs")};
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        const QString type = object.value(QStringLiteral("fstype")).toString();
        if (interestingTypes.contains(type)) {
            MountRow row;
            row.target = object.value(QStringLiteral("target")).toString();
            row.source = object.value(QStringLiteral("source")).toString();
            row.filesystemType = type;
            row.options = object.value(QStringLiteral("options")).toString();
            row.status = QStringLiteral("Mounted");
            rows->append(row);
        }
        if (object.contains(QStringLiteral("children"))) {
            collectMounts(object.value(QStringLiteral("children")).toArray(), rows);
        }
    }
}

} // namespace

QVector<ServiceRow> parseSystemdListUnits(const QString &output,
                                          const QStringList &watchedServices,
                                          const QString &timestamp,
                                          QString *error)
{
    QVector<ServiceRow> rows;
    if (error) {
        error->clear();
    }
    const QRegularExpression lineExpression(QStringLiteral(R"(^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.*)$)"));
    QSet<QString> seen;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const auto match = lineExpression.match(line.trimmed());
        if (!match.hasMatch() || !match.captured(1).endsWith(QStringLiteral(".service"))) {
            continue;
        }
        ServiceRow row;
        row.unit = match.captured(1);
        row.loadState = match.captured(2);
        row.activeState = match.captured(3);
        row.subState = match.captured(4);
        row.description = match.captured(5);
        row.lastRefresh = timestamp;
        if (watchedServices.isEmpty() || watchedServices.contains(row.unit)) {
            rows.append(row);
            seen.insert(row.unit);
        }
    }
    for (const QString &unit : watchedServices) {
        if (!seen.contains(unit)) {
            rows.append(ServiceRow{unit, QStringLiteral("unavailable"), QStringLiteral("unknown"), QStringLiteral("-"), QStringLiteral("Unit not found"), timestamp});
        }
    }
    return rows;
}

int parseSystemdFailedCount(const QString &output, QString *error)
{
    if (error) {
        error->clear();
    }
    int count = 0;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(QStringLiteral("UNIT "))) {
            continue;
        }
        if (line.contains(QStringLiteral(".service"))) {
            ++count;
        }
    }
    return count;
}

QVector<DockerContainerRow> parseDockerContainerLines(const QString &output, QString *error)
{
    QVector<DockerContainerRow> rows;
    if (error) {
        error->clear();
    }
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QString parseError;
        const QJsonDocument document = parseJsonDocument(line, &parseError);
        if (!document.isObject()) {
            continue;
        }
        const QJsonObject object = document.object();
        DockerContainerRow row;
        row.name = object.value(QStringLiteral("Names")).toString();
        row.image = object.value(QStringLiteral("Image")).toString();
        row.status = object.value(QStringLiteral("Status")).toString();
        row.state = object.value(QStringLiteral("State")).toString();
        row.ports = object.value(QStringLiteral("Ports")).toString();
        row.created = object.value(QStringLiteral("CreatedAt")).toString(object.value(QStringLiteral("Created")).toString());
        row.id = object.value(QStringLiteral("ID")).toString();
        rows.append(row);
    }
    return rows;
}

VpnStatus parseNmcliActiveVpnConnections(const QString &output, const QString &timestamp, QString *error)
{
    if (error) {
        error->clear();
    }
    VpnStatus status;
    status.lastRefresh = timestamp;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = line.split(':');
        if (fields.size() >= 4 && isVpnLikeConnectionType(fields.at(1))) {
            status.connected = true;
            status.connectionName = fields.at(0);
            status.device = fields.at(2);
            status.state = fields.at(3);
            break;
        }
    }
    if (!status.connected) {
        status.state = QStringLiteral("Disconnected");
    }
    return status;
}

QVector<MountRow> parseFindmntJson(const QString &output, QString *error)
{
    QVector<MountRow> rows;
    if (error) {
        error->clear();
    }
    QString parseError;
    const QJsonDocument document = parseJsonDocument(output, &parseError);
    if (!parseError.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Could not parse findmnt JSON: %1").arg(parseError);
        }
        return rows;
    }
    collectMounts(document.object().value(QStringLiteral("filesystems")).toArray(), &rows);
    return rows;
}

QVector<SensorRow> parseSensorsJson(const QString &output, QString *error)
{
    QVector<SensorRow> rows;
    if (error) {
        error->clear();
    }
    QString parseError;
    const QJsonDocument document = parseJsonDocument(output, &parseError);
    if (!parseError.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Could not parse sensors JSON: %1").arg(parseError);
        }
        return rows;
    }
    const QJsonObject root = document.object();
    for (auto chipIt = root.begin(); chipIt != root.end(); ++chipIt) {
        const QJsonObject chip = chipIt.value().toObject();
        for (auto featureIt = chip.begin(); featureIt != chip.end(); ++featureIt) {
            const QJsonObject feature = featureIt.value().toObject();
            double input = 0.0;
            double high = 0.0;
            double critical = 0.0;
            bool hasInput = false;
            for (auto valueIt = feature.begin(); valueIt != feature.end(); ++valueIt) {
                const QString key = valueIt.key();
                if (key.endsWith(QStringLiteral("_input"))) {
                    input = valueIt.value().toDouble();
                    hasInput = true;
                } else if (key.endsWith(QStringLiteral("_max")) || key.endsWith(QStringLiteral("_high"))) {
                    high = valueIt.value().toDouble();
                } else if (key.endsWith(QStringLiteral("_crit"))) {
                    critical = valueIt.value().toDouble();
                }
            }
            if (hasInput) {
                rows.append(SensorRow{chipIt.key(), featureIt.key(), input, high, critical, temperatureStatus(input, high, critical)});
            }
        }
    }
    return rows;
}

QVector<FstabEntry> parseFstab(const QString &content, QString *error)
{
    QVector<FstabEntry> entries;
    if (error) {
        error->clear();
    }
    const QStringList lines = content.split('\n', Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() < 4) {
            if (error) {
                *error = QStringLiteral("Invalid fstab line: %1").arg(line);
            }
            continue;
        }
        FstabEntry entry;
        entry.device = fields.at(0);
        entry.mountPoint = fields.at(1);
        entry.filesystemType = fields.at(2);
        entry.options = fields.at(3);
        if (fields.size() > 4) {
            entry.dump = fields.at(4).toInt();
        }
        if (fields.size() > 5) {
            entry.pass = fields.at(5).toInt();
        }
        entries.append(entry);
    }
    return entries;
}

} // namespace ProviderParsers
