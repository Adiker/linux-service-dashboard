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

bool isNetworkFilesystem(const QString &type)
{
    static const QSet<QString> types{
        QStringLiteral("cifs"),
        QStringLiteral("nfs"),
        QStringLiteral("nfs4"),
        QStringLiteral("sshfs"),
        QStringLiteral("fuse.sshfs"),
    };
    return types.contains(type);
}

QString decodeFstabField(const QString &field)
{
    QString decoded;
    decoded.reserve(field.size());
    for (int i = 0; i < field.size(); ++i) {
        if (field.at(i) == QLatin1Char('\\') && i + 1 < field.size()) {
            const QChar next = field.at(i + 1);
            if (next.isDigit()) {
                int j = i + 1;
                QString octal;
                while (j < field.size() && j < i + 4 && field.at(j).isDigit()) {
                    octal += field.at(j);
                    ++j;
                }
                decoded += QChar(octal.toInt(nullptr, 8));
                i = j - 1;
                continue;
            }
            decoded += next;
            ++i;
            continue;
        }
        decoded += field.at(i);
    }
    return decoded;
}

void collectMountsRecursive(const QJsonArray &array, QVector<MountRow> *networkRows, QSet<QString> *allTargets)
{
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        const QString type = object.value(QStringLiteral("fstype")).toString();
        const QString target = object.value(QStringLiteral("target")).toString();

        if (networkRows && isNetworkFilesystem(type)) {
            MountRow row;
            row.target = target;
            row.source = object.value(QStringLiteral("source")).toString();
            row.filesystemType = type;
            row.options = object.value(QStringLiteral("options")).toString();
            row.status = QStringLiteral("Mounted");
            networkRows->append(row);
        }

        if (allTargets) {
            // Ignore autofs placeholders: with x-systemd.automount the target carries an
            // autofs mount until first access, and treating that as "occupied" would hide
            // the configured fstab/profile row for a share that is not actually mounted.
            const bool isAutofs = (type == QStringLiteral("autofs"));
            if (!target.isEmpty() && !isAutofs) {
                allTargets->insert(target);
            }
        }

        if (object.contains(QStringLiteral("children"))) {
            collectMountsRecursive(object.value(QStringLiteral("children")).toArray(), networkRows, allTargets);
        }
    }
}

} // namespace

bool isVpnLikeConnectionType(const QString &type)
{
    return type == QStringLiteral("vpn")
        || type == QStringLiteral("tun")
        || type == QStringLiteral("wireguard")
        || type == QStringLiteral("ppp");
}

QVector<ServiceRow> parseSystemdListUnits(const QString &output,
                                          const QStringList &watchedServices,
                                          const QString &timestamp,
                                          QString *error)
{
    QVector<ServiceRow> rows;
    if (error) {
        error->clear();
    }
    static const QRegularExpression lineExpression(QStringLiteral(R"(^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.*)$)"));
    QSet<QString> seen;
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
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
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
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
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
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

QVector<VpnStatus> parseNmcliActiveVpnList(const QString &output, const QString &timestamp, QString *error)
{
    if (error) {
        error->clear();
    }
    QVector<VpnStatus> result;
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = line.split(QLatin1Char(':'));
        if (fields.size() >= 4 && isVpnLikeConnectionType(fields.at(1))) {
            VpnStatus status;
            status.lastRefresh = timestamp;
            status.connected = true;
            status.connectionName = fields.at(0);
            status.device = fields.at(2).isEmpty() ? QStringLiteral("-") : fields.at(2);
            status.state = fields.at(3);
            result.append(status);
        }
    }
    return result;
}

VpnStatus parseNmcliActiveVpnConnections(const QString &output, const QString &timestamp, QString *error)
{
    const QVector<VpnStatus> list = parseNmcliActiveVpnList(output, timestamp, error);
    if (!list.isEmpty()) {
        return list.first();
    }
    VpnStatus status;
    status.lastRefresh = timestamp;
    status.connected = false;
    status.state = QStringLiteral("Disconnected");
    return status;
}

QVector<VpnProfile> parseNmcliVpnProfiles(const QString &output, QString *error)
{
    if (error) {
        error->clear();
    }
    QVector<VpnProfile> profiles;
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = line.split(QLatin1Char(':'));
        if (fields.size() >= 2 && isVpnLikeConnectionType(fields.at(1))) {
            VpnProfile profile;
            profile.name = fields.at(0);
            profile.type = fields.at(1);
            profile.device = (fields.size() > 2 && !fields.at(2).isEmpty()) ? fields.at(2) : QStringLiteral("-");
            profile.state = (fields.size() > 3 && !fields.at(3).isEmpty()) ? fields.at(3) : QStringLiteral("disconnected");
            profile.active = (profile.state == QStringLiteral("activated") || profile.state == QStringLiteral("activating"));
            profiles.append(profile);
        }
    }
    return profiles;
}

void parseSmartctlJson(const QJsonObject &root, DiskRow *row)
{
    if (!row || root.isEmpty()) {
        return;
    }

    const QJsonValue passed = root.value(QStringLiteral("smart_status")).toObject().value(QStringLiteral("passed"));
    if (!passed.isUndefined()) {
        row->smartHealth = passed.toBool() ? QStringLiteral("PASSED") : QStringLiteral("FAILED/Warning");
    }

    const QJsonValue temp = root.value(QStringLiteral("temperature")).toObject().value(QStringLiteral("current"));
    if (!temp.isUndefined()) {
        row->temperature = QStringLiteral("%1 C").arg(temp.toInt());
    }

    const QJsonValue powerOn = root.value(QStringLiteral("power_on_time")).toObject().value(QStringLiteral("hours"));
    if (!powerOn.isUndefined()) {
        row->powerOnHours = QStringLiteral("%1 h").arg(powerOn.toVariant().toLongLong());
    }

    const QJsonArray table = root.value(QStringLiteral("ata_smart_attributes")).toObject().value(QStringLiteral("table")).toArray();
    for (const QJsonValue &attributeValue : table) {
        const QJsonObject attribute = attributeValue.toObject();
        const QString name = attribute.value(QStringLiteral("name")).toString();
        const QString raw = QString::number(attribute.value(QStringLiteral("raw")).toObject().value(QStringLiteral("value")).toVariant().toLongLong());
        if (name == QStringLiteral("Reallocated_Sector_Ct")) {
            row->reallocated = raw;
        } else if (name == QStringLiteral("Current_Pending_Sector")) {
            row->pending = raw;
        } else if (name == QStringLiteral("Power_On_Hours") && row->powerOnHours == QStringLiteral("-")) {
            row->powerOnHours = QStringLiteral("%1 h").arg(raw);
        }
    }

    const QJsonObject nvmeLog = root.value(QStringLiteral("nvme_smart_health_information_log")).toObject();
    if (!nvmeLog.isEmpty()) {
        if (row->temperature == QStringLiteral("-") && nvmeLog.contains(QStringLiteral("temperature"))) {
            row->temperature = QStringLiteral("%1 C").arg(nvmeLog.value(QStringLiteral("temperature")).toInt());
        }
        if (nvmeLog.contains(QStringLiteral("percentage_used"))) {
            row->reallocated = QStringLiteral("%1% wear").arg(nvmeLog.value(QStringLiteral("percentage_used")).toInt());
        }
        if (nvmeLog.contains(QStringLiteral("media_errors"))) {
            row->pending = QStringLiteral("%1 err").arg(nvmeLog.value(QStringLiteral("media_errors")).toVariant().toLongLong());
        }
        if (row->powerOnHours == QStringLiteral("-") && nvmeLog.contains(QStringLiteral("power_on_hours"))) {
            row->powerOnHours = QStringLiteral("%1 h").arg(nvmeLog.value(QStringLiteral("power_on_hours")).toVariant().toLongLong());
        }
        if (nvmeLog.value(QStringLiteral("critical_warning")).toInt(0) != 0) {
            row->smartHealth = QStringLiteral("FAILED/Warning");
        }
    }
}

void parseFindmntSnapshot(const QString &output,
                          QVector<MountRow> *networkRows,
                          QSet<QString> *allTargets,
                          QString *error)
{
    if (error) {
        error->clear();
    }
    QString parseError;
    const QJsonDocument document = parseJsonDocument(output, &parseError);
    if (!parseError.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Could not parse findmnt JSON: %1").arg(parseError);
        }
        return;
    }
    collectMountsRecursive(document.object().value(QStringLiteral("filesystems")).toArray(), networkRows, allTargets);
}

QVector<MountRow> parseFindmntJson(const QString &output, QString *error)
{
    QVector<MountRow> rows;
    parseFindmntSnapshot(output, &rows, nullptr, error);
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

QVector<SensorRow> parseSensorsText(const QString &output, QString *error)
{
    QVector<SensorRow> rows;
    if (error) {
        error->clear();
    }

    static const QRegularExpression readingRe(
        QStringLiteral(R"(^([^:]+):\s*([+-]?\d+(?:\.\d+)?)\s*(?:°|C|°C|RPM|V|W|A|mW|%)(.*)$)"));
    static const QRegularExpression highRe(QStringLiteral(R"(\b(?:high|max)\s*=\s*([+-]?\d+(?:\.\d+)?))"));
    static const QRegularExpression critRe(QStringLiteral(R"(\bcrit\s*=\s*([+-]?\d+(?:\.\d+)?))"));

    QString currentChip;
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            currentChip.clear();
            continue;
        }
        if (line.startsWith(QStringLiteral("Adapter:"))) {
            continue;
        }
        if (!line.contains(QLatin1Char(':'))) {
            currentChip = line;
            continue;
        }
        const QRegularExpressionMatch match = readingRe.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        const QString label = match.captured(1).trimmed();
        const double input = match.captured(2).toDouble();
        const QString tail = match.captured(3);
        double high = 0.0;
        double critical = 0.0;
        const QRegularExpressionMatch highMatch = highRe.match(tail);
        if (highMatch.hasMatch()) {
            high = highMatch.captured(1).toDouble();
        }
        const QRegularExpressionMatch critMatch = critRe.match(tail);
        if (critMatch.hasMatch()) {
            critical = critMatch.captured(1).toDouble();
        }
        rows.append(SensorRow{
            currentChip.isEmpty() ? QStringLiteral("unknown") : currentChip,
            label,
            input,
            high,
            critical,
            temperatureStatus(input, high, critical),
        });
    }
    return rows;
}

QVector<FstabEntry> parseFstab(const QString &content, QString *error)
{
    QVector<FstabEntry> entries;
    if (error) {
        error->clear();
    }
    static const QRegularExpression whitespaceRe(QStringLiteral("\\s+"));
    const QStringList lines = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList fields = line.split(whitespaceRe, Qt::SkipEmptyParts);
        if (fields.size() < 4) {
            if (error) {
                *error = QStringLiteral("Invalid fstab line: %1").arg(line);
            }
            continue;
        }
        FstabEntry entry;
        entry.device = decodeFstabField(fields.at(0));
        entry.mountPoint = decodeFstabField(fields.at(1));
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

QVector<MountRow> parseFstabNetworkMounts(const QString &content, QString *error)
{
    QVector<MountRow> rows;
    const QVector<FstabEntry> entries = parseFstab(content, error);
    for (const FstabEntry &entry : entries) {
        // Legacy SSHFS fstab form: "sshfs#user@host:/path /mnt fuse ...", where the
        // type is the generic "fuse" and the source carries the sshfs# prefix.
        const bool isSshfsFuse = entry.filesystemType == QStringLiteral("fuse")
            && entry.device.startsWith(QStringLiteral("sshfs#"));
        if (!isNetworkFilesystem(entry.filesystemType) && !isSshfsFuse) {
            continue;
        }
        MountRow row;
        row.source = entry.device;
        row.target = entry.mountPoint;
        row.filesystemType = isSshfsFuse ? QStringLiteral("sshfs") : entry.filesystemType;
        row.options = entry.options;
        row.status = QStringLiteral("Configured (fstab)");
        rows.append(row);
    }
    return rows;
}

} // namespace ProviderParsers
