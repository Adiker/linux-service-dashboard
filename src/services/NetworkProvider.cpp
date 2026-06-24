#include "NetworkProvider.h"

#include "../parsers/ProviderParsers.h"
#include "../utils/TimeUtils.h"

NetworkProvider::NetworkProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context != QStringLiteral("vpn-active")) {
            return;
        }
        QString error;
        VpnStatus status;
        if (result.ok()) {
            status = ProviderParsers::parseNmcliActiveVpnConnections(result.standardOutput, currentTimestamp(), nullptr);
        } else {
            error = result.startFailed ? QStringLiteral("nmcli not found. Install NetworkManager CLI tools.") : result.standardError.trimmed();
            status.lastRefresh = currentTimestamp();
            status.state = QStringLiteral("Unknown");
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
