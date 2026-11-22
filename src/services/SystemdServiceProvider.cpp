#include "SystemdServiceProvider.h"

#include "../parsers/ProviderParsers.h"
#include "../utils/TimeUtils.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QSet>

namespace {

constexpr int kDBusTimeoutMs = 3000;

struct UnitInfo {
    QString name;
    QString description;
    QString loadState;
    QString activeState;
    QString subState;
};

void parseUnitStructure(const QDBusArgument& argument, UnitInfo* unit) {
    argument.beginStructure();
    argument >> unit->name >> unit->description >> unit->loadState >> unit->activeState >> unit->subState;
    QString following;
    QDBusObjectPath unitPath;
    quint32 jobId = 0;
    QString jobType;
    QDBusObjectPath jobPath;
    argument >> following >> unitPath >> jobId >> jobType >> jobPath;
    argument.endStructure();
}

QVector<UnitInfo> extractUnitsFromReply(const QDBusMessage& reply) {
    QVector<UnitInfo> units;
    if (reply.arguments().isEmpty()) {
        return units;
    }
    const QDBusArgument argument = reply.arguments().at(0).value<QDBusArgument>();
    argument.beginArray();
    while (!argument.atEnd()) {
        UnitInfo unit;
        parseUnitStructure(argument, &unit);
        units.append(unit);
    }
    argument.endArray();
    return units;
}

QVector<ServiceRow> buildServiceRows(const QVector<UnitInfo>& units, const QStringList& watchedServices, const QString& timestamp) {
    QVector<ServiceRow> rows;
    QSet<QString> seen;
    for (const UnitInfo& unit : units) {
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
    for (const QString& unit : watchedServices) {
        if (!seen.contains(unit)) {
            rows.append(ServiceRow{unit, QStringLiteral("unavailable"), QStringLiteral("unknown"), QStringLiteral("-"),
                                   QStringLiteral("Unit not found"), timestamp});
        }
    }
    return rows;
}

} // namespace

SystemdServiceProvider::SystemdServiceProvider(QObject* parent) : QObject(parent) {
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString&, const CommandResult& result, const QString& context) {
        if (context == QStringLiteral("systemd-list")) {
            QString error;
            if (!result.ok()) {
                error = result.startFailed ? QStringLiteral("systemctl not found or failed to start.") : result.standardError.trimmed();
            }
            emit servicesReady(ProviderParsers::parseSystemdListUnits(result.standardOutput, m_watched, currentTimestamp(), nullptr),
                               error);
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
            emit logsReady(QStringLiteral("journalctl -u %1").arg(unit), result.ok() ? result.standardOutput : result.standardError);
        } else if (context.startsWith(QStringLiteral("systemd-action:"))) {
            const QString action = context.section(QLatin1Char(':'), 1, 1);
            const QString unit = context.section(QLatin1Char(':'), 2);
            const QString message =
                result.ok() ? QStringLiteral("%1 %2 succeeded.").arg(action, unit) : QStringLiteral("%1 %2 failed.").arg(action, unit);
            emit actionFinished(message, result.ok() ? result.standardOutput : result.standardError);
        }
    });
}

void SystemdServiceProvider::refreshServices(const QStringList& watchedServices) {
    m_watched = watchedServices;
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        runSystemctlListUnits();
        return;
    }
    if (m_dbusListInFlight) {
        return;
    }

    m_dbusListInFlight = true;
    QDBusMessage message =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"), QStringLiteral("/org/freedesktop/systemd1"),
                                       QStringLiteral("org.freedesktop.systemd1.Manager"), QStringLiteral("ListUnits"));
    QDBusPendingCall pending = bus.asyncCall(message, kDBusTimeoutMs);
    auto* watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watchedServices](QDBusPendingCallWatcher* w) { handleListUnitsReply(w, watchedServices); });
}

void SystemdServiceProvider::handleListUnitsReply(QDBusPendingCallWatcher* watcher, const QStringList& watchedServices) {
    m_dbusListInFlight = false;
    const QDBusMessage reply = watcher->reply();
    watcher->deleteLater();
    if (reply.type() != QDBusMessage::ReplyMessage) {
        runSystemctlListUnits();
        return;
    }
    const QVector<UnitInfo> units = extractUnitsFromReply(reply);
    emit servicesReady(buildServiceRows(units, watchedServices, currentTimestamp()), QString());
}

void SystemdServiceProvider::refreshFailedCount() {
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        runSystemctlFailedCount();
        return;
    }
    if (m_dbusFailedInFlight) {
        return;
    }

    m_dbusFailedInFlight = true;
    QDBusMessage message =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"), QStringLiteral("/org/freedesktop/systemd1"),
                                       QStringLiteral("org.freedesktop.systemd1.Manager"), QStringLiteral("ListUnitsFiltered"));
    message << QStringList{QStringLiteral("failed")};
    QDBusPendingCall pending = bus.asyncCall(message, kDBusTimeoutMs);
    auto* watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &SystemdServiceProvider::handleFailedUnitsReply);
}

void SystemdServiceProvider::handleFailedUnitsReply(QDBusPendingCallWatcher* watcher) {
    m_dbusFailedInFlight = false;
    const QDBusMessage reply = watcher->reply();
    watcher->deleteLater();
    if (reply.type() != QDBusMessage::ReplyMessage) {
        runSystemctlFailedCount();
        return;
    }
    const QVector<UnitInfo> units = extractUnitsFromReply(reply);
    int count = 0;
    for (const UnitInfo& unit : units) {
        if (unit.name.endsWith(QStringLiteral(".service"))) {
            ++count;
        }
    }
    emit failedCountReady(count, QString());
}

void SystemdServiceProvider::runSystemctlListUnits() {
    m_runner.run(QStringLiteral("systemctl"),
                 {QStringLiteral("list-units"), QStringLiteral("--type=service"), QStringLiteral("--all"), QStringLiteral("--no-pager"),
                  QStringLiteral("--plain")},
                 15000, QStringLiteral("systemd-list"),
                 /*deduplicateContext=*/true);
}

void SystemdServiceProvider::runSystemctlFailedCount() {
    m_runner.run(QStringLiteral("systemctl"), {QStringLiteral("--failed"), QStringLiteral("--no-pager")}, 12000,
                 QStringLiteral("systemd-failed"),
                 /*deduplicateContext=*/true);
}

void SystemdServiceProvider::serviceLogs(const QString& unit, int tailLines) {
    m_runner.run(QStringLiteral("journalctl"),
                 {QStringLiteral("-u"), unit, QStringLiteral("-n"), QString::number(qMax(10, tailLines)), QStringLiteral("--no-pager")},
                 20000, QStringLiteral("systemd-logs:%1").arg(unit));
}

void SystemdServiceProvider::controlService(const QString& unit, const QString& action) {
    m_runner.run(QStringLiteral("systemctl"), {action, unit}, 30000, QStringLiteral("systemd-action:%1:%2").arg(action, unit));
}
