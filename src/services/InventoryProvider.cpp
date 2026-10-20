#include "InventoryProvider.h"

#include "SystemInventory.h"
#include "ToolLauncher.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>

InventoryProvider::InventoryProvider(QObject* parent) : QObject(parent) {
    connect(&m_firewall, &FirewallProvider::ready, this,
            [this](const auto& rows, const auto& error) { emit ready("firewall", headers("firewall"), rows, error); });
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const auto&, const CommandResult& r, const QString& context) {
        if (context.startsWith("desktop-launch:")) {
            emit launchCompleted(r.ok());
            if (!r.ok())
                emit launchFailed(r.standardError);
            return;
        }
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
                for (const auto& timer : (centerModuleEnabled("timers") ? m_timers : QVector<CenterRecord>{}))
                    if (timer.meta.value("scope") == row.meta.value("scope") && timer.meta.value("activates") == row.meta.value("unit"))
                        row.cells[6] = timer.cells.value(2);
            }
        if (--m_pending[module] == 0) {
            m_pending.remove(module);
            auto collected = m_rows.take(module);
            if (module == "packages" || module == "vms") {
                QMap<QString, CenterRecord> unique;
                for (const auto& row : collected)
                    if (!unique.contains(row.id) || row.meta.value("foreign").toBool() || row.meta.value("running").toBool())
                        unique[row.id] = row;
                collected = unique.values().toVector();
            }
            emit ready(module, headers(module), collected, m_errors.take(module).join("\n"));
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
    if (QStringList{"apps", "packages", "metrics", "vms", "tools", "filesystems", "audio", "bluetooth", "power", "printing", "usb"}
            .contains(module))
        return SystemInventory::headers(module);
    if (module == "network")
        return {
            tr("Interface / destination"), tr("State / gateway"), tr("Addresses / device"), tr("Details"), tr("RX bytes"), tr("TX bytes")};
    if (module == "ports")
        return {tr("Protocol"), tr("State"), tr("Local address"), tr("Peer"), tr("Process (if permitted)")};
    if (module == "firewall")
        return {tr("Zone / property"), tr("Value"), tr("Details")};
    if (module == "servers")
        return {tr("Name"), tr("State"), tr("Scope"), tr("Description")};
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
    if (QStringList{"packages", "vms", "filesystems", "audio", "bluetooth", "power", "printing", "usb"}.contains(module))
        return SystemInventory::parse(module, tag, output, error);
    QVector<CenterRecord> rows;
    if (module == "network") {
        if (tag == "dns") {
            for (const auto& line : output.split('\n', Qt::SkipEmptyParts))
                rows.append({"dns:" + line, {line, "", "", "", "", ""}, {tr("DNS")}, {}});
        } else {
            QJsonParseError e;
            const auto doc = QJsonDocument::fromJson(output.toUtf8(), &e);
            if (!doc.isArray()) {
                *error = tr("Invalid network JSON: %1").arg(e.errorString());
                return {};
            }
            for (const auto& value : doc.array()) {
                const auto o = value.toObject();
                if (tag == "addresses") {
                    QStringList addresses;
                    for (const auto& v : o["addr_info"].toArray()) {
                        const auto a = v.toObject();
                        addresses << a["local"].toString() + "/" + QString::number(a["prefixlen"].toInt());
                    }
                    const auto stats = o["stats64"].toObject();
                    const auto name = o["ifname"].toString();
                    rows.append({name,
                                 {name, o["operstate"].toString(), addresses.join(", "), o["address"].toString(),
                                  QString::number(stats["rx"].toObject()["bytes"].toVariant().toULongLong()),
                                  QString::number(stats["tx"].toObject()["bytes"].toVariant().toULongLong())},
                                 {tr("Interfaces")},
                                 {}});
                } else
                    rows.append(
                        {tag + ":" + QString::number(rows.size()),
                         {o["dst"].toString("default"), o["gateway"].toString(), o["dev"].toString(), o["protocol"].toString(), "", ""},
                         {tag},
                         {}});
            }
        }
    } else if (module == "ports") {
        for (const auto& line : output.split('\n', Qt::SkipEmptyParts)) {
            const auto f = line.simplified().split(' ');
            if (f.size() < 6) {
                *error = tr("Invalid socket listing");
                continue;
            }
            rows.append({line, {f[0], f[1], f[4], f[5], f.size() > 6 ? f.mid(6).join(' ') : tr("Unknown / not permitted")}, {f[0]}, {}});
        }
    } else if (module == "firewall") {
        QString zone;
        for (const auto& line : output.split('\n', Qt::SkipEmptyParts)) {
            if (!line.startsWith(' '))
                zone = line.trimmed();
            else
                rows.append({zone + ":" + QString::number(rows.size()),
                             {line.trimmed().section(':', 0, 0), line.trimmed().section(':', 1), tr("Read-only")},
                             {zone},
                             {}});
        }
        if (rows.isEmpty())
            *error = tr("No firewall zone data returned");
    } else if (module == "backups") {
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
    if (QStringList{"sync", "servers", "backups"}.contains(module) && !m_serviceRead && centerModuleEnabled("systemd"))
        return;
    if (QStringList{"apps", "tools", "metrics", "vms"}.contains(module)) {
        m_pending[module] = 1;
        auto* watcher = new QFutureWatcher<CenterSnapshot>(this);
        connect(watcher, &QFutureWatcher<CenterSnapshot>::finished, this, [this, module, watcher]() {
            auto data = watcher->result();
            watcher->deleteLater();
            m_pending.remove(module);
            if (module == "metrics" && !data.rows.isEmpty()) {
                auto& cpu = data.rows[0];
                const auto total = cpu.meta.value("total").toULongLong(), idle = cpu.meta.value("idle").toULongLong();
                if (m_cpuTotal && total > m_cpuTotal && idle >= m_cpuIdle) {
                    const double percent = qBound(0.0, 100.0 * (1.0 - double(idle - m_cpuIdle) / double(total - m_cpuTotal)), 100.0);
                    cpu.cells[1] = QString::number(percent, 'f', 1) + "%";
                    cpu.meta["percent"] = percent;
                }
                m_cpuTotal = total;
                m_cpuIdle = idle;
            }
            if (module == "vms" && !QStandardPaths::findExecutable("VBoxManage").isEmpty()) {
                start(module, {{"VBoxManage", {"list", "vms"}, "registered"}, {"VBoxManage", {"list", "runningvms"}, "running"}});
                m_rows[module] += data.rows;
            } else
                emit ready(module, data.headers, data.rows, data.error);
        });
        watcher->setFuture(QtConcurrent::run([module]() { return SystemInventory::local(module); }));
        return;
    }
    if (module == "packages") {
        QVector<InventoryCommand> commands;
        if (!QStandardPaths::findExecutable("pacman").isEmpty()) {
            commands.append({"pacman", {"-Q"}, "pacman"});
            commands.append({"pacman", {"-Qm"}, "foreign"});
        } else if (!QStandardPaths::findExecutable("dpkg-query").isEmpty())
            commands.append({"dpkg-query", {"-W", "-f=${db:Status-Status}\t${binary:Package}\t${Version}\n"}, "dpkg"});
        else if (!QStandardPaths::findExecutable("rpm").isEmpty())
            commands.append({"rpm", {"-qa", "--qf", "%{NAME}\t%{VERSION}-%{RELEASE}\t%{ARCH}\n"}, "rpm"});
        if (!QStandardPaths::findExecutable("flatpak").isEmpty())
            commands.append({"flatpak", {"list", "--columns=application,name,version,installation,ref"}, "flatpak"});
        if (commands.isEmpty()) {
            emit ready(module, headers(module), {}, tr("No supported package manager installed"));
            return;
        }
        start(module, commands);
        return;
    }
    if (module == "filesystems") {
        start(module, {{"lsblk", {"-J", "-b", "-o", "NAME,PATH,TYPE,SIZE,FSTYPE,MOUNTPOINTS,FSAVAIL,FSUSE%"}, "devices"}});
        return;
    }
    if (module == "audio") {
        start(module, {{"pw-dump", {}, "PipeWire"}});
        return;
    }
    if (module == "bluetooth") {
        start(module, {{"bluetoothctl", {"--timeout", "5", "list"}, "Controllers"},
                       {"bluetoothctl", {"--timeout", "5", "devices", "Connected"}, "Connected devices"}});
        return;
    }
    if (module == "power") {
        QVector<InventoryCommand> commands{{"upower", {"--dump"}, "UPower"}};
        if (!QStandardPaths::findExecutable("powerprofilesctl").isEmpty())
            commands.append({"powerprofilesctl", {"get"}, "profile"});
        start(module, commands);
        return;
    }
    if (module == "printing") {
        start(module, {{"lpstat", {"-p", "-d"}, "CUPS"}});
        return;
    }
    if (module == "usb") {
        start(module, {{"lsusb", {}, "USB"}});
        return;
    }
    if (module == "network")
        start(module, {{"ip", {"-s", "-j", "address", "show"}, "addresses"},
                       {"ip", {"-j", "route", "show"}, "IPv4 routes"},
                       {"ip", {"-6", "-j", "route", "show"}, "IPv6 routes"},
                       {"nmcli", {"-t", "-f", "GENERAL.DEVICE,IP4.DNS,IP6.DNS", "device", "show"}, "dns"}});
    else if (module == "ports")
        start(module, {{"ss", {"-H", "-lntup"}, "ports"}});
    else if (module == "firewall")
        m_firewall.refresh();
    else if (module == "servers") {
        QVector<CenterRecord> rows;
        for (const auto& service : m_services) {
            if (!service.cells.join(' ').contains(
                    QRegularExpression("jellyfin|caddy|smb|samba|sshd|sunshine|mediamtx|autobrr|postgresql|filebrowser|mcp",
                                       QRegularExpression::CaseInsensitiveOption)))
                continue;
            auto r = service;
            r.cells = {service.cells.value(0), service.cells.value(1), service.meta.value("scope").toString(), service.cells.value(2)};
            r.groups = {service.meta.value("scope").toString()};
            rows.append(r);
        }
        for (const auto& container : (centerModuleEnabled("docker") ? m_containers : QVector<CenterRecord>{})) {
            auto r = container;
            r.cells = {container.cells.value(0), container.cells.value(1), "Docker",
                       container.cells.value(3) + " | " + container.cells.value(4)};
            r.groups = {"Docker"};
            rows.append(r);
        }
        emit ready(module, headers(module), rows,
                   !centerModuleEnabled("systemd")
                       ? tr("Service module disabled; state is unknown")
                       : QStringList{m_serviceError, centerModuleEnabled("docker") ? m_containerError : QString()}.join("\n").trimmed());
    } else if (module == "backups") {
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
                       !centerModuleEnabled("systemd") ? tr("Service module disabled; state is unknown") : m_serviceError);
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
        emit ready(module, headers(module), rows,
                   !centerModuleEnabled("systemd") ? tr("Service module disabled; state is unknown") : m_serviceError);
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

void InventoryProvider::launchDesktop(const QString& file) {
    const auto error = ToolLauncher::desktopError(file);
    if (!error.isEmpty()) {
        emit launchFailed(error);
        emit launchCompleted(false);
        return;
    }
    m_runner.run("gio", {"launch", file}, 10000, "desktop-launch:" + file);
}
