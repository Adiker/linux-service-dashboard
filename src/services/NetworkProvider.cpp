#include "NetworkProvider.h"

#include "../utils/TimeUtils.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
bool tunnelType(const QString& type) {
    return QStringList{"vpn", "tun", "wireguard", "ppp", "ipip", "gre", "sit"}.contains(type);
}
QDBusMessage propertyMessage(const QString& path, const QString& method) {
    return QDBusMessage::createMethodCall("org.freedesktop.NetworkManager", path, "org.freedesktop.DBus.Properties", method);
}
} // namespace
NetworkProvider::NetworkProvider(QObject* parent) : QObject(parent) {
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const auto&, const CommandResult& r, const QString& context) {
        if (context == "vpn-cli") {
            if (!r.ok())
                m_errors.append(r.standardError);
            else
                for (const auto& line : r.standardOutput.split('\n', Qt::SkipEmptyParts)) {
                    const auto f = splitNmcli(line);
                    if (f.size() < 5 || !tunnelType(f[2]))
                        continue;
                    m_connections.append({f[1], f[0], f[2], f[3], f[4], "NetworkManager / nmcli", f[4] == "activated"});
                }
            m_nmDone = true;
        } else if (context == "vpn-links") {
            if (!r.ok())
                m_errors.append(r.standardError);
            else {
                QJsonParseError e;
                const auto doc = QJsonDocument::fromJson(r.standardOutput.toUtf8(), &e);
                if (!doc.isArray())
                    m_errors.append(tr("Invalid link JSON"));
                else
                    for (const auto& v : doc.array()) {
                        const auto o = v.toObject();
                        const auto type = o["linkinfo"].toObject()["info_kind"].toString();
                        if (!tunnelType(type))
                            continue;
                        const auto name = o["ifname"].toString();
                        m_links.append({"kernel:" + name, name, type, name, o["operstate"].toString(),
                                        tr("Kernel tunnel; connectivity not verified"), false});
                    }
            }
            m_linksDone = true;
        }
        finish();
    });
}
QStringList NetworkProvider::splitNmcli(const QString& line) {
    QStringList fields;
    QString field;
    bool escaped = false;
    for (const auto c : line) {
        if (escaped) {
            field += c;
            escaped = false;
        } else if (c == '\\')
            escaped = true;
        else if (c == ':') {
            fields.append(field);
            field.clear();
        } else
            field += c;
    }
    if (escaped)
        field += '\\';
    fields.append(field);
    return fields;
}
void NetworkProvider::fallback() {
    m_runner.run("nmcli", {"-t", "--escape", "yes", "-f", "NAME,UUID,TYPE,DEVICE,STATE", "connection", "show", "--active"}, 10000,
                 "vpn-cli");
}
void NetworkProvider::refreshVpnStatus() {
    if (m_busy)
        return;
    m_busy = true;
    m_nmDone = m_linksDone = false;
    m_connections.clear();
    m_links.clear();
    m_errors.clear();
    m_runner.run("ip", {"-j", "-d", "link", "show"}, 10000, "vpn-links");
    auto bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        fallback();
        return;
    }
    auto message = propertyMessage("/org/freedesktop/NetworkManager", "Get");
    message << "org.freedesktop.NetworkManager" << "ActiveConnections";
    auto* watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* w) {
        QDBusPendingReply<QVariant> reply = *w;
        w->deleteLater();
        if (reply.isError()) {
            fallback();
            return;
        }
        const auto paths = qdbus_cast<QList<QDBusObjectPath>>(reply.value());
        m_pending = paths.size();
        if (paths.isEmpty()) {
            m_nmDone = true;
            finish();
            return;
        }
        for (const auto& path : paths) {
            auto msg = propertyMessage(path.path(), "GetAll");
            msg << "org.freedesktop.NetworkManager.Connection.Active";
            auto* active = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg, 5000), this);
            connect(active, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* a) {
                QDBusPendingReply<QVariantMap> p = *a;
                a->deleteLater();
                if (p.isError()) {
                    m_errors.append(p.error().message());
                    if (--m_pending == 0) {
                        m_nmDone = true;
                        finish();
                    }
                    return;
                }
                const auto v = p.value();
                const auto type = v.value("Type").toString();
                if (!tunnelType(type)) {
                    if (--m_pending == 0) {
                        m_nmDone = true;
                        finish();
                    }
                    return;
                }
                const auto state = v.value("State").toUInt();
                VpnConnection row{v.value("Uuid").toString(), v.value("Id").toString(), type,      "-",
                                  QString::number(state),     "NetworkManager",         state == 2};
                row.state = state == 2 ? tr("Activated; connectivity not verified") : state == 1 ? tr("Activating") : tr("Inactive");
                const auto devices = qdbus_cast<QList<QDBusObjectPath>>(v.value("Devices"));
                if (devices.isEmpty()) {
                    m_connections.append(row);
                    if (--m_pending == 0) {
                        m_nmDone = true;
                        finish();
                    }
                    return;
                }
                auto deviceMsg = propertyMessage(devices.first().path(), "Get");
                deviceMsg << "org.freedesktop.NetworkManager.Device" << "Interface";
                auto* device = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(deviceMsg, 5000), this);
                connect(device, &QDBusPendingCallWatcher::finished, this, [this, row](QDBusPendingCallWatcher* d) mutable {
                    QDBusPendingReply<QVariant> value = *d;
                    d->deleteLater();
                    if (!value.isError())
                        row.device = value.value().toString();
                    m_connections.append(row);
                    if (--m_pending == 0) {
                        m_nmDone = true;
                        finish();
                    }
                });
            });
        }
    });
}
void NetworkProvider::finish() {
    if (!m_busy || !m_nmDone || !m_linksDone)
        return;
    for (const auto& link : m_links) {
        bool seen = false;
        for (const auto& row : m_connections)
            if (row.device == link.device)
                seen = true;
        if (!seen)
            m_connections.append(link);
    }
    m_busy = false;
    const auto error = m_errors.join("\n");
    emit vpnConnectionsReady(m_connections, error);
    VpnStatus legacy;
    legacy.lastRefresh = currentTimestamp();
    for (const auto& row : m_connections)
        if (row.managerActive) {
            legacy.connected = true;
            legacy.connectionName = row.name;
            legacy.device = row.device;
            legacy.state = row.state;
            break;
        }
    emit vpnStatusReady(legacy, error);
}
