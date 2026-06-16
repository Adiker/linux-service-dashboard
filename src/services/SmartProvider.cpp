#include "SmartProvider.h"

#include "../utils/JsonUtils.h"
#include "../utils/TimeUtils.h"

#include <QJsonArray>
#include <QJsonObject>

SmartProvider::SmartProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("disk-list")) {
            QVector<DiskRow> rows;
            QString error;
            if (result.ok()) {
                QString parseError;
                const QJsonDocument document = parseJsonDocument(result.standardOutput, &parseError);
                const QJsonArray devices = document.object().value(QStringLiteral("blockdevices")).toArray();
                for (const QJsonValue &value : devices) {
                    const QJsonObject device = value.toObject();
                    if (device.value(QStringLiteral("type")).toString() != QStringLiteral("disk")) {
                        continue;
                    }
                    DiskRow row;
                    row.path = device.value(QStringLiteral("path")).toString();
                    row.model = device.value(QStringLiteral("model")).toString();
                    row.serial = device.value(QStringLiteral("serial")).toString();
                    row.size = device.value(QStringLiteral("size")).toString();
                    row.transport = device.value(QStringLiteral("tran")).toString();
                    rows.append(row);
                }
                if (!parseError.isEmpty()) {
                    error = QStringLiteral("Could not parse lsblk JSON: %1").arg(parseError);
                }
            } else {
                error = result.startFailed ? QStringLiteral("lsblk not found. Install util-linux.") : result.standardError.trimmed();
            }
            emit disksReady(rows, error);
        } else if (context.startsWith(QStringLiteral("smart-check:"))) {
            const QString path = context.mid(QStringLiteral("smart-check:").size());
            DiskRow smart;
            smart.lastCheck = currentTimestamp();
            QString error;
            if (result.ok() || result.exitCode == 4) {
                QString parseError;
                const QJsonDocument document = parseJsonDocument(result.standardOutput, &parseError);
                const QJsonObject root = document.object();
                smart.smartHealth = root.value(QStringLiteral("smart_status")).toObject().value(QStringLiteral("passed")).toBool()
                    ? QStringLiteral("PASSED")
                    : QStringLiteral("FAILED/Warning");
                const QJsonValue temp = root.value(QStringLiteral("temperature")).toObject().value(QStringLiteral("current"));
                if (!temp.isUndefined()) {
                    smart.temperature = QStringLiteral("%1 C").arg(temp.toInt());
                }
                const QJsonArray table = root.value(QStringLiteral("ata_smart_attributes")).toObject().value(QStringLiteral("table")).toArray();
                for (const QJsonValue &attributeValue : table) {
                    const QJsonObject attribute = attributeValue.toObject();
                    const QString name = attribute.value(QStringLiteral("name")).toString();
                    const QString raw = QString::number(attribute.value(QStringLiteral("raw")).toObject().value(QStringLiteral("value")).toVariant().toLongLong());
                    if (name == QStringLiteral("Reallocated_Sector_Ct")) {
                        smart.reallocated = raw;
                    } else if (name == QStringLiteral("Current_Pending_Sector")) {
                        smart.pending = raw;
                    }
                }
                if (!parseError.isEmpty()) {
                    error = QStringLiteral("Could not parse smartctl JSON: %1").arg(parseError);
                }
            } else {
                error = result.startFailed ? QStringLiteral("smartctl not found. Install smartmontools.") : result.standardError.trimmed();
                if (error.contains(QStringLiteral("permission"), Qt::CaseInsensitive)) {
                    error = QStringLiteral("smartctl needs permission for this device. Run the app as a user with disk access or check manually with sudo.");
                }
            }
            emit smartReady(path, smart, error);
        }
    });
}

void SmartProvider::refreshDisks()
{
    m_runner.run(QStringLiteral("lsblk"),
                 {QStringLiteral("-J"), QStringLiteral("-o"), QStringLiteral("NAME,PATH,SIZE,TYPE,MODEL,SERIAL,TRAN,MOUNTPOINTS")},
                 15000,
                 QStringLiteral("disk-list"));
}

void SmartProvider::checkSmart(const QString &devicePath)
{
    m_runner.run(QStringLiteral("smartctl"), {QStringLiteral("-j"), QStringLiteral("-H"), QStringLiteral("-A"), devicePath}, 30000, QStringLiteral("smart-check:%1").arg(devicePath));
}
