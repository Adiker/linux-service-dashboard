#include "SensorProvider.h"

#include "../parsers/ProviderParsers.h"

SensorProvider::SensorProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("sensors-json")) {
            if (result.ok()) {
                QString parseError;
                const QVector<SensorRow> rows = ProviderParsers::parseSensorsJson(result.standardOutput, &parseError);
                if (parseError.isEmpty()) {
                    emit sensorsReady(rows, QString());
                    return;
                }
                // JSON parse failed (e.g. partial/non-JSON output); try plain text fallback.
                runTextFallback();
                return;
            }
            if (result.startFailed) {
                emit sensorsReady({}, QStringLiteral("sensors not found. Install lm_sensors and run sensors-detect if needed."));
                return;
            }
            // Older lm_sensors versions may not support -j; fall back to plain text output.
            runTextFallback();
        } else if (context == QStringLiteral("sensors-text")) {
            QString error;
            QVector<SensorRow> rows;
            if (result.ok()) {
                rows = ProviderParsers::parseSensorsText(result.standardOutput, &error);
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
    m_runner.run(QStringLiteral("sensors"),
                 {QStringLiteral("-j")},
                 15000,
                 QStringLiteral("sensors-json"),
                 /*deduplicateContext=*/true);
}

void SensorProvider::runTextFallback()
{
    m_runner.run(QStringLiteral("sensors"),
                 {},
                 15000,
                 QStringLiteral("sensors-text"),
                 /*deduplicateContext=*/true);
}
