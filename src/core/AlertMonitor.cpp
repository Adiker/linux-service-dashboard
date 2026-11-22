#include "AlertMonitor.h"

#include <QStringList>

namespace {

int sensorSeverityRank(const QString &status)
{
    if (status == QStringLiteral("Critical")) {
        return 2;
    }
    if (status == QStringLiteral("Warning")) {
        return 1;
    }
    return 0;
}

bool isSmartFailureHealth(const QString &health)
{
    const QString lower = health.toLower();
    return lower.contains(QStringLiteral("fail")) || lower.contains(QStringLiteral("warning"));
}

} // namespace

AlertMonitor::AlertMonitor(QObject *parent)
    : QObject(parent)
{
}

void AlertMonitor::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void AlertMonitor::processSystemdServices(const QVector<ServiceRow> &rows)
{
    QSet<QString> currentFailed;
    QStringList newlyFailed;
    for (const ServiceRow &row : rows) {
        if (row.activeState == QStringLiteral("failed")) {
            currentFailed.insert(row.unit);
            if (m_systemdServicesInitialized && !m_failedServices.contains(row.unit)) {
                newlyFailed.append(row.unit);
            }
        }
    }
    m_failedServices = currentFailed;
    m_systemdServicesInitialized = true;

    if (!newlyFailed.isEmpty()) {
        emitAlert(QStringLiteral("Systemd service failed"),
                  QStringLiteral("Service(s) entered failed state: %1").arg(newlyFailed.join(QStringLiteral(", "))),
                  AlertSeverity::Critical);
    }
}

void AlertMonitor::processSystemdFailedCount(int count)
{
    if (m_failedCountInitialized && count > m_lastFailedCount && !m_systemdServicesInitialized) {
        emitAlert(QStringLiteral("Systemd failed services"),
                  QStringLiteral("Failed systemd services increased to %1.").arg(count),
                  AlertSeverity::Critical);
    }
    m_lastFailedCount = count;
    m_failedCountInitialized = true;
    updateSummary();
}

void AlertMonitor::processDockerContainers(const QVector<DockerContainerRow> &rows)
{
    int running = 0;
    for (const DockerContainerRow &row : rows) {
        if (row.state == QStringLiteral("running")) {
            ++running;
        }
    }
    m_dockerRunning = running;
    m_dockerTotal = rows.size();
    updateSummary();
}

void AlertMonitor::processSensors(const QVector<SensorRow> &rows)
{
    QHash<QString, QString> nextStates;
    for (const SensorRow &row : rows) {
        const QString key = row.chip + QLatin1Char('/') + row.label;
        nextStates.insert(key, row.status);
        if (!m_sensorsInitialized) {
            continue;
        }
        const int previousRank = sensorSeverityRank(m_sensorStates.value(key));
        const int currentRank = sensorSeverityRank(row.status);
        if (currentRank > previousRank && currentRank > 0) {
            const AlertSeverity severity = (currentRank >= 2) ? AlertSeverity::Critical : AlertSeverity::Warning;
            emitAlert(QStringLiteral("Sensor %1").arg(row.status),
                      QStringLiteral("%1 (%2) reached %3 C.").arg(row.label, row.chip).arg(row.current, 0, 'f', 1),
                      severity);
        }
    }
    m_sensorStates = nextStates;
    m_sensorsInitialized = true;
}

void AlertMonitor::processSmartResult(const QString &devicePath, const DiskRow &smartRow)
{
    const QString previous = m_diskSmartStates.value(devicePath);
    const QString current = smartRow.smartHealth;
    m_diskSmartStates.insert(devicePath, current);

    if (isSmartFailureHealth(current) && !isSmartFailureHealth(previous)) {
        emitAlert(QStringLiteral("SMART health warning"),
                  QStringLiteral("Disk %1 reported SMART status: %2.").arg(devicePath, current),
                  AlertSeverity::Critical);
    }
}

void AlertMonitor::processVpnStatus(const VpnStatus &status)
{
    if (m_vpnInitialized && m_lastVpnConnected && !status.connected) {
        const QString name = m_lastVpnName.isEmpty() ? QStringLiteral("VPN") : m_lastVpnName;
        emitAlert(QStringLiteral("VPN disconnected"),
                  QStringLiteral("VPN connection \"%1\" is no longer active.").arg(name),
                  AlertSeverity::Warning);
    }
    m_vpnInitialized = true;
    m_lastVpnConnected = status.connected;
    m_lastVpnName = status.connected ? status.connectionName : QString();
    updateSummary();
}

QString AlertMonitor::summaryTooltip() const
{
    QStringList parts;
    if (m_lastFailedCount >= 0) {
        parts.append(QStringLiteral("Failed services: %1").arg(m_lastFailedCount));
    }
    if (m_dockerTotal >= 0) {
        parts.append(QStringLiteral("Docker: %1/%2").arg(m_dockerRunning).arg(m_dockerTotal));
    }
    if (m_vpnInitialized) {
        parts.append(m_lastVpnConnected
                         ? QStringLiteral("VPN: %1").arg(m_lastVpnName)
                         : QStringLiteral("VPN: Disconnected"));
    }
    if (parts.isEmpty()) {
        return QStringLiteral("Linux Service Dashboard");
    }
    return QStringLiteral("Linux Service Dashboard\n%1").arg(parts.join(QStringLiteral(" | ")));
}

void AlertMonitor::emitAlert(const QString &title, const QString &message, AlertSeverity severity)
{
    if (!m_enabled) {
        return;
    }
    emit alertRaised(title, message, severity);
}

void AlertMonitor::updateSummary()
{
    emit summaryChanged(summaryTooltip());
}
