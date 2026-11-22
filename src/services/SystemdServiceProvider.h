#pragma once

#include "../core/CommandRunner.h"
#include "../models/ServiceModel.h"

#include <QObject>

class QDBusPendingCallWatcher;

class SystemdServiceProvider : public QObject {
    Q_OBJECT

  public:
    explicit SystemdServiceProvider(QObject* parent = nullptr);
    void refreshServices(const QStringList& watchedServices);
    void refreshFailedCount();
    void serviceLogs(const QString& unit, int tailLines = 300);
    void controlService(const QString& unit, const QString& action);

    [[nodiscard]] CommandRunner* commandRunner() { return &m_runner; }

  signals:
    void servicesReady(const QVector<ServiceRow>& rows, const QString& error);
    void failedCountReady(int count, const QString& error);
    void logsReady(const QString& title, const QString& text);
    void actionFinished(const QString& message, const QString& details);

  private:
    void runSystemctlListUnits();
    void runSystemctlFailedCount();
    void handleListUnitsReply(QDBusPendingCallWatcher* watcher, const QStringList& watchedServices);
    void handleFailedUnitsReply(QDBusPendingCallWatcher* watcher);

    CommandRunner m_runner;
    QStringList m_watched;
    bool m_dbusListInFlight = false;
    bool m_dbusFailedInFlight = false;
};
