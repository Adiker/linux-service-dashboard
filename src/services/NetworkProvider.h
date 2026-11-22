#pragma once

#include "../core/CommandRunner.h"

#include <QObject>
#include <QVector>

struct VpnStatus {
    bool connected = false;
    QString connectionName = QStringLiteral("None");
    QString device = QStringLiteral("-");
    QString state = QStringLiteral("Unknown");
    QString lastRefresh;
};

struct VpnProfile {
    QString name;
    QString type;
    QString device = QStringLiteral("-");
    QString state = QStringLiteral("disconnected");
    bool active = false;
};

class QDBusPendingCallWatcher;

class NetworkProvider : public QObject {
    Q_OBJECT

  public:
    explicit NetworkProvider(QObject* parent = nullptr);
    void refreshVpnStatus();
    void refreshVpnProfiles();
    void connectVpn(const QString& connectionName);
    void disconnectVpn(const QString& connectionName);

    [[nodiscard]] CommandRunner* commandRunner() { return &m_runner; }

  signals:
    void vpnStatusReady(const VpnStatus& status, const QString& error);
    void activeVpnsReady(const QVector<VpnStatus>& activeVpns, const QString& error);
    void vpnProfilesReady(const QVector<VpnProfile>& profiles, const QString& error);
    void actionFinished(const QString& message, const QString& details);

  private:
    void refreshVpnViaNmcli();
    void handleActiveConnectionsReply(QDBusPendingCallWatcher* watcher);

    CommandRunner m_runner;
    bool m_dbusQueryInFlight = false;
};
