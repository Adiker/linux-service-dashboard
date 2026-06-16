#pragma once

#include "../core/CommandRunner.h"
#include "../models/ServiceModel.h"

#include <QObject>

class SystemdServiceProvider : public QObject {
    Q_OBJECT

public:
    explicit SystemdServiceProvider(QObject *parent = nullptr);
    void refreshServices(const QStringList &watchedServices);
    void refreshFailedCount();
    void serviceLogs(const QString &unit);
    void controlService(const QString &unit, const QString &action);

signals:
    void servicesReady(const QVector<ServiceRow> &rows, const QString &error);
    void failedCountReady(int count, const QString &error);
    void logsReady(const QString &title, const QString &text);
    void actionFinished(const QString &message, const QString &details);

private:
    CommandRunner m_runner;
    QStringList m_watched;
};
