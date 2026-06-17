#include "SmartProvider.h"

#include "../utils/JsonUtils.h"
#include "../utils/TimeUtils.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

QString smartctlMessage(const QJsonObject &root)
{
    const QJsonArray messages = root.value(QStringLiteral("smartctl")).toObject().value(QStringLiteral("messages")).toArray();
    QStringList parts;
    for (const QJsonValue &messageValue : messages) {
        const QString text = messageValue.toObject().value(QStringLiteral("string")).toString().trimmed();
        if (!text.isEmpty()) {
            parts.append(text);
        }
    }
    return parts.join(QStringLiteral("; "));
}

QString permissionAwareError(const QString &message)
{
    if (message.contains(QStringLiteral("permission"), Qt::CaseInsensitive)) {
        return QStringLiteral("smartctl needs permission for this device. Run the app as a user with disk access or check manually with sudo.");
    }
    return message;
}

QString smartHealthFromError(const QString &message)
{
    if (message.contains(QStringLiteral("permission"), Qt::CaseInsensitive)) {
        return QStringLiteral("Permission denied");
    }
    if (message.contains(QStringLiteral("not found"), Qt::CaseInsensitive)) {
        return QStringLiteral("smartctl missing");
    }
    return QStringLiteral("Check failed");
}

QString nvmeControllerPath(const QString &devicePath)
{
    static const QRegularExpression namespacePath(QStringLiteral(R"(^(/dev/nvme\d+)n\d+$)"));
    const QRegularExpressionMatch match = namespacePath.match(devicePath);
    return match.hasMatch() ? match.captured(1) : devicePath;
}

QStringList smartctlArgumentsForRow(const DiskRow &row)
{
    QString devicePath = row.path;
    QStringList arguments{QStringLiteral("-j"), QStringLiteral("-H"), QStringLiteral("-A")};

    if (row.transport == QStringLiteral("nvme")) {
        devicePath = nvmeControllerPath(devicePath);
        arguments.append({QStringLiteral("-d"), QStringLiteral("nvme")});
    } else if (row.transport == QStringLiteral("usb")) {
        arguments.append({QStringLiteral("-d"), QStringLiteral("sat")});
    }

    arguments.append(devicePath);
    return arguments;
}

} // namespace

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
            QString parseError;
            const QJsonDocument document = parseJsonDocument(result.standardOutput, &parseError);
            const QJsonObject root = document.object();
            const QString jsonMessage = smartctlMessage(root);
            if (result.ok() || result.exitCode == 4 || !root.isEmpty()) {
                if (!jsonMessage.isEmpty() && !result.ok() && result.exitCode != 4) {
                    error = permissionAwareError(jsonMessage);
                    smart.smartHealth = smartHealthFromError(jsonMessage);
                }
                const QJsonValue passed = root.value(QStringLiteral("smart_status")).toObject().value(QStringLiteral("passed"));
                if (!passed.isUndefined()) {
                    smart.smartHealth = passed.toBool() ? QStringLiteral("PASSED") : QStringLiteral("FAILED/Warning");
                }
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
                if (error.isEmpty()) {
                    error = result.errorString.trimmed();
                }
                error = permissionAwareError(error);
                smart.smartHealth = smartHealthFromError(error);
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
    DiskRow row;
    row.path = devicePath;
    checkSmart(row);
}

void SmartProvider::checkSmart(const DiskRow &row)
{
    if (row.path.isEmpty()) {
        return;
    }
    m_runner.run(QStringLiteral("smartctl"), smartctlArgumentsForRow(row), 30000, QStringLiteral("smart-check:%1").arg(row.path));
}

void SmartProvider::checkSmart(const QVector<DiskRow> &rows)
{
    for (const DiskRow &row : rows) {
        checkSmart(row);
    }
}
