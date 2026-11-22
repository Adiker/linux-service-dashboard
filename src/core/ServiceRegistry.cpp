#include "ServiceRegistry.h"

#include "AlertMonitor.h"

ServiceRegistry::ServiceRegistry(QObject* parent)
    : QObject(parent), m_systemd(this), m_docker(this), m_network(this), m_mounts(this), m_sensors(this), m_smart(this) {
    attachRunner(m_systemd.commandRunner());
    attachRunner(m_docker.commandRunner());
    attachRunner(m_network.commandRunner());
    attachRunner(m_mounts.commandRunner());
    attachRunner(m_sensors.commandRunner());
    attachRunner(m_smart.commandRunner());
}

void ServiceRegistry::attachRunner(CommandRunner* runner) {
    if (!runner) {
        return;
    }
    connect(runner, &CommandRunner::failedCommandRecorded, this, [this](const FailedCommandEntry& entry) {
        m_failedCommands.append(entry);
        while (m_failedCommands.size() > kMaxDiagnosticEntries) {
            m_failedCommands.removeFirst();
        }
        emit failedCommandRecorded(entry);
    });
}

void ServiceRegistry::clearFailedCommands() {
    m_failedCommands.clear();
    m_systemd.commandRunner()->clearFailedCommands();
    m_docker.commandRunner()->clearFailedCommands();
    m_network.commandRunner()->clearFailedCommands();
    m_mounts.commandRunner()->clearFailedCommands();
    m_sensors.commandRunner()->clearFailedCommands();
    m_smart.commandRunner()->clearFailedCommands();
    emit failedCommandsCleared();
}

void ServiceRegistry::bindAlertMonitor(AlertMonitor* monitor) {
    if (!monitor) {
        return;
    }
    connect(&m_systemd, &SystemdServiceProvider::servicesReady, monitor, [monitor](const QVector<ServiceRow>& rows, const QString& error) {
        if (error.isEmpty()) {
            monitor->processSystemdServices(rows);
        }
    });
    connect(&m_systemd, &SystemdServiceProvider::failedCountReady, monitor, [monitor](int count, const QString& error) {
        if (error.isEmpty()) {
            monitor->processSystemdFailedCount(count);
        }
    });
    connect(&m_docker, &DockerProvider::containersReady, monitor, [monitor](const QVector<DockerContainerRow>& rows, const QString& error) {
        if (error.isEmpty()) {
            monitor->processDockerContainers(rows);
        }
    });
    connect(&m_sensors, &SensorProvider::sensorsReady, monitor, [monitor](const QVector<SensorRow>& rows, const QString& error) {
        if (error.isEmpty()) {
            monitor->processSensors(rows);
        }
    });
    connect(&m_smart, &SmartProvider::smartReady, monitor, [monitor](const QString& path, const DiskRow& smartRow, const QString& error) {
        if (error.isEmpty()) {
            monitor->processSmartResult(path, smartRow);
        }
    });
    connect(&m_network, &NetworkProvider::vpnStatusReady, monitor, [monitor](const VpnStatus& status, const QString& error) {
        if (error.isEmpty()) {
            monitor->processVpnStatus(status);
        }
    });
}
