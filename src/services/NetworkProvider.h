#pragma once

#include "../core/CommandRunner.h"

#include <QObject>

struct VpnStatus {
    bool connected = false;
    QString connectionName = "None";
    QString device = "-";
    QString state = "Unknown";
    QString lastRefresh;
};

class QDBusPendingCallWatcher;

class NetworkProvider : public QObject {
    Q_OBJECT

  public:
    explicit NetworkProvider(QObject* parent = nullptr);
    void refreshVpnStatus();

  signals:
    void vpnStatusReady(const VpnStatus& status, const QString& error);

  private:
    void refreshVpnViaNmcli();
    void handleActiveConnectionsReply(QDBusPendingCallWatcher* watcher);

    CommandRunner m_runner;
};
