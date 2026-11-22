#pragma once

#include "../models/DiskModel.h"
#include "../models/DockerContainerModel.h"
#include "../models/SensorModel.h"
#include "../models/ServiceModel.h"
#include "../services/NetworkProvider.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class ServiceRegistry;

enum class AlertSeverity {
    Info,
    Warning,
    Critical,
};

class AlertMonitor : public QObject {
    Q_OBJECT

public:
    explicit AlertMonitor(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] QString summaryTooltip() const;

    // Public entry points so state transitions can be unit-tested directly.
    void processSystemdServices(const QVector<ServiceRow> &rows);
    void processSystemdFailedCount(int count);
    void processDockerContainers(const QVector<DockerContainerRow> &rows);
    void processSensors(const QVector<SensorRow> &rows);
    void processSmartResult(const QString &devicePath, const DiskRow &smartRow);
    void processVpnStatus(const VpnStatus &status);

signals:
    void alertRaised(const QString &title, const QString &message, AlertSeverity severity);
    void summaryChanged(const QString &tooltip);

private:
    void emitAlert(const QString &title, const QString &message, AlertSeverity severity);
    void updateSummary();

    bool m_enabled = true;

    bool m_systemdServicesInitialized = false;
    QSet<QString> m_failedServices;

    bool m_failedCountInitialized = false;
    int m_lastFailedCount = -1;

    int m_dockerRunning = -1;
    int m_dockerTotal = -1;

    bool m_sensorsInitialized = false;
    QHash<QString, QString> m_sensorStates;

    QHash<QString, QString> m_diskSmartStates;

    bool m_vpnInitialized = false;
    bool m_lastVpnConnected = false;
    QString m_lastVpnName;
};
