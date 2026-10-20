#include "SystemdServiceProvider.h"

#include "../parsers/ProviderParsers.h"
#include "../utils/TimeUtils.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QFileInfo>
#include <QRegularExpression>

namespace {
QDBusConnection busFor(ServiceScope s) {
    return s == ServiceScope::User ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
}
QStringList scopeArgs(ServiceScope s) {
    return s == ServiceScope::User ? QStringList{"--user"} : QStringList{};
}
QDBusMessage message(const QString& method) {
    return QDBusMessage::createMethodCall("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
                                          method);
}
} // namespace
SystemdServiceProvider::SystemdServiceProvider(QObject* parent) : QObject(parent) {
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const auto&, const CommandResult& r, const QString& context) {
        const auto scope = context.section(':', 1, 1) == "user" ? ServiceScope::User : ServiceScope::System;
        const int key = int(scope);
        if (context.startsWith("list:")) {
            if (!r.ok()) {
                finishScope(scope, r.standardError);
                return;
            }
            m_next[key] = ProviderParsers::parseSystemdListUnits(r.standardOutput, {}, currentTimestamp(), nullptr);
            for (auto& row : m_next[key])
                row.scope = scope;
            listFiles(scope);
        } else if (context.startsWith("files:")) {
            if (!r.ok()) {
                finishScope(scope, r.standardError);
                return;
            }
            for (const auto& line : r.standardOutput.split('\n', Qt::SkipEmptyParts)) {
                const auto parts = line.simplified().split(' ');
                if (parts.size() < 2 || !parts[0].endsWith(".service"))
                    continue;
                const auto unit = parts[0];
                bool found = false;
                for (auto& row : m_next[key])
                    if (row.unit == unit) {
                        row.unitFileState = parts[1];
                        found = true;
                        break;
                    }
                if (!found)
                    m_next[key].append({unit, "not-loaded", "inactive", "-", unit, currentTimestamp(), scope, parts[1]});
            }
            finishScope(scope);
        } else if (context.startsWith("logs:"))
            emit logsReady(context.mid(5), r.ok() ? r.standardOutput : r.standardError);
        else if (context.startsWith("action:")) {
            emit actionFinished(r.ok() ? tr("Action completed") : tr("Action failed"), r.ok() ? r.standardOutput : r.standardError);
            refreshServices({});
        }
    });
}
void SystemdServiceProvider::refreshServices(const QStringList&) {
    if (!m_pending.isEmpty())
        return;
    m_errors.clear();
    m_pending = {int(ServiceScope::System), int(ServiceScope::User)};
    listScope(ServiceScope::System);
    listScope(ServiceScope::User);
}
void SystemdServiceProvider::listScope(ServiceScope scope) {
    const int key = int(scope);
    m_next[key].clear();
    auto fallback = [this, scope]() {
        auto args = scopeArgs(scope);
        args << "list-units" << "--type=service" << "--all" << "--no-pager" << "--no-legend" << "--plain";
        m_runner.run("systemctl", args, 10000, "list:" + QString(scope == ServiceScope::User ? "user" : "system"));
    };
    auto bus = busFor(scope);
    if (!bus.isConnected()) {
        fallback();
        return;
    }
    auto* watcher = new QDBusPendingCallWatcher(bus.asyncCall(message("ListUnits"), 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, scope, key, fallback](QDBusPendingCallWatcher* w) {
        const auto reply = w->reply();
        w->deleteLater();
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            fallback();
            return;
        }
        const QDBusArgument a = reply.arguments()[0].value<QDBusArgument>();
        a.beginArray();
        while (!a.atEnd()) {
            QString name, description, load, active, sub, following, jobType;
            QDBusObjectPath path, jobPath;
            quint32 job;
            a.beginStructure();
            a >> name >> description >> load >> active >> sub >> following >> path >> job >> jobType >> jobPath;
            a.endStructure();
            if (name.endsWith(".service"))
                m_next[key].append({name, load, active, sub, description, currentTimestamp(), scope, {}});
        }
        a.endArray();
        listFiles(scope);
    });
}
void SystemdServiceProvider::listFiles(ServiceScope scope) {
    // CLI fallback also enumerates not-loaded units. It is asynchronous and locale-stable.
    auto args = scopeArgs(scope);
    args << "list-unit-files" << "--type=service" << "--no-pager" << "--no-legend";
    m_runner.run("systemctl", args, 10000, "files:" + QString(scope == ServiceScope::User ? "user" : "system"));
}
void SystemdServiceProvider::finishScope(ServiceScope scope, const QString& error) {
    const int key = int(scope);
    if (error.isEmpty())
        m_rows[key] = m_next[key];
    else
        m_errors[key] = (scope == ServiceScope::User ? "User: " : "System: ") + error;
    m_pending.remove(key);
    if (!m_pending.isEmpty())
        return;
    auto rows = m_rows.value(0);
    rows += m_rows.value(1);
    const auto errors = m_errors.values().join("\n");
    int failed = 0;
    for (const auto& row : rows)
        if (row.activeState == "failed")
            ++failed;
    emit servicesReady(rows, errors);
    emit failedCountReady(failed, errors);
}
void SystemdServiceProvider::refreshFailedCount() {
    refreshServices({});
}
bool SystemdServiceProvider::isProtectedUnit(const QString& unit) {
    static const QStringList critical{"dbus.service",
                                      "dbus-broker.service",
                                      "sddm.service",
                                      "systemd-logind.service",
                                      "systemd-udevd.service",
                                      "plasma-kwin_wayland.service",
                                      "plasma-kwin_x11.service",
                                      "plasma-plasmashell.service",
                                      "plasma-ksmserver.service"};
    return critical.contains(unit);
}
void SystemdServiceProvider::serviceLogs(const QString& unit, ServiceScope scope) {
    auto args = scopeArgs(scope);
    args << "-u" << unit << "-n" << "300" << "--no-pager";
    m_runner.run("journalctl", args, 15000, "logs:" + QString(scope == ServiceScope::User ? "user:" : "system:") + unit);
}
void SystemdServiceProvider::controlService(const QString& unit, const QString& action, ServiceScope scope) {
    static const QRegularExpression name(R"(^[A-Za-z0-9_@.\\:\-]+\.service$)");
    if (isProtectedUnit(unit) || !name.match(unit).hasMatch() || !QStringList{"start", "stop", "restart"}.contains(action)) {
        emit actionFinished(tr("Read-only service"), unit);
        return;
    }
    auto args = scopeArgs(scope);
    args << "--no-ask-password" << action << "--" << unit;
    m_runner.run("systemctl", args, 30000, "action:" + QString(scope == ServiceScope::User ? "user:" : "system:") + action + ":" + unit);
}
