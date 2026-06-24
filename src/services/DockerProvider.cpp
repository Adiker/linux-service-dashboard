#include "DockerProvider.h"

#include "../parsers/ProviderParsers.h"

DockerProvider::DockerProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("docker-list")) {
            QString error;
            QVector<DockerContainerRow> rows;
            if (result.ok()) {
                rows = ProviderParsers::parseDockerContainerLines(result.standardOutput, nullptr);
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
