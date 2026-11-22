#pragma once

#include "CommandRunner.h"
#include "../services/DockerProvider.h"
#include "../services/MountProvider.h"
#include "../services/NetworkProvider.h"
#include "../services/SensorProvider.h"
#include "../services/SmartProvider.h"
#include "../services/SystemdServiceProvider.h"

#include <QObject>
#include <QVector>

class ServiceRegistry : public QObject {
    Q_OBJECT

public:
    static constexpr int kMaxDiagnosticEntries = 300;

    explicit ServiceRegistry(QObject *parent = nullptr);

    [[nodiscard]] SystemdServiceProvider *systemd() { return &m_systemd; }
    [[nodiscard]] DockerProvider *docker() { return &m_docker; }
    [[nodiscard]] NetworkProvider *network() { return &m_network; }
    [[nodiscard]] MountProvider *mounts() { return &m_mounts; }
    [[nodiscard]] SensorProvider *sensors() { return &m_sensors; }
    [[nodiscard]] SmartProvider *smart() { return &m_smart; }

    [[nodiscard]] QVector<FailedCommandEntry> failedCommands() const { return m_failedCommands; }
    void clearFailedCommands();
    void bindAlertMonitor(class AlertMonitor *monitor);

signals:
    void failedCommandRecorded(const FailedCommandEntry &entry);
    void failedCommandsCleared();

private:
    void attachRunner(CommandRunner *runner);

    SystemdServiceProvider m_systemd;
    DockerProvider m_docker;
    NetworkProvider m_network;
    MountProvider m_mounts;
    SensorProvider m_sensors;
    SmartProvider m_smart;
    QVector<FailedCommandEntry> m_failedCommands;
};
