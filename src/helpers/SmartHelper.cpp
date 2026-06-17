#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <cstdio>

namespace {

constexpr int UsageError = 64;
constexpr int Unavailable = 69;

struct SmartRequest {
    QString devicePath;
    QString transport;
};

void writeStderr(const QString &message)
{
    const QByteArray bytes = message.toLocal8Bit();
    std::fwrite(bytes.constData(), 1, bytes.size(), stderr);
    std::fwrite("\n", 1, 1, stderr);
}

bool isSupportedDevicePath(const QString &devicePath)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^/dev/(sd[a-z]+|hd[a-z]+|vd[a-z]+|xvd[a-z]+|nvme\d+(n\d+)?|mmcblk\d+)$)"));
    return pattern.match(devicePath).hasMatch();
}

bool isSupportedTransport(const QString &transport)
{
    static const QStringList transports{
        QString(),
        QStringLiteral("ata"),
        QStringLiteral("sata"),
        QStringLiteral("sas"),
        QStringLiteral("scsi"),
        QStringLiteral("usb"),
        QStringLiteral("nvme"),
    };
    return transports.contains(transport);
}

QString nvmeControllerPath(const QString &devicePath)
{
    static const QRegularExpression namespacePath(QStringLiteral(R"(^(/dev/nvme\d+)n\d+$)"));
    const QRegularExpressionMatch match = namespacePath.match(devicePath);
    return match.hasMatch() ? match.captured(1) : devicePath;
}

QStringList smartctlArguments(QString devicePath, const QString &transport)
{
    QStringList arguments{QStringLiteral("-j"), QStringLiteral("-H"), QStringLiteral("-A")};

    if (transport == QStringLiteral("nvme")) {
        devicePath = nvmeControllerPath(devicePath);
        arguments.append({QStringLiteral("-d"), QStringLiteral("nvme")});
    } else if (transport == QStringLiteral("usb")) {
        arguments.append({QStringLiteral("-d"), QStringLiteral("sat")});
    }

    arguments.append(devicePath);
    return arguments;
}

QJsonObject runSmartctl(const SmartRequest &request)
{
    QJsonObject result;
    result.insert(QStringLiteral("device"), request.devicePath);

    QProcess smartctl;
    smartctl.setProgram(QStringLiteral("smartctl"));
    smartctl.setArguments(smartctlArguments(request.devicePath, request.transport));
    smartctl.start();
    if (!smartctl.waitForStarted()) {
        result.insert(QStringLiteral("exit_code"), Unavailable);
        result.insert(QStringLiteral("start_failed"), true);
        result.insert(QStringLiteral("stderr"), QStringLiteral("smartctl not found. Install smartmontools."));
        return result;
    }
    if (!smartctl.waitForFinished(30000)) {
        smartctl.kill();
        smartctl.waitForFinished();
        result.insert(QStringLiteral("exit_code"), Unavailable);
        result.insert(QStringLiteral("timed_out"), true);
        result.insert(QStringLiteral("stderr"), QStringLiteral("smartctl timed out."));
        return result;
    }

    const QByteArray output = smartctl.readAllStandardOutput();
    QJsonParseError parseError;
    const QJsonDocument smartDocument = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error == QJsonParseError::NoError && smartDocument.isObject()) {
        result.insert(QStringLiteral("stdout"), smartDocument.object());
    } else {
        result.insert(QStringLiteral("stdout_text"), QString::fromLocal8Bit(output));
    }
    result.insert(QStringLiteral("stderr"), QString::fromLocal8Bit(smartctl.readAllStandardError()));
    result.insert(QStringLiteral("exit_code"), smartctl.exitCode());
    return result;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("linux-service-dashboard-smart-helper"));

    QVector<SmartRequest> requests;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg == QStringLiteral("--device") && i + 1 < args.size()) {
            SmartRequest request;
            request.devicePath = args.at(++i);
            requests.append(request);
        } else if (arg == QStringLiteral("--transport") && i + 1 < args.size()) {
            if (requests.isEmpty()) {
                writeStderr(QStringLiteral("--transport must follow --device."));
                return UsageError;
            }
            requests.last().transport = args.at(++i);
        } else {
            writeStderr(QStringLiteral("Usage: linux-service-dashboard-smart-helper --device /dev/DEVICE [--transport TRANSPORT]..."));
            return UsageError;
        }
    }

    if (requests.isEmpty()) {
        writeStderr(QStringLiteral("At least one --device is required."));
        return UsageError;
    }

    for (const SmartRequest &request : requests) {
        if (!isSupportedDevicePath(request.devicePath)) {
            writeStderr(QStringLiteral("Unsupported SMART device path: %1").arg(request.devicePath));
            return UsageError;
        }
        if (!isSupportedTransport(request.transport)) {
            writeStderr(QStringLiteral("Unsupported SMART transport: %1").arg(request.transport));
            return UsageError;
        }
        if (!QFileInfo::exists(request.devicePath)) {
            writeStderr(QStringLiteral("Device does not exist: %1").arg(request.devicePath));
            return UsageError;
        }
    }

    QJsonArray results;
    for (const SmartRequest &request : requests) {
        results.append(runSmartctl(request));
    }

    QJsonObject root;
    root.insert(QStringLiteral("results"), results);
    const QByteArray output = QJsonDocument(root).toJson(QJsonDocument::Compact);
    std::fwrite(output.constData(), 1, output.size(), stdout);
    std::fwrite("\n", 1, 1, stdout);
    return 0;
}
