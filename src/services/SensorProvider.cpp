#include "SensorProvider.h"

#include "../utils/JsonUtils.h"
#include "../utils/StatusUtils.h"

#include <QJsonObject>

SensorProvider::SensorProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("sensors-json")) {
            QVector<SensorRow> rows;
            QString error;
            if (result.ok()) {
                QString parseError;
                const QJsonDocument document = parseJsonDocument(result.standardOutput, &parseError);
                const QJsonObject root = document.object();
                for (auto chipIt = root.begin(); chipIt != root.end(); ++chipIt) {
                    const QJsonObject chip = chipIt.value().toObject();
                    for (auto featureIt = chip.begin(); featureIt != chip.end(); ++featureIt) {
                        const QJsonObject feature = featureIt.value().toObject();
                        double input = 0.0;
                        double high = 0.0;
                        double critical = 0.0;
                        bool hasInput = false;
                        for (auto valueIt = feature.begin(); valueIt != feature.end(); ++valueIt) {
                            const QString key = valueIt.key();
                            if (key.endsWith(QStringLiteral("_input"))) {
                                input = valueIt.value().toDouble();
                                hasInput = true;
                            } else if (key.endsWith(QStringLiteral("_max")) || key.endsWith(QStringLiteral("_high"))) {
                                high = valueIt.value().toDouble();
                            } else if (key.endsWith(QStringLiteral("_crit"))) {
                                critical = valueIt.value().toDouble();
                            }
                        }
                        if (hasInput) {
                            rows.append(SensorRow{chipIt.key(), featureIt.key(), input, high, critical, temperatureStatus(input, high, critical)});
                        }
                    }
                }
                if (!parseError.isEmpty()) {
                    error = QStringLiteral("Could not parse sensors JSON: %1").arg(parseError);
                }
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
