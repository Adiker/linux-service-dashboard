#include "NetworkProvider.h"

#include "../utils/TimeUtils.h"

NetworkProvider::NetworkProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context != QStringLiteral("vpn-active")) {
            return;
        }
        VpnStatus status;
        status.lastRefresh = currentTimestamp();
        QString error;
        if (result.ok()) {
            const QStringList lines = result.standardOutput.split('\n', Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const QStringList fields = line.split(':');
                if (fields.size() >= 4 && fields.at(1) == QStringLiteral("vpn")) {
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
        } else {
            error = result.startFailed ? QStringLiteral("nmcli not found. Install NetworkManager CLI tools.") : result.standardError.trimmed();
        }
        emit vpnStatusReady(status, error);
    });
}

void NetworkProvider::refreshVpnStatus()
{
    m_runner.run(QStringLiteral("nmcli"),
                 {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,TYPE,DEVICE,STATE"), QStringLiteral("connection"), QStringLiteral("show"), QStringLiteral("--active")},
                 12000,
                 QStringLiteral("vpn-active"));
}
