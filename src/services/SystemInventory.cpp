#include "SystemInventory.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <functional>

namespace {
QString t(const char* s) {
    return QCoreApplication::translate("SystemInventory", s);
}
QString smallFile(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.read(128 * 1024)).trimmed() : QString();
}
QHash<QString, QString> desktopFields(const QString& path) {
    QHash<QString, QString> values;
    bool entry = false;
    for (const auto& line : smallFile(path).split('\n')) {
        if (line.startsWith('[')) {
            entry = line.trimmed() == "[Desktop Entry]";
            continue;
        }
        if (!entry || line.startsWith('#'))
            continue;
        const auto i = line.indexOf('=');
        if (i < 1)
            continue;
        auto value = line.mid(i + 1).trimmed();
        value.replace("\\s", " ").replace("\\n", "\n").replace("\\t", "\t").replace("\\\\", "\\");
        values[line.left(i).trimmed()] = value;
    }
    return values;
}
QString category(const QString& cats) {
    const auto c = cats.split(';');
    if (c.contains("Game"))
        return t("Games");
    if (c.contains("Development"))
        return t("Development");
    if (c.contains("AudioVideo") || c.contains("Audio") || c.contains("Video"))
        return t("Multimedia");
    if (c.contains("Graphics"))
        return t("Graphics");
    if (c.contains("Office"))
        return t("Office");
    if (c.contains("Network"))
        return t("Internet and communication");
    if (c.contains("System") || c.contains("Utility") || c.contains("Settings"))
        return t("System and tools");
    return t("Other");
}
QString gib(qulonglong bytes) {
    return QString::number(double(bytes) / (1024 * 1024 * 1024), 'f', 1) + " GiB";
}
} // namespace
QStringList SystemInventory::dataRoots() {
    QStringList roots{QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)};
    roots += qEnvironmentVariable("XDG_DATA_DIRS", "/usr/local/share:/usr/share").split(':', Qt::SkipEmptyParts);
    roots << QDir::homePath() + "/.local/share/flatpak/exports/share" << "/var/lib/flatpak/exports/share";
    roots.removeDuplicates();
    return roots;
}
CenterSnapshot SystemInventory::applications(const QStringList& roots, const QString& locale, const QString& desktop) {
    CenterSnapshot out;
    out.headers = headers("apps");
    QSet<QString> seen;
    const auto currentDesktops = desktop.split(':', Qt::SkipEmptyParts);
    for (const auto& root : roots) {
        const QString base = root + "/applications/";
        QDirIterator it(base, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto path = it.next();
            QString id = QDir(base).relativeFilePath(path);
            id.replace('/', '-');
            if (seen.contains(id))
                continue;
            seen.insert(id);
            const auto fields = desktopFields(path);
            if (fields.value("Hidden") == "true" || fields.value("NoDisplay") == "true" || fields.value("Type") != "Application")
                continue;
            const auto only = fields.value("OnlyShowIn").split(';', Qt::SkipEmptyParts),
                       notIn = fields.value("NotShowIn").split(';', Qt::SkipEmptyParts);
            bool allowed = only.isEmpty();
            for (const auto& d : currentDesktops) {
                if (only.contains(d))
                    allowed = true;
                if (notIn.contains(d)) {
                    allowed = false;
                    break;
                }
            }
            if (!allowed)
                continue;
            const auto tryExec = fields.value("TryExec");
            if (!tryExec.isEmpty() && QStandardPaths::findExecutable(tryExec).isEmpty())
                continue;
            const auto name =
                fields.value("Name[" + locale + "]", fields.value("Name[" + locale.section('_', 0, 0) + "]", fields.value("Name", id)));
            const auto group = category(fields.value("Categories"));
            const auto source = fields.contains("X-Flatpak")            ? "Flatpak"
                                : path.startsWith(roots.value(0) + "/") ? t("User desktop entry")
                                                                        : t("Desktop entry");
            out.rows.append(
                {id, {name, group, source, fields.value("Comment"), id}, {group}, {{"desktop", path}, {"icon", fields.value("Icon")}}});
        }
    }
    return out;
}
QStringList SystemInventory::headers(const QString& module) {
    if (module == "apps")
        return {t("Name"), t("Category"), t("Source"), t("Description"), t("Desktop ID")};
    if (module == "packages")
        return {t("Name"), t("Version"), t("Source"), t("Kind")};
    if (module == "filesystems")
        return {t("Device"), t("Type"), t("Size"), t("Filesystem"), t("Mountpoints"), t("Available"), t("Used")};
    if (module == "metrics")
        return {t("Resource"), t("Value"), t("Details")};
    if (module == "vms")
        return {t("Name / process"), t("State"), t("Backend"), t("ID")};
    if (module == "audio")
        return {t("Node"), t("Class"), t("ID")};
    return {t("Name"), t("State / value"), t("Details")};
}
CenterSnapshot SystemInventory::local(const QString& module) {
    CenterSnapshot out;
    out.headers = headers(module);
    if (module == "apps")
        return applications(dataRoots(), QLocale::system().name(), qEnvironmentVariable("XDG_CURRENT_DESKTOP"));
    if (module == "metrics") {
        const auto cpu = smallFile("/proc/stat").section('\n', 0, 0).simplified().split(' ');
        qulonglong total = 0, idle = 0;
        for (int i = 1; i < qMin(9, cpu.size()); ++i)
            total += cpu[i].toULongLong();
        if (cpu.size() > 5)
            idle = cpu[4].toULongLong() + cpu[5].toULongLong();
        if (total == 0) {
            out.error = t("CPU counters unavailable");
            return out;
        }
        out.rows.append({"cpu", {"CPU", t("Sampling…"), smallFile("/proc/loadavg")}, {}, {{"total", total}, {"idle", idle}}});
        QHash<QString, qulonglong> memory;
        for (const auto& line : smallFile("/proc/meminfo").split('\n')) {
            const auto f = line.simplified().split(' ');
            if (f.size() > 1)
                memory[f[0]] = f[1].toULongLong() * 1024;
        }
        const auto ram = memory.value("MemTotal:"), available = memory.value("MemAvailable:", memory.value("MemFree:"));
        if (!ram) {
            out.error = t("Memory counters unavailable");
            return out;
        }
        const auto used = ram - qMin(ram, available);
        out.rows.append({"memory",
                         {t("Memory"), gib(used) + " / " + gib(ram), t("Available: %1").arg(gib(available))},
                         {},
                         {{"percent", 100.0 * used / ram}}});
        out.rows.append(
            {"swap",
             {"Swap", gib(memory.value("SwapTotal:") - memory.value("SwapFree:")) + " / " + gib(memory.value("SwapTotal:")), ""},
             {},
             {}});
        for (const auto& card : QDir("/sys/class/drm").entryList({"card*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
            const auto base = "/sys/class/drm/" + card + "/device/";
            if (smallFile(base + "vendor").isEmpty())
                continue;
            const auto busy = smallFile(base + "gpu_busy_percent");
            const auto vramTotal = smallFile(base + "mem_info_vram_total");
            out.rows.append({"gpu:" + card,
                             {"GPU / " + card, busy.isEmpty() ? t("Unknown") : busy + "%",
                              vramTotal.isEmpty() ? t("VRAM unavailable")
                                                  : "VRAM " + gib(smallFile(base + "mem_info_vram_used").toULongLong()) + " / " +
                                                        gib(vramTotal.toULongLong())},
                             {},
                             {}});
        }
        out.rows.append({"kernel", {t("Kernel"), smallFile("/proc/sys/kernel/osrelease"), ""}, {}, {}});
    } else if (module == "vms") {
        for (const auto& pid : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            bool numeric;
            pid.toUInt(&numeric);
            if (!numeric)
                continue;
            const auto name = smallFile("/proc/" + pid + "/comm");
            if (name.startsWith("qemu-system") || name == "qemu-kvm")
                out.rows.append({"qemu:" + pid, {name, t("Process detected; guest state unknown"), "QEMU", pid}, {"QEMU"}, {}});
        }
    } else if (module == "tools") {
        struct Tool {
            const char* program;
            const char* group;
            bool gui;
        };
        const Tool tools[] = {{"timeshift-launcher", "Backups", true},
                              {"rclone", "Backups", false},
                              {"syncthing", "Backups", false},
                              {"coolercontrol", "Hardware", true},
                              {"lact", "Hardware", true},
                              {"corectrl", "Hardware", true},
                              {"qpwgraph", "Audio", true},
                              {"easyeffects", "Audio", true},
                              {"pavucontrol", "Audio", true},
                              {"firewall-config", "Network", true},
                              {"nm-connection-editor", "Network", true},
                              {"gsmartcontrol", "Disks", true},
                              {"gnome-disks", "Disks", true},
                              {"plasma-systemmonitor", "Monitoring", true},
                              {"missioncenter", "Monitoring", true},
                              {"resources", "Monitoring", true},
                              {"VirtualBox", "Virtualization", true},
                              {"wine", "Virtualization", false},
                              {"docker", "Virtualization", false},
                              {"qemu-system-x86_64", "Virtualization", false},
                              {"git", "Development", false},
                              {"cmake", "Development", false},
                              {"c++", "Development", false},
                              {"python", "Development", false},
                              {"node", "Development", false},
                              {"go", "Development", false},
                              {"rustup", "Development", false},
                              {"dotnet", "Development", false},
                              {"adb", "Development", false},
                              {"qtcreator", "Development", true},
                              {"code", "Development", true}};
        for (const auto& tool : tools) {
            const auto executable = QStandardPaths::findExecutable(tool.program);
            QVariantMap meta;
            if (tool.gui && !executable.isEmpty())
                meta["program"] = tool.program;
            out.rows.append({tool.program,
                             {tool.program, executable.isEmpty() ? t("Not installed") : t("Installed"), executable},
                             {t(tool.group)},
                             meta});
        }
        const auto apps = applications(dataRoots(), QLocale::system().name(), qEnvironmentVariable("XDG_CURRENT_DESKTOP"));
        for (const auto& app : apps.rows)
            if (app.id == "kopia-ui.desktop" || app.id.contains("bottles", Qt::CaseInsensitive) ||
                app.id.contains("defguard", Qt::CaseInsensitive) || app.id.contains("proton.vpn", Qt::CaseInsensitive)) {
                auto r = app;
                r.cells = {app.cells.value(0), t("Installed"), app.id};
                r.groups = {t("Specialist tools")};
                out.rows.append(r);
            }
    }
    return out;
}
QVector<CenterRecord> SystemInventory::parse(const QString& module, const QString& tag, const QString& output, QString* error) {
    QVector<CenterRecord> rows;
    if (module == "packages") {
        for (const auto& line : output.split('\n', Qt::SkipEmptyParts)) {
            if (tag == "flatpak") {
                const auto f = line.split('\t');
                if (f.size() < 4) {
                    *error = t("Invalid Flatpak listing");
                    continue;
                }
                const auto id = "flatpak:" + f[0] + ":" + f[3] + ":" + f.value(4);
                rows.append({id, {f[1] + " (" + f[0] + ")", f[2], "Flatpak / " + f[3], f.value(4)}, {"Flatpak"}, {}});
            } else {
                const auto f = tag == "dpkg" || tag == "rpm" ? line.split('\t') : line.simplified().split(' ');
                if (f.size() < 2) {
                    *error = t("Invalid package listing");
                    continue;
                }
                rows.append({"package:" + f[0],
                             {f[0], f[1],
                              tag == "foreign"  ? t("Pacman (foreign)")
                              : tag == "pacman" ? "Pacman"
                                                : tag,
                              t("Package")},
                             {tag == "foreign" ? "Pacman" : tag},
                             {{"foreign", tag == "foreign"}}});
            }
        }
    } else if (module == "filesystems") {
        QJsonParseError e;
        const auto doc = QJsonDocument::fromJson(output.toUtf8(), &e);
        if (!doc.isObject() || !doc.object()["blockdevices"].isArray()) {
            *error = t("Invalid block device JSON");
            return {};
        }
        std::function<void(QJsonArray, QString)> visit = [&](QJsonArray devices, const QString& parent) {
            for (const auto& v : devices) {
                const auto o = v.toObject();
                const auto path = o["path"].toString();
                QStringList mounts;
                for (const auto& m : o["mountpoints"].toArray())
                    if (m.isString())
                        mounts << m.toString();
                rows.append(
                    {path,
                     {path, o["type"].toString(), gib(o["size"].toVariant().toULongLong()), o["fstype"].toString(), mounts.join(", "),
                      o["fsavail"].isNull() ? "-" : gib(o["fsavail"].toVariant().toULongLong()), o["fsuse%"].toString()},
                     parent.isEmpty() ? QStringList{} : QStringList{parent},
                     {}});
                visit(o["children"].toArray(), parent.isEmpty() ? path : parent);
            }
        };
        visit(doc.object()["blockdevices"].toArray(), {});
    } else if (module == "audio") {
        QJsonParseError e;
        const auto doc = QJsonDocument::fromJson(output.toUtf8(), &e);
        if (!doc.isArray()) {
            *error = t("Invalid PipeWire JSON");
            return {};
        }
        for (const auto& v : doc.array()) {
            const auto o = v.toObject();
            const auto props = o["info"].toObject()["props"].toObject();
            const auto media = props["media.class"].toString();
            if (media.isEmpty())
                continue;
            const auto id = QString::number(o["id"].toInt());
            rows.append({id, {props["node.description"].toString(props["node.name"].toString()), media, id}, {media}, {}});
        }
    } else if (module == "vms") {
        const QRegularExpression pattern(R"vm(^"(.*)"\s+\{([^}]+)\}$)vm");
        for (const auto& line : output.split('\n', Qt::SkipEmptyParts)) {
            const auto match = pattern.match(line);
            if (!match.hasMatch()) {
                *error = t("Invalid VirtualBox listing");
                continue;
            }
            rows.append({match.captured(2),
                         {match.captured(1), tag == "running" ? t("Running") : t("Registered"), "VirtualBox", match.captured(2)},
                         {"VirtualBox"},
                         {{"running", tag == "running"}}});
        }
    } else if (module == "power") {
        const QStringList allowed{"model",   "state",         "percentage",   "capacity",      "energy", "energy-full", "energy-rate",
                                  "voltage", "time to empty", "time to full", "warning-level", "type",   "on-battery"};
        if (tag == "profile")
            rows.append({"profile", {t("Power profile"), output.trimmed(), "power-profiles-daemon"}, {}, {}});
        else
            for (const auto& line : output.split('\n', Qt::SkipEmptyParts)) {
                const auto key = line.trimmed().section(':', 0, 0);
                if (allowed.contains(key))
                    rows.append({QString::number(rows.size()), {key, line.section(':', 1).trimmed(), "UPower"}, {}, {}});
            }
    } else {
        for (const auto& line : output.split('\n', Qt::SkipEmptyParts))
            rows.append({tag + ":" + QString::number(rows.size()), {line.trimmed(), tag, t("Read-only")}, {tag}, {}});
    }
    return rows;
}
