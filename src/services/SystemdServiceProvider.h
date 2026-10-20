#pragma once
#include "../core/CommandRunner.h"
#include "../models/ServiceModel.h"

#include <QHash>
#include <QObject>
#include <QSet>

class SystemdServiceProvider : public QObject {
    Q_OBJECT
  public:
    explicit SystemdServiceProvider(QObject* parent = nullptr);
    void refreshServices(const QStringList& watchedServices);
    void refreshFailedCount();
    void serviceLogs(const QString& unit, ServiceScope scope = ServiceScope::System);
    void controlService(const QString& unit, const QString& action, ServiceScope scope = ServiceScope::System);
    static bool isProtectedUnit(const QString& unit);
  signals:
    void servicesReady(const QVector<ServiceRow>& rows, const QString& error);
    void failedCountReady(int count, const QString& error);
    void logsReady(const QString& title, const QString& text);
    void actionFinished(const QString& message, const QString& details);

  private:
    CommandRunner m_runner;
    QHash<int, QVector<ServiceRow>> m_rows, m_next;
    QHash<int, QString> m_errors;
    QSet<int> m_pending;
    void listScope(ServiceScope scope);
    void listFiles(ServiceScope scope);
    void finishScope(ServiceScope scope, const QString& error = {});
};
