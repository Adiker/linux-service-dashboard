#include "SmartProvider.h"

#include "../utils/JsonUtils.h"
#include "../utils/TimeUtils.h"

#include <QCoreApplication>
#include <QFileInfo>
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

QString permissionMessage()
{
    return QStringLiteral("smartctl needs permission for this device. Authenticate with polkit or check manually with sudo.");
}

QString permissionAwareError(const QString &message)
{
    if (message.contains(QStringLiteral("permission"), Qt::CaseInsensitive)) {
        return permissionMessage();
    }
    return message;
}

bool isPermissionError(const QString &message)
{
    return message.contains(QStringLiteral("permission"), Qt::CaseInsensitive);
}

QString smartHealthFromError(const QString &message)
{
    if (isPermissionError(message)) {
        return QStringLiteral("Permission denied");
    }
    if (message.contains(QStringLiteral("not authorized"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("dismissed"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("canceled"), Qt::CaseInsensitive)) {
        return QStringLiteral("Authorization failed");
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

QString smartCheckContext(const QString &mode, const QString &devicePath)
{
    return QStringLiteral("smart-check:%1:%2").arg(mode, devicePath);
}

bool parseSmartCheckContext(const QString &context, QString *mode, QString *devicePath)
{
    constexpr auto prefix = "smart-check:";
    if (!context.startsWith(QLatin1String(prefix))) {
        return false;
    }
    const QString rest = context.mid(QString::fromLatin1(prefix).size());
    const int separator = rest.indexOf(QLatin1Char(':'));
    if (separator <= 0) {
        return false;
    }
    *mode = rest.left(separator);
    *devicePath = rest.mid(separator + 1);
    return !devicePath->isEmpty();
}

QString smartHelperPath()
{
#ifdef SMART_HELPER_PATH
    const QString installedPath = QStringLiteral(SMART_HELPER_PATH);
#else
    const QString installedPath = QStringLiteral("/usr/libexec/linux-service-dashboard-smart-helper");
#endif
    if (QFileInfo::exists(installedPath)) {
        return installedPath;
    }

    const QString buildTreePath = QCoreApplication::applicationDirPath() + QStringLiteral("/linux-service-dashboard-smart-helper");
    return QFileInfo::exists(buildTreePath) ? buildTreePath : installedPath;
}

struct SmartParseResult {
    DiskRow row;
    QString error;
    bool permissionDenied = false;
};

SmartParseResult parseSmartResult(const CommandResult &result)
{
    SmartParseResult parsed;
    parsed.row.lastCheck = currentTimestamp();

    QString parseError;
    const QJsonDocument document = parseJsonDocument(result.standardOutput, &parseError);
    const QJsonObject root = document.object();
    const QString jsonMessage = smartctlMessage(root);
    const QString stderrMessage = result.standardError.trimmed();
    const QString fallbackMessage = stderrMessage.isEmpty() ? result.errorString.trimmed() : stderrMessage;
    const QString combinedMessage = QStringList{jsonMessage, fallbackMessage}.join(QLatin1Char(' '));
    parsed.permissionDenied = isPermissionError(combinedMessage);

    if (result.ok() || result.exitCode == 4 || !root.isEmpty()) {
        if (!jsonMessage.isEmpty() && !result.ok() && result.exitCode != 4) {
            parsed.error = permissionAwareError(jsonMessage);
            parsed.row.smartHealth = smartHealthFromError(jsonMessage);
        }
        const QJsonValue passed = root.value(QStringLiteral("smart_status")).toObject().value(QStringLiteral("passed"));
        if (!passed.isUndefined()) {
            parsed.row.smartHealth = passed.toBool() ? QStringLiteral("PASSED") : QStringLiteral("FAILED/Warning");
        }
        const QJsonValue temp = root.value(QStringLiteral("temperature")).toObject().value(QStringLiteral("current"));
        if (!temp.isUndefined()) {
            parsed.row.temperature = QStringLiteral("%1 C").arg(temp.toInt());
        }
        const QJsonArray table = root.value(QStringLiteral("ata_smart_attributes")).toObject().value(QStringLiteral("table")).toArray();
        for (const QJsonValue &attributeValue : table) {
            const QJsonObject attribute = attributeValue.toObject();
            const QString name = attribute.value(QStringLiteral("name")).toString();
            const QString raw = QString::number(attribute.value(QStringLiteral("raw")).toObject().value(QStringLiteral("value")).toVariant().toLongLong());
            if (name == QStringLiteral("Reallocated_Sector_Ct")) {
                parsed.row.reallocated = raw;
            } else if (name == QStringLiteral("Current_Pending_Sector")) {
                parsed.row.pending = raw;
            }
        }
        if (!parseError.isEmpty()) {
            parsed.error = QStringLiteral("Could not parse smartctl JSON: %1").arg(parseError);
        }
    } else {
        if (result.startFailed) {
            parsed.error = result.program == QStringLiteral("pkexec")
                ? QStringLiteral("pkexec not found. Install polkit to run SMART checks with authorization.")
                : QStringLiteral("smartctl not found. Install smartmontools.");
        } else {
            parsed.error = fallbackMessage;
        }
        parsed.error = permissionAwareError(parsed.error);
        parsed.row.smartHealth = smartHealthFromError(parsed.error);
    }

    return parsed;
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
        } else {
            QString mode;
            QString path;
            if (!parseSmartCheckContext(context, &mode, &path)) {
                return;
            }

            const SmartParseResult parsed = parseSmartResult(result);
            const DiskRow activeRow = m_activeSmartChecks.value(path);
            if (mode == QStringLiteral("direct") && parsed.permissionDenied && !activeRow.path.isEmpty() && runPrivilegedSmartCheck(activeRow)) {
                return;
            }

            emit smartReady(path, parsed.row, parsed.error);
            m_activeSmartChecks.remove(path);
            m_smartCheckRunning = false;
            startNextSmartCheck();
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
    m_pendingSmartChecks.enqueue(row);
    startNextSmartCheck();
}

void SmartProvider::checkSmart(const QVector<DiskRow> &rows)
{
    for (const DiskRow &row : rows) {
        if (!row.path.isEmpty()) {
            m_pendingSmartChecks.enqueue(row);
        }
    }
    startNextSmartCheck();
}

void SmartProvider::startNextSmartCheck()
{
    if (m_smartCheckRunning || m_pendingSmartChecks.isEmpty()) {
        return;
    }

    const DiskRow row = m_pendingSmartChecks.dequeue();
    m_activeSmartChecks.insert(row.path, row);
    m_smartCheckRunning = true;
    runDirectSmartCheck(row);
}

void SmartProvider::runDirectSmartCheck(const DiskRow &row)
{
    m_runner.run(QStringLiteral("smartctl"), smartctlArgumentsForRow(row), 30000, smartCheckContext(QStringLiteral("direct"), row.path));
}

bool SmartProvider::runPrivilegedSmartCheck(const DiskRow &row)
{
    const QString helperPath = smartHelperPath();
    if (!QFileInfo::exists(helperPath)) {
        return false;
    }

    m_runner.run(QStringLiteral("pkexec"),
                 {helperPath, QStringLiteral("--device"), row.path, QStringLiteral("--transport"), row.transport},
                 45000,
                 smartCheckContext(QStringLiteral("helper"), row.path));
    return true;
}
