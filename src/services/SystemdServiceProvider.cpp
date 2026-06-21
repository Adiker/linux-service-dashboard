#include "SystemdServiceProvider.h"

#include "../parsers/ProviderParsers.h"
#include "../utils/TimeUtils.h"

SystemdServiceProvider::SystemdServiceProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("systemd-list")) {
            QString error;
            if (!result.ok()) {
                error = result.startFailed
                    ? QStringLiteral("systemctl not found or failed to start.")
                    : result.standardError.trimmed();
            }
            const QVector<ServiceRow> rows = ProviderParsers::parseSystemdListUnits(
                result.standardOutput, m_watched, currentTimestamp(), result.ok() ? nullptr : &error);
            emit servicesReady(rows, error);
        } else if (context == QStringLiteral("systemd-failed")) {
            QString error;
            int count = 0;
            if (result.ok() || result.exitCode == 1) {
                count = ProviderParsers::parseSystemdFailedCount(result.standardOutput, nullptr);
            } else {
                error = result.startFailed ? QStringLiteral("systemctl not found.") : result.standardError.trimmed();
            }
            emit failedCountReady(count, error);
        } else if (context.startsWith(QStringLiteral("systemd-logs:"))) {
            const QString unit = context.mid(QStringLiteral("systemd-logs:").size());
            emit logsReady(QStringLiteral("journalctl -u %1").arg(unit),
                           result.ok() ? result.standardOutput : result.standardError);
        } else if (context.startsWith(QStringLiteral("systemd-action:"))) {
            const QString action = context.section(':', 1, 1);
            const QString unit = context.section(':', 2);
            const QString message = result.ok()
                ? QStringLiteral("%1 %2 succeeded.").arg(action, unit)
                : QStringLiteral("%1 %2 failed.").arg(action, unit);
            emit actionFinished(message, result.ok() ? result.standardOutput : result.standardError);
        }
    });
}

void SystemdServiceProvider::refreshServices(const QStringList &watchedServices)
{
    m_watched = watchedServices;
    m_runner.run(QStringLiteral("systemctl"),
                 {QStringLiteral("list-units"), QStringLiteral("--type=service"), QStringLiteral("--all"), QStringLiteral("--no-pager"), QStringLiteral("--plain")},
                 15000,
                 QStringLiteral("systemd-list"));
}

void SystemdServiceProvider::refreshFailedCount()
{
    m_runner.run(QStringLiteral("systemctl"),
                 {QStringLiteral("--failed"), QStringLiteral("--no-pager")},
                 12000,
                 QStringLiteral("systemd-failed"));
}

void SystemdServiceProvider::serviceLogs(const QString &unit)
{
    m_runner.run(QStringLiteral("journalctl"),
                 {QStringLiteral("-u"), unit, QStringLiteral("-n"), QStringLiteral("300"), QStringLiteral("--no-pager")},
                 20000,
                 QStringLiteral("systemd-logs:%1").arg(unit));
}

void SystemdServiceProvider::controlService(const QString &unit, const QString &action)
{
    m_runner.run(QStringLiteral("systemctl"), {action, unit}, 30000, QStringLiteral("systemd-action:%1:%2").arg(action, unit));
}
