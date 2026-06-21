#include "NetworkProvider.h"

#include "../utils/TimeUtils.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>

namespace {

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

bool refreshVpnViaDBus(VpnStatus *status, QString *error)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        if (error) {
            *error = QStringLiteral("System DBus unavailable.");
        }
        return false;
    }

    QDBusInterface manager(QStringLiteral("org.freedesktop.NetworkManager"),
                           QStringLiteral("/org/freedesktop/NetworkManager"),
                           QStringLiteral("org.freedesktop.NetworkManager"),
                           bus);
    if (!manager.isValid()) {
        if (error) {
            *error = QStringLiteral("NetworkManager DBus service not available.");
        }
        return false;
    }

    QDBusInterface properties(QStringLiteral("org.freedesktop.NetworkManager"),
                              QStringLiteral("/org/freedesktop/NetworkManager"),
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              bus);
    const QDBusReply<QVariant> activeReply = properties.call(QStringLiteral("Get"),
                                                               QStringLiteral("org.freedesktop.NetworkManager"),
                                                               QStringLiteral("ActiveConnections"));
    if (!activeReply.isValid()) {
        if (error) {
            *error = activeReply.error().message();
        }
        return false;
    }

    const QList<QDBusObjectPath> paths = qdbus_cast<QList<QDBusObjectPath>>(activeReply.value());
    status->lastRefresh = currentTimestamp();
    for (const QDBusObjectPath &path : paths) {
        const QString type = readActiveConnectionProperty(path, "Type");
        if (!isVpnLikeConnectionType(type)) {
            continue;
        }
        status->connected = true;
        status->connectionName = readActiveConnectionProperty(path, "Id");
        status->state = stateLabel(readActiveConnectionState(path));
        status->device = QStringLiteral("-");
        return true;
    }

    status->connected = false;
    status->state = QStringLiteral("Disconnected");
    status->connectionName = QStringLiteral("None");
    status->device = QStringLiteral("-");
    return true;
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
    VpnStatus status;
    QString error;
    if (refreshVpnViaDBus(&status, &error)) {
        emit vpnStatusReady(status, QString());
        return;
    }

    m_runner.run(QStringLiteral("nmcli"),
                 {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,TYPE,DEVICE,STATE"), QStringLiteral("connection"), QStringLiteral("show"), QStringLiteral("--active")},
                 12000,
                 QStringLiteral("vpn-active"));
}
