#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace SmartDeviceRules {

inline bool isSupportedDevicePath(const QString& devicePath) {
    static const QRegularExpression pattern(QStringLiteral(R"(^/dev/(sd[a-z]+|hd[a-z]+|vd[a-z]+|xvd[a-z]+|nvme\d+(n\d+)?|mmcblk\d+)$)"));
    return pattern.match(devicePath).hasMatch();
}

inline bool isSupportedTransport(const QString& transport) {
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

inline QString nvmeControllerPath(const QString& devicePath) {
    static const QRegularExpression namespacePath(QStringLiteral(R"(^(/dev/nvme\d+)n\d+$)"));
    const QRegularExpressionMatch match = namespacePath.match(devicePath);
    return match.hasMatch() ? match.captured(1) : devicePath;
}

inline QStringList smartctlArguments(QString devicePath, const QString& transport) {
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

} // namespace SmartDeviceRules
