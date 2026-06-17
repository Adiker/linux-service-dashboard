#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

#include <cstdio>

namespace {

constexpr int UsageError = 64;
constexpr int Unavailable = 69;

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

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("linux-service-dashboard-smart-helper"));

    QString devicePath;
    QString transport;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg == QStringLiteral("--device") && i + 1 < args.size()) {
            devicePath = args.at(++i);
        } else if (arg == QStringLiteral("--transport") && i + 1 < args.size()) {
            transport = args.at(++i);
        } else {
            writeStderr(QStringLiteral("Usage: linux-service-dashboard-smart-helper --device /dev/DEVICE [--transport TRANSPORT]"));
            return UsageError;
        }
    }

    if (!isSupportedDevicePath(devicePath)) {
        writeStderr(QStringLiteral("Unsupported SMART device path: %1").arg(devicePath));
        return UsageError;
    }
    if (!isSupportedTransport(transport)) {
        writeStderr(QStringLiteral("Unsupported SMART transport: %1").arg(transport));
        return UsageError;
    }
    if (!QFileInfo::exists(devicePath)) {
        writeStderr(QStringLiteral("Device does not exist: %1").arg(devicePath));
        return UsageError;
    }

    QProcess smartctl;
    smartctl.setProgram(QStringLiteral("smartctl"));
    smartctl.setArguments(smartctlArguments(devicePath, transport));
    smartctl.start();
    if (!smartctl.waitForStarted()) {
        writeStderr(QStringLiteral("smartctl not found. Install smartmontools."));
        return Unavailable;
    }
    if (!smartctl.waitForFinished(30000)) {
        smartctl.kill();
        smartctl.waitForFinished();
        writeStderr(QStringLiteral("smartctl timed out."));
        return Unavailable;
    }

    const QByteArray output = smartctl.readAllStandardOutput();
    const QByteArray error = smartctl.readAllStandardError();
    std::fwrite(output.constData(), 1, output.size(), stdout);
    std::fwrite(error.constData(), 1, error.size(), stderr);

    return smartctl.exitCode();
}
