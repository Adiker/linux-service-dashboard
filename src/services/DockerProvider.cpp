#include "DockerProvider.h"

#include "../utils/JsonUtils.h"

#include <QJsonObject>

DockerProvider::DockerProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("docker-list")) {
            QVector<DockerContainerRow> rows;
            QString error;
            if (result.ok()) {
                const QStringList lines = result.standardOutput.split('\n', Qt::SkipEmptyParts);
                for (const QString &line : lines) {
                    QString parseError;
                    const QJsonDocument document = parseJsonDocument(line, &parseError);
                    if (!document.isObject()) {
                        continue;
                    }
                    const QJsonObject object = document.object();
                    DockerContainerRow row;
                    row.name = object.value(QStringLiteral("Names")).toString();
                    row.image = object.value(QStringLiteral("Image")).toString();
                    row.status = object.value(QStringLiteral("Status")).toString();
                    row.state = object.value(QStringLiteral("State")).toString();
                    row.ports = object.value(QStringLiteral("Ports")).toString();
                    row.created = object.value(QStringLiteral("CreatedAt")).toString(object.value(QStringLiteral("Created")).toString());
                    row.id = object.value(QStringLiteral("ID")).toString();
                    rows.append(row);
                }
            } else {
                error = result.startFailed
                    ? QStringLiteral("docker not found. Install the Docker CLI to enable this page.")
                    : result.standardError.trimmed();
                if (error.contains(QStringLiteral("permission denied"), Qt::CaseInsensitive)) {
                    error = QStringLiteral("Docker permission denied. Add your user to the docker group or run a rootless Docker setup.");
                }
            }
            emit containersReady(rows, error);
        } else if (context.startsWith(QStringLiteral("docker-logs:"))) {
            const QString container = context.mid(QStringLiteral("docker-logs:").size());
            emit textReady(QStringLiteral("docker logs %1").arg(container), result.ok() ? result.standardOutput : result.standardError);
        } else if (context.startsWith(QStringLiteral("docker-inspect:"))) {
            const QString container = context.mid(QStringLiteral("docker-inspect:").size());
            emit textReady(QStringLiteral("docker inspect %1").arg(container), result.ok() ? result.standardOutput : result.standardError);
        } else if (context.startsWith(QStringLiteral("docker-action:"))) {
            const QString action = context.section(':', 1, 1);
            const QString container = context.section(':', 2);
            const QString message = result.ok()
                ? QStringLiteral("docker %1 %2 succeeded.").arg(action, container)
                : QStringLiteral("docker %1 %2 failed.").arg(action, container);
            emit actionFinished(message, result.ok() ? result.standardOutput : result.standardError);
        }
    });
}

void DockerProvider::refreshContainers()
{
    m_runner.run(QStringLiteral("docker"),
                 {QStringLiteral("ps"), QStringLiteral("-a"), QStringLiteral("--format"), QStringLiteral("{{json .}}")},
                 20000,
                 QStringLiteral("docker-list"));
}

void DockerProvider::containerLogs(const QString &container)
{
    m_runner.run(QStringLiteral("docker"), {QStringLiteral("logs"), QStringLiteral("--tail"), QStringLiteral("300"), container}, 20000, QStringLiteral("docker-logs:%1").arg(container));
}

void DockerProvider::inspectContainer(const QString &container)
{
    m_runner.run(QStringLiteral("docker"), {QStringLiteral("inspect"), container}, 20000, QStringLiteral("docker-inspect:%1").arg(container));
}

void DockerProvider::controlContainer(const QString &container, const QString &action)
{
    m_runner.run(QStringLiteral("docker"), {action, container}, 30000, QStringLiteral("docker-action:%1:%2").arg(action, container));
}
