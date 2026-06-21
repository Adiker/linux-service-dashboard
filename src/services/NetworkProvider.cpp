#include "NetworkProvider.h"

#include "../utils/TimeUtils.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>

namespace {

constexpr int kDBusTimeoutMs = 5000;

bool isVpnLikeConnectionType(const QString &type)
{
    return type == QStringLiteral("vpn")
        || type == QStringLiteral("tun")
        || type == QStringLiteral("wireguard")
        || type == QStringLiteral("ppp");
}

QString readActiveConnectionProperty(const QDBusObjectPath &path, const char *property)
{
    QDBusInterface properties(QStringLiteral("org.freedesktop.NetworkManager"),
                            path.path(),
                            QStringLiteral("org.freedesktop.DBus.Properties"),
                            QDBusConnection::systemBus());
    const QDBusReply<QVariant> reply = properties.call(QStringLiteral("Get"),
                                                         QStringLiteral("org.freedesktop.NetworkManager.Connection.Active"),
                                                         QString(property));
    return reply.isValid() ? reply.value().toString() : QString();
}

quint32 readActiveConnectionState(const QDBusObjectPath &path)
{
    QDBusInterface properties(QStringLiteral("org.freedesktop.NetworkManager"),
                            path.path(),
                            QStringLiteral("org.freedesktop.DBus.Properties"),
                            QDBusConnection::systemBus());
    const QDBusReply<QVariant> reply = properties.call(QStringLiteral("Get"),
                                                         QStringLiteral("org.freedesktop.NetworkManager.Connection.Active"),
                                                         QStringLiteral("State"));
    return reply.isValid() ? reply.value().toUInt() : 0U;
}

QString stateLabel(quint32 state)
{
    switch (state) {
    case 2:
        return QStringLiteral("activated");
    case 1:
        return QStringLiteral("activating");
    case 3:
        return QStringLiteral("deactivating");
    default:
        return QStringLiteral("unknown");
    }
}

VpnStatus parseNmcliFallback(const QString &output, const QString &timestamp)
{
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
            return status;
        }
    }
    status.state = QStringLiteral("Disconnected");
    return status;
}

VpnStatus vpnStatusFromActiveConnections(const QList<QDBusObjectPath> &paths)
{
    VpnStatus status;
    status.lastRefresh = currentTimestamp();
    for (const QDBusObjectPath &path : paths) {
        const QString type = readActiveConnectionProperty(path, "Type");
        if (!isVpnLikeConnectionType(type)) {
            continue;
        }
        status.connected = true;
        status.connectionName = readActiveConnectionProperty(path, "Id");
        status.state = stateLabel(readActiveConnectionState(path));
        status.device = QStringLiteral("-");
        return status;
    }
    status.connected = false;
    status.state = QStringLiteral("Disconnected");
    status.connectionName = QStringLiteral("None");
    status.device = QStringLiteral("-");
    return status;
}

} // namespace

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
            status = parseNmcliFallback(result.standardOutput, currentTimestamp());
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
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        refreshVpnViaNmcli();
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.NetworkManager"),
                                                            QStringLiteral("/org/freedesktop/NetworkManager"),
                                                            QStringLiteral("org.freedesktop.DBus.Properties"),
                                                            QStringLiteral("Get"));
    message << QStringLiteral("org.freedesktop.NetworkManager") << QStringLiteral("ActiveConnections");

    QDBusPendingCall pending = bus.asyncCall(message, kDBusTimeoutMs);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &NetworkProvider::handleActiveConnectionsReply);
}

void NetworkProvider::handleActiveConnectionsReply(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();
    QDBusPendingReply<QVariant> reply = *watcher;
    if (reply.isError()) {
        refreshVpnViaNmcli();
        return;
    }

    const QList<QDBusObjectPath> paths = qdbus_cast<QList<QDBusObjectPath>>(reply.value());
    emit vpnStatusReady(vpnStatusFromActiveConnections(paths), QString());
}

void NetworkProvider::refreshVpnViaNmcli()
{
    m_runner.run(QStringLiteral("nmcli"),
                 {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,TYPE,DEVICE,STATE"), QStringLiteral("connection"), QStringLiteral("show"), QStringLiteral("--active")},
                 12000,
                 QStringLiteral("vpn-active"));
}
