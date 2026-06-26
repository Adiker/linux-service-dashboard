#include "SensorProvider.h"

#include "../parsers/ProviderParsers.h"

SensorProvider::SensorProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("sensors-json")) {
            QString error;
            QVector<SensorRow> rows;
            if (result.ok()) {
                rows = ProviderParsers::parseSensorsJson(result.standardOutput, &error);
            } else {
                error = result.startFailed
                    ? QStringLiteral("sensors not found. Install lm_sensors and run sensors-detect if needed.")
                    : result.standardError.trimmed();
            }
            emit sensorsReady(rows, error);
        }
    });
}

void SensorProvider::refreshSensors()
{
    m_runner.run(QStringLiteral("sensors"), {QStringLiteral("-j")}, 15000, QStringLiteral("sensors-json"));
}
