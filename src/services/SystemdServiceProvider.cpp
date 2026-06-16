#include "SystemdServiceProvider.h"

#include "../utils/TimeUtils.h"

#include <QRegularExpression>
#include <QSet>

SystemdServiceProvider::SystemdServiceProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("systemd-list")) {
            QVector<ServiceRow> rows;
            QString error;
            const QString now = currentTimestamp();
            const QRegularExpression lineExpression(QStringLiteral(R"(^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.*)$)"));
            QSet<QString> seen;

            if (result.ok()) {
                const QStringList lines = result.standardOutput.split('\n', Qt::SkipEmptyParts);
                for (const QString &line : lines) {
                    const auto match = lineExpression.match(line.trimmed());
                    if (!match.hasMatch() || !match.captured(1).endsWith(QStringLiteral(".service"))) {
                        continue;
                    }
                    ServiceRow row;
                    row.unit = match.captured(1);
                    row.loadState = match.captured(2);
                    row.activeState = match.captured(3);
                    row.subState = match.captured(4);
                    row.description = match.captured(5);
                    row.lastRefresh = now;
                    if (m_watched.isEmpty() || m_watched.contains(row.unit)) {
                        rows.append(row);
                        seen.insert(row.unit);
                    }
                }
            } else {
                error = result.startFailed
                    ? QStringLiteral("systemctl not found or failed to start.")
                    : result.standardError.trimmed();
            }

            for (const QString &unit : m_watched) {
                if (!seen.contains(unit)) {
                    rows.append(ServiceRow{unit, QStringLiteral("unavailable"), QStringLiteral("unknown"), QStringLiteral("-"), QStringLiteral("Unit not found"), now});
                }
            }
            emit servicesReady(rows, error);
        } else if (context == QStringLiteral("systemd-failed")) {
            int count = 0;
            QString error;
            if (result.ok() || result.exitCode == 1) {
                const QStringList lines = result.standardOutput.split('\n', Qt::SkipEmptyParts);
                for (const QString &line : lines) {
                    if (line.trimmed().startsWith(QStringLiteral("UNIT "))) {
                        continue;
                    }
                    if (line.contains(QStringLiteral(".service"))) {
                        ++count;
                    }
                }
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
