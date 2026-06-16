#pragma once

#include "../core/CommandRunner.h"
#include "../models/SensorModel.h"

#include <QObject>

class SensorProvider : public QObject {
    Q_OBJECT

public:
    explicit SensorProvider(QObject *parent = nullptr);
    void refreshSensors();

signals:
    void sensorsReady(const QVector<SensorRow> &rows, const QString &error);

private:
    CommandRunner m_runner;
};
