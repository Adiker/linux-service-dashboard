#include "InventoryProvider.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

InventoryProvider::InventoryProvider(QObject* parent) : QObject(parent) {
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const auto&, const CommandResult& r, const QString& context) {
        const auto module = context.section(':', 0, 0), tag = context.section(':', 1);
        QString error;
        if (r.ok())
            m_rows[module] += parse(module, tag, r.standardOutput, &error);
        else
            error = r.standardError.trimmed();
        if (!error.isEmpty())
            m_errors[module].append(tag + ": " + error);
        if (module == "backups")
            for (auto& row : m_rows[module]) {
                while (row.cells.size() < 8)
                    row.cells.append("-");
                row.cells[6] = tr("Unknown");
                for (const auto& timer : m_timers)
                    if (timer.meta.value("scope") == row.meta.value("scope") && timer.meta.value("activates") == row.meta.value("unit"))
                        row.cells[6] = timer.cells.value(2);
            }
        if (--m_pending[module] == 0) {
            m_pending.remove(module);
            emit ready(module, headers(module), m_rows.take(module), m_errors.take(module).join("\n"));
        }
    });
}
void InventoryProvider::start(const QString& module, const QVector<InventoryCommand>& commands) {
    if (m_pending.contains(module))
        return;
    m_rows[module].clear();
    m_errors[module].clear();
    m_pending[module] = commands.size();
    if (commands.isEmpty()) {
        m_pending.remove(module);
        emit ready(module, headers(module), {}, {});
        return;
    }
    for (const auto& c : commands)
        m_runner.run(c.program, c.arguments, 15000, module + ":" + c.tag);
}
QStringList InventoryProvider::headers(const QString& module) {
    if (module == "backups")
        return {tr("Task"),      tr("Scope"),    tr("State"),    tr("Last result"),
                tr("Exit code"), tr("Finished"), tr("Next run"), tr("Backup integrity")};
    if (module == "snapshots")
        return {tr("Source"), tr("Started"), tr("Finished"), tr("Snapshot"), tr("Integrity")};
    if (module == "sync")
        return {tr("Name"), tr("Service state"), tr("Scope / filesystem"), tr("Transfer completion")};
    if (module == "timers")
        return {tr("Unit"), tr("Scope"), tr("Next run"), tr("Last run"), tr("Activates")};
    if (module == "sockets")
        return {tr("Unit"), tr("Scope"), tr("Listen"), tr("Activates")};
    if (module == "images")
        return {tr("Image"), tr("Tag"), tr("ID"), tr("Size"), tr("Created")};
    if (module == "volumes")
        return {tr("Name"), tr("Driver"), tr("Scope")};
    if (module == "dockerStats")
        return {tr("Container"), tr("CPU"), tr("Memory")};
    return {tr("Name"), tr("State"), tr("Details")};
}
QVector<CenterRecord> InventoryProvider::parse(const QString& module, const QString& tag, const QString& output, QString* error) {
    QVector<CenterRecord> rows;
    if (module == "backups") {
        for (const auto& block : output.split("\n\n", Qt::SkipEmptyParts)) {
            QHash<QString, QString> properties;
            for (const auto& line : block.split('\n', Qt::SkipEmptyParts)) {
                const int i = line.indexOf('=');
                if (i > 0)
                    properties[line.left(i)] = line.mid(i + 1);
            }
            const auto unit = properties.value("Id");
            if (unit.isEmpty())
                continue;
            const auto result = properties.value("Result");
            const bool hasRun = !properties.value("ExecMainExitTimestamp").isEmpty() && properties.value("ExecMainExitTimestamp") != "n/a";
            rows.append({tag + ":" + unit,
                         {unit, tag, properties.value("ActiveState") + "/" + properties.value("SubState"), hasRun ? result : tr("Unknown"),
                          hasRun ? properties.value("ExecMainStatus") : "-", properties.value("ExecMainExitTimestamp"), tr("Unknown"),
                          tr("Not verified")},
                         {},
                         {{"unit", unit}, {"scope", tag}},
                         hasRun && result != "success"});
        }
        if (rows.isEmpty() && !output.trimmed().isEmpty())
            *error = tr("Invalid backup task metadata");
    } else if (module == "snapshots") {
        QJsonParseError e;
        const auto doc = QJsonDocument::fromJson(output.toUtf8(), &e);
        if (!doc.isArray()) {
            *error = tr("Invalid Kopia snapshot JSON: %1").arg(e.errorString());
            return {};
        }
        for (const auto& value : doc.array()) {
            const auto o = value.toObject();
            const auto source = o["source"].toObject();
            const auto id = o["id"].toString();
            rows.append({id,
                         {source["path"].toString(), o["startTime"].toString(), o["endTime"].toString(), id, tr("Listed; not verified")},
                         {},
                         {},
                         o.contains("incomplete") && !o["incomplete"].toString().isEmpty()});
        }
    } else if (module == "timers" || module == "sockets") {
        QJsonParseError e;
        const auto doc = QJsonDocument::fromJson(output.toUtf8(), &e);
        if (!doc.isArray()) {
            *error = tr("Invalid structured systemctl output: %1").arg(e.errorString());
            return {};
        }
        const auto date = [](QJsonValue v) {
            const qint64 usec = v.toVariant().toLongLong();
            return usec > 0 && usec < 9000000000000000LL ? QDateTime::fromMSecsSinceEpoch(usec / 1000).toString(Qt::ISODate) : QString("-");
        };
        for (const auto& value : doc.array()) {
            const auto o = value.toObject();
            const auto unit = o["unit"].toString();
            if (unit.isEmpty())
                continue;
            QStringList cells{unit, tag};
            if (module == "timers")
                cells << date(o["next"]) << date(o["last"]) << o["activates"].toString();
            else
                cells << o["listen"].toString() << o["activates"].toString();
            rows.append({tag + ":" + unit, cells, {tag}, {{"unit", unit}, {"scope", tag}, {"activates", o["activates"].toString()}}});
        }
    } else if (module == "images" || module == "volumes" || module == "dockerStats") {
        for (const auto& line : output.split('\n', Qt::SkipEmptyParts)) {
            QJsonParseError e;
            const auto doc = QJsonDocument::fromJson(line.toUtf8(), &e);
            if (!doc.isObject()) {
                *error = tr("Invalid Docker JSON");
                continue;
            }
            const auto o = doc.object();
            if (module == "images")
                rows.append({o["ID"].toString() + ":" + o["Tag"].toString(),
                             {o["Repository"].toString(), o["Tag"].toString(), o["ID"].toString(), o["Size"].toString(),
                              o["CreatedSince"].toString()},
                             {},
                             {}});
            else if (module == "volumes")
                rows.append({o["Name"].toString(), {o["Name"].toString(), o["Driver"].toString(), o["Scope"].toString()}, {}, {}});
            else
                rows.append({o["ID"].toString(), {o["Name"].toString(), o["CPUPerc"].toString(), o["MemUsage"].toString()}, {}, {}});
        }
    }
    return rows;
}
void InventoryProvider::refresh(const QString& module) {
    if (m_pending.contains(module))
        return;
    if (module == "backups") {
        QVector<InventoryCommand> commands;
        for (const auto& scope : {QString("system"), QString("user")}) {
            QStringList args;
            if (scope == "user")
                args << "--user";
            args << "show" << "--no-pager" << "--property=Id,ActiveState,SubState,Result,ExecMainStatus,ExecMainExitTimestamp" << "--";
            const auto before = args.size();
            for (const auto& service : m_services) {
                if (service.meta.value("scope") != scope)
                    continue;
                if (service.cells.join(' ').contains(
                        QRegularExpression("kopia|timeshift|backup", QRegularExpression::CaseInsensitiveOption)))
                    args << service.meta.value("unit").toString();
            }
            if (args.size() > before)
                commands.append({"systemctl", args, scope});
        }
        if (commands.isEmpty()) {
            emit ready(module, headers(module),
                       {{"backup-unknown",
                         {tr("Backup tasks"), "-", tr("No detected task"), tr("Unknown"), "-", "-", tr("Unknown"), tr("Not verified")},
                         {},
                         {}}},
                       {});
        } else
            start(module, commands);
    } else if (module == "snapshots") {
        QString binary = QStandardPaths::findExecutable("kopia");
        if (binary.isEmpty() && QFileInfo::exists("/opt/KopiaUI/resources/server/kopia"))
            binary = "/opt/KopiaUI/resources/server/kopia";
        const auto config = QSettings().value("backups/kopiaConfig").toString();
        if (binary.isEmpty() || !QFileInfo(config).isFile()) {
            emit ready(module, headers(module), {},
                       tr("Select an existing Kopia repository configuration. Credentials are never requested or stored."));
            return;
        }
        start(module, {{binary,
                        {"--config-file", config, "--no-persist-credentials", "--no-use-keyring", "--disable-file-logging",
                         "--disable-content-log", "snapshot", "list", "--json", "--max-results=20"},
                        "kopia"}});
    } else if (module == "sync") {
        QVector<CenterRecord> rows;
        for (const auto& service : m_services) {
            if (!service.cells.join(' ').contains(QRegularExpression("syncthing|rclone", QRegularExpression::CaseInsensitiveOption)))
                continue;
            auto r = service;
            r.cells = {service.cells.value(0), service.cells.value(1), service.meta.value("scope").toString(),
                       tr("Unknown; service state only")};
            rows.append(r);
        }
        for (const auto& mount : m_mounts)
            if (mount.cells.value(2) == "fuse.rclone") {
                auto r = mount;
                r.cells = {mount.cells.value(0), mount.cells.value(1), "fuse.rclone", tr("Unknown; mount state only")};
                rows.append(r);
            }
        if (rows.isEmpty())
            rows.append({"sync-unknown", {tr("Synchronization"), tr("No detected service"), "-", tr("Unknown")}, {}, {}});
        emit ready(module, headers(module), rows, {});
    } else if (module == "timers" || module == "sockets") {
        const auto action = module == "timers" ? "list-timers" : "list-sockets";
        start(module, {{"systemctl", {action, "--all", "--no-pager", "--output=json"}, "system"},
                       {"systemctl", {"--user", action, "--all", "--no-pager", "--output=json"}, "user"}});
    } else if (module == "images")
        start(module, {{"docker", {"image", "ls", "--format", "{{json .}}"}, "images"}});
    else if (module == "volumes")
        start(module, {{"docker", {"volume", "ls", "--format", "{{json .}}"}, "volumes"}});
    else if (module == "dockerStats")
        start(module, {{"docker", {"stats", "--no-stream", "--format", "{{json .}}"}, "stats"}});
    else if (module == "autostart") {
        QVector<CenterRecord> rows;
        QSet<QString> seen;
        QStringList roots{QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)};
        const auto dirs = qEnvironmentVariable("XDG_CONFIG_DIRS", "/etc/xdg").split(':', Qt::SkipEmptyParts);
        roots += dirs;
        for (const auto& root : roots) {
            QDir dir(root + "/autostart");
            for (const auto& file : dir.entryList({"*.desktop"}, QDir::Files)) {
                if (seen.contains(file))
                    continue;
                seen.insert(file);
                QSettings entry(dir.filePath(file), QSettings::IniFormat);
                entry.beginGroup("Desktop Entry");
                const bool hidden = entry.value("Hidden", false).toBool() || !entry.value("X-GNOME-Autostart-enabled", true).toBool();
                rows.append({file, {entry.value("Name", file).toString(), hidden ? tr("Disabled") : tr("Configured"), file}, {}, {}});
            }
        }
        emit ready(module, headers(module), rows, {});
    }
}
