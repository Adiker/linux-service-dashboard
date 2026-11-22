#include "NetworkProvider.h"

#include "../parsers/ProviderParsers.h"
#include "../utils/TimeUtils.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>
#include <memory>

namespace {

constexpr int kDBusTimeoutMs = 5000;
constexpr quint32 kStateActivating = 1;
constexpr quint32 kStateActivated = 2;

QString stateLabel(quint32 state) {
    switch (state) {
    case kStateActivated:
        return QStringLiteral("activated");
    case kStateActivating:
        return QStringLiteral("activating");
    case 3:
        return QStringLiteral("deactivating");
    default:
        return QStringLiteral("unknown");
    }
}

struct ActiveVpnCandidate {
    QString id;
    QString device = QStringLiteral("-");
    quint32 state = 0;
};

struct ActiveConnectionsBatch {
    int remaining = 0;
    QString timestamp;
    QVector<ActiveVpnCandidate> candidates;
};

QVector<VpnStatus> buildActiveVpnList(const QVector<ActiveVpnCandidate>& candidates, const QString& timestamp) {
    QVector<VpnStatus> list;
    for (const ActiveVpnCandidate& candidate : candidates) {
        if (candidate.state != kStateActivated && candidate.state != kStateActivating) {
            continue;
        }
        VpnStatus status;
        status.connected = (candidate.state == kStateActivated);
        status.connectionName = candidate.id;
        status.device = candidate.device;
        status.state = stateLabel(candidate.state);
        status.lastRefresh = timestamp;
        list.append(status);
    }
    return list;
}

VpnStatus selectPrimaryVpnStatus(const QVector<VpnStatus>& activeList, const QString& timestamp) {
    for (const VpnStatus& status : activeList) {
        if (status.connected) {
            return status;
        }
    }
    if (!activeList.isEmpty()) {
        return activeList.first();
    }
    VpnStatus status;
    status.lastRefresh = timestamp;
    status.connected = false;
    status.state = QStringLiteral("Disconnected");
    status.connectionName = QStringLiteral("None");
    status.device = QStringLiteral("-");
    return status;
}

} // namespace

NetworkProvider::NetworkProvider(QObject* parent) : QObject(parent) {
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString&, const CommandResult& result, const QString& context) {
        if (context == QStringLiteral("vpn-active")) {
            QString error;
            QVector<VpnStatus> activeList;
            const QString timestamp = currentTimestamp();
            VpnStatus primary;
            if (result.ok()) {
                activeList = ProviderParsers::parseNmcliActiveVpnList(result.standardOutput, timestamp, nullptr);
                primary = selectPrimaryVpnStatus(activeList, timestamp);
            } else {
                error = result.startFailed ? QStringLiteral("nmcli not found. Install NetworkManager CLI tools.")
                                           : result.standardError.trimmed();
                primary.lastRefresh = timestamp;
                primary.state = QStringLiteral("Unknown");
            }
            emit activeVpnsReady(activeList, error);
            emit vpnStatusReady(primary, error);
        } else if (context == QStringLiteral("vpn-profiles")) {
            QString error;
            QVector<VpnProfile> profiles;
            if (result.ok()) {
                profiles = ProviderParsers::parseNmcliVpnProfiles(result.standardOutput, nullptr);
            } else {
                error = result.startFailed ? QStringLiteral("nmcli not found. Install NetworkManager CLI tools.")
                                           : result.standardError.trimmed();
            }
            emit vpnProfilesReady(profiles, error);
        } else if (context.startsWith(QStringLiteral("vpn-action:"))) {
            const QString action = context.section(QLatin1Char(':'), 1, 1);
            const QString name = context.section(QLatin1Char(':'), 2);
            const QString message = result.ok() ? QStringLiteral("VPN %1 (%2) succeeded.").arg(action, name)
                                                : QStringLiteral("VPN %1 (%2) failed.").arg(action, name);
            emit actionFinished(message, result.ok() ? result.standardOutput : result.standardError);
        }
    });
}

void NetworkProvider::refreshVpnStatus() {
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        refreshVpnViaNmcli();
        return;
    }
    if (m_dbusQueryInFlight) {
        return;
    }

    m_dbusQueryInFlight = true;
    QDBusMessage message =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.NetworkManager"), QStringLiteral("/org/freedesktop/NetworkManager"),
                                       QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    message << QStringLiteral("org.freedesktop.NetworkManager") << QStringLiteral("ActiveConnections");

    QDBusPendingCall pending = bus.asyncCall(message, kDBusTimeoutMs);
    auto* watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &NetworkProvider::handleActiveConnectionsReply);
}

void NetworkProvider::handleActiveConnectionsReply(QDBusPendingCallWatcher* watcher) {
    QDBusPendingReply<QVariant> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError()) {
        m_dbusQueryInFlight = false;
        refreshVpnViaNmcli();
        return;
    }

    const QList<QDBusObjectPath> paths = qdbus_cast<QList<QDBusObjectPath>>(reply.value());
    const QString timestamp = currentTimestamp();
    if (paths.isEmpty()) {
        m_dbusQueryInFlight = false;
        const QVector<VpnStatus> empty;
        emit activeVpnsReady(empty, QString());
        emit vpnStatusReady(selectPrimaryVpnStatus(empty, timestamp), QString());
        return;
    }

    auto batch = std::make_shared<ActiveConnectionsBatch>();
    batch->remaining = paths.size();
    batch->timestamp = timestamp;

    auto finishOne = [this, batch]() {
        --batch->remaining;
        if (batch->remaining <= 0) {
            m_dbusQueryInFlight = false;
            const QVector<VpnStatus> activeList = buildActiveVpnList(batch->candidates, batch->timestamp);
            emit activeVpnsReady(activeList, QString());
            emit vpnStatusReady(selectPrimaryVpnStatus(activeList, batch->timestamp), QString());
        }
    };

    QDBusConnection bus = QDBusConnection::systemBus();
    for (const QDBusObjectPath& path : paths) {
        QDBusMessage propCall = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.NetworkManager"), path.path(),
                                                               QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
        propCall << QStringLiteral("org.freedesktop.NetworkManager.Connection.Active");

        auto* propWatcher = new QDBusPendingCallWatcher(bus.asyncCall(propCall, kDBusTimeoutMs), this);
        connect(propWatcher, &QDBusPendingCallWatcher::finished, this, [this, propWatcher, batch, finishOne]() {
            QDBusPendingReply<QVariantMap> propReply = *propWatcher;
            propWatcher->deleteLater();
            if (propReply.isError()) {
                finishOne();
                return;
            }
            const QVariantMap props = propReply.value();
            const QString type = props.value(QStringLiteral("Type")).toString();
            if (!ProviderParsers::isVpnLikeConnectionType(type)) {
                finishOne();
                return;
            }

            ActiveVpnCandidate candidate;
            candidate.id = props.value(QStringLiteral("Id")).toString();
            candidate.state = props.value(QStringLiteral("State")).toUInt();
            const QList<QDBusObjectPath> devices = qdbus_cast<QList<QDBusObjectPath>>(props.value(QStringLiteral("Devices")));
            if (devices.isEmpty()) {
                batch->candidates.append(candidate);
                finishOne();
                return;
            }

            const int candidateIndex = batch->candidates.size();
            batch->candidates.append(candidate);
            QDBusMessage devCall = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.NetworkManager"), devices.first().path(),
                                                                  QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
            devCall << QStringLiteral("org.freedesktop.NetworkManager.Device") << QStringLiteral("Interface");

            auto* devWatcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(devCall, kDBusTimeoutMs), this);
            connect(devWatcher, &QDBusPendingCallWatcher::finished, this, [devWatcher, batch, candidateIndex, finishOne]() {
                QDBusPendingReply<QVariant> devReply = *devWatcher;
                devWatcher->deleteLater();
                if (!devReply.isError() && candidateIndex >= 0 && candidateIndex < batch->candidates.size()) {
                    const QString iface = devReply.value().toString();
                    if (!iface.isEmpty()) {
                        batch->candidates[candidateIndex].device = iface;
                    }
                }
                finishOne();
            });
        });
    }
}

void NetworkProvider::refreshVpnViaNmcli() {
    m_runner.run(QStringLiteral("nmcli"),
                 {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,TYPE,DEVICE,STATE"), QStringLiteral("connection"),
                  QStringLiteral("show"), QStringLiteral("--active")},
                 12000, QStringLiteral("vpn-active"),
                 /*deduplicateContext=*/true);
}

void NetworkProvider::refreshVpnProfiles() {
    m_runner.run(QStringLiteral("nmcli"),
                 {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,TYPE,DEVICE,STATE"), QStringLiteral("connection"),
                  QStringLiteral("show")},
                 12000, QStringLiteral("vpn-profiles"),
                 /*deduplicateContext=*/true);
}

void NetworkProvider::connectVpn(const QString& connectionName) {
    m_runner.run(QStringLiteral("nmcli"), {QStringLiteral("connection"), QStringLiteral("up"), connectionName}, 30000,
                 QStringLiteral("vpn-action:up:%1").arg(connectionName));
}

void NetworkProvider::disconnectVpn(const QString& connectionName) {
    m_runner.run(QStringLiteral("nmcli"), {QStringLiteral("connection"), QStringLiteral("down"), connectionName}, 20000,
                 QStringLiteral("vpn-action:down:%1").arg(connectionName));
}
