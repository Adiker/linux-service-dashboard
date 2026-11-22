#pragma once

#include "../core/CommandRunner.h"
#include "../models/SensorModel.h"

#include <QObject>

class SensorProvider : public QObject {
    Q_OBJECT

public:
    explicit SensorProvider(QObject *parent = nullptr);
    void refreshSensors();

    [[nodiscard]] CommandRunner *commandRunner() { return &m_runner; }

signals:
    void sensorsReady(const QVector<SensorRow> &rows, const QString &error);

private:
    void runTextFallback();

    CommandRunner m_runner;
};
