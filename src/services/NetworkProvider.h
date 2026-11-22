#pragma once
#include "../core/CommandRunner.h"

#include <QObject>
#include <QVector>

// Retained for parser/source compatibility; UI consumes the collection below.
struct VpnStatus {
    bool connected = false;
    QString connectionName = "None", device = "-", state = "Unknown", lastRefresh;
};
struct VpnConnection {
    QString id, name, type, device, state, source;
    bool managerActive = false;
};
class NetworkProvider : public QObject {
    Q_OBJECT
  public:
    explicit NetworkProvider(QObject* parent = nullptr);
    void refreshVpnStatus();
    static QStringList splitNmcli(const QString& line);
  signals:
    void vpnStatusReady(const VpnStatus& status, const QString& error);
    void vpnConnectionsReady(const QVector<VpnConnection>& rows, const QString& error);

  private:
    CommandRunner m_runner;
    bool m_busy = false, m_nmDone = false, m_linksDone = false;
    int m_pending = 0;
    QVector<VpnConnection> m_connections, m_links;
    QStringList m_errors;
    void fallback();
    void finish();
};
