#include "SystemdServiceProvider.h"

#include "../utils/TimeUtils.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QRegularExpression>
#include <QSet>

namespace {

struct UnitInfo {
    QString name;
    QString description;
    QString loadState;
    QString activeState;
    QString subState;
};

bool parseUnitStructure(QDBusArgument *argument, UnitInfo *unit)
{
    argument->beginStructure();
    *argument >> unit->name >> unit->description >> unit->loadState >> unit->activeState >> unit->subState;
    QString following;
    QDBusObjectPath unitPath;
    quint32 jobId = 0;
    QString jobType;
    QDBusObjectPath jobPath;
    *argument >> following >> unitPath >> jobId >> jobType >> jobPath;
    argument->endStructure();
    return true;
}

QVector<UnitInfo> listUnitsViaDBus(QString *error)
{
    QVector<UnitInfo> units;
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        if (error) {
            *error = QStringLiteral("System DBus unavailable.");
        }
        return units;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                                            QStringLiteral("/org/freedesktop/systemd1"),
                                                            QStringLiteral("org.freedesktop.systemd1.Manager"),
                                                            QStringLiteral("ListUnits"));
    const QDBusMessage reply = bus.call(message);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error) {
            *error = reply.errorMessage();
        }
        return units;
    }

    QDBusArgument argument = reply.arguments().at(0).value<QDBusArgument>();
    argument.beginArray();
    while (!argument.atEnd()) {
        UnitInfo unit;
        parseUnitStructure(&argument, &unit);
        units.append(unit);
    }
    argument.endArray();
    return units;
}

QVector<UnitInfo> listFailedUnitsViaDBus(QString *error)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        if (error) {
            *error = QStringLiteral("System DBus unavailable.");
        }
        return {};
    }

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                                            QStringLiteral("/org/freedesktop/systemd1"),
                                                            QStringLiteral("org.freedesktop.systemd1.Manager"),
                                                            QStringLiteral("ListUnitsFiltered"));
    message << QStringList{QStringLiteral("failed")};
    const QDBusMessage reply = bus.call(message);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error) {
            *error = reply.errorMessage();
        }
        return {};
    }

    QVector<UnitInfo> units;
    QDBusArgument argument = reply.arguments().at(0).value<QDBusArgument>();
    argument.beginArray();
    while (!argument.atEnd()) {
        UnitInfo unit;
        parseUnitStructure(&argument, &unit);
        units.append(unit);
    }
    argument.endArray();
    return units;
}

QVector<ServiceRow> buildServiceRows(const QVector<UnitInfo> &units, const QStringList &watchedServices, const QString &timestamp)
{
    QVector<ServiceRow> rows;
    QSet<QString> seen;
    for (const UnitInfo &unit : units) {
        if (!unit.name.endsWith(QStringLiteral(".service"))) {
            continue;
        }
        if (!watchedServices.isEmpty() && !watchedServices.contains(unit.name)) {
            continue;
        }
        ServiceRow row;
        row.unit = unit.name;
        row.loadState = unit.loadState;
        row.activeState = unit.activeState;
        row.subState = unit.subState;
        row.description = unit.description;
        row.lastRefresh = timestamp;
        rows.append(row);
        seen.insert(unit.name);
    }
    for (const QString &unit : watchedServices) {
        if (!seen.contains(unit)) {
            rows.append(ServiceRow{unit, QStringLiteral("unavailable"), QStringLiteral("unknown"), QStringLiteral("-"), QStringLiteral("Unit not found"), timestamp});
        }
    }
    return rows;
}

QVector<ServiceRow> parseSystemctlListUnits(const QString &output, const QStringList &watchedServices, const QString &timestamp)
{
    QVector<ServiceRow> rows;
    const QRegularExpression lineExpression(QStringLiteral(R"(^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.*)$)"));
    QSet<QString> seen;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
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
        row.lastRefresh = timestamp;
        if (watchedServices.isEmpty() || watchedServices.contains(row.unit)) {
            rows.append(row);
            seen.insert(row.unit);
        }
    }
    for (const QString &unit : watchedServices) {
        if (!seen.contains(unit)) {
            rows.append(ServiceRow{unit, QStringLiteral("unavailable"), QStringLiteral("unknown"), QStringLiteral("-"), QStringLiteral("Unit not found"), timestamp});
        }
    }
    return rows;
}

int parseSystemctlFailedCount(const QString &output)
{
    int count = 0;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(QStringLiteral("UNIT "))) {
            continue;
        }
        if (line.contains(QStringLiteral(".service"))) {
            ++count;
        }
    }
    return count;
}

} // namespace

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
            emit servicesReady(parseSystemctlListUnits(result.standardOutput, m_watched, currentTimestamp()), error);
        } else if (context == QStringLiteral("systemd-failed")) {
            QString error;
            int count = 0;
            if (result.ok() || result.exitCode == 1) {
                count = parseSystemctlFailedCount(result.standardOutput);
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
    QString error;
    const QVector<UnitInfo> units = listUnitsViaDBus(&error);
    if (!units.isEmpty() || error.isEmpty()) {
        emit servicesReady(buildServiceRows(units, watchedServices, currentTimestamp()), QString());
        return;
    }

    m_runner.run(QStringLiteral("systemctl"),
                 {QStringLiteral("list-units"), QStringLiteral("--type=service"), QStringLiteral("--all"), QStringLiteral("--no-pager"), QStringLiteral("--plain")},
                 15000,
                 QStringLiteral("systemd-list"));
}

void SystemdServiceProvider::refreshFailedCount()
{
    QString error;
    const QVector<UnitInfo> units = listFailedUnitsViaDBus(&error);
    if (!units.isEmpty() || error.isEmpty()) {
        int count = 0;
        for (const UnitInfo &unit : units) {
            if (unit.name.endsWith(QStringLiteral(".service"))) {
                ++count;
            }
        }
        emit failedCountReady(count, QString());
        return;
    }

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
