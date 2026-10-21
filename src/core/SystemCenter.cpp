#include "SystemCenter.h"

#include "../services/ToolLauncher.h"
#include "CommandRunner.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

SystemCenter::SystemCenter(QObject* parent) : QObject(parent) {
    connect(this, &SystemCenter::requestRefresh, &inventory, &InventoryProvider::refresh);
    connect(&inventory, &InventoryProvider::ready, this,
            [this](const QString& id, const QStringList& headers, const auto& rows, const QString& error) {
                if (id == "dockerStats") {
                    if (!error.isEmpty())
                        return;
                    auto s = snapshot("docker");
                    if (!s.error.isEmpty())
                        return;
                    for (auto& r : s.rows) {
                        while (r.cells.size() < 7)
                            r.cells.append("-");
                        for (const auto& stat : rows)
                            if (r.id.startsWith(stat.id) || stat.id.startsWith(r.id)) {
                                r.cells[5] = stat.cells.value(1);
                                r.cells[6] = stat.cells.value(2);
                            }
                    }
                    publish("docker", {tr("Name"), tr("State"), tr("Health"), tr("Image"), tr("Ports"), tr("CPU"), tr("Memory")}, s.rows);
                    return;
                }
                publish(id, headers, rows, error);
            });
    connect(&inventory, &InventoryProvider::launchFailed, this, [this](const QString& error) { emit textReady(tr("Open tool"), error); });
    connect(&inventory, &InventoryProvider::launchCompleted, this, [this](bool ok) {
        m_events.append("apps", ok ? "action-succeeded" : "action-failed");
        updateEvents();
    });
    QSettings settings;
    const auto savedFavorites = settings.value("favorites/ids").toStringList();
    for (const auto& key : savedFavorites)
        m_favorites.insert(key);
    if (settings.value("schema/version", 1).toInt() < 2) {
        if (settings.contains("systemd/activeGroup") && !settings.contains("systemd/showAll"))
            settings.setValue("systemd/showAll", false);
        settings.setValue("schema/version", 2);
    }
    connect(&systemd, &SystemdServiceProvider::actionCompleted, this, [this](bool ok) {
        m_events.append("systemd", ok ? "action-succeeded" : "action-failed");
        updateEvents();
    });
    connect(&docker, &DockerProvider::actionCompleted, this, [this](bool ok) {
        m_events.append("docker", ok ? "action-succeeded" : "action-failed");
        updateEvents();
    });
    connect(&mounts, &MountProvider::actionCompleted, this, [this](bool ok) {
        m_events.append("mounts", ok ? "action-succeeded" : "action-failed");
        updateEvents();
    });
    updateEvents();
    connect(&docker, &DockerProvider::containersReady, this, [this](const auto& rows, const QString& error) {
        QVector<CenterRecord> records;
        for (const auto& r : rows)
            records.append({r.id,
                            {r.name, r.state, r.health, r.image, r.ports},
                            {r.project.isEmpty() ? tr("Standalone containers") : r.project,
                             r.composeService.isEmpty() ? tr("Containers") : r.composeService},
                            {{"container", r.id}},
                            r.state == "running" && r.health == "unhealthy"});
        publish("docker", {tr("Name"), tr("State"), tr("Health"), tr("Image"), tr("Ports")}, records, error);
    });
    connect(&systemd, &SystemdServiceProvider::servicesReady, this, [this](const auto& rows, const QString& error) {
        QVector<CenterRecord> records;
        for (const auto& r : rows) {
            const QString scope = r.scope == ServiceScope::User ? "user" : "system";
            records.append({scope + ":" + r.unit,
                            {r.unit, r.activeState, r.description, scope, r.unitFileState},
                            {r.scope == ServiceScope::User ? tr("User") : tr("System")},
                            {{"unit", r.unit}, {"scope", scope}},
                            r.activeState == "failed"});
        }
        publish("systemd", {tr("Unit"), tr("State"), tr("Description"), tr("Scope"), tr("Unit file state")}, records, error);
    });
    connect(&mounts, &MountProvider::mountsReady, this, [this](const auto& rows, const QString& error) {
        QVector<CenterRecord> records;
        for (const auto& r : rows)
            records.append({r.target, {r.target, r.status, r.filesystemType, r.source}, {}, {{"path", r.target}}});
        publish("mounts", {tr("Path"), tr("State"), tr("Filesystem"), tr("Source")}, records, error);
    });
    connect(&sensors, &SensorProvider::sensorsReady, this, [this](const auto& rows, const QString& error) {
        QVector<CenterRecord> records;
        for (const auto& r : rows)
            records.append({r.chip + "/" + r.label,
                            {r.label, QString::number(r.current) + " " + r.unit, r.chip},
                            {},
                            {},
                            r.status == "Critical" || r.status == "Warning"});
        publish("sensors", {tr("Sensor"), tr("Value"), tr("Device")}, records, error);
    });
    connect(&smart, &SmartProvider::disksReady, this, [this](const auto& rows, const QString& error) {
        QVector<CenterRecord> records;
        QHash<QString, QString> currentIdentities;
        for (const auto& disk : rows) {
            const auto identity = disk.serial + ":" + disk.model + ":" + disk.transport;
            currentIdentities[disk.path] = identity;
            if (m_diskIdentity.value(disk.path) != identity)
                m_smartHealth.remove(disk.path);
        }
        m_diskIdentity = currentIdentities;
        for (const auto& r : rows) {
            const auto checked = m_smartHealth.value(r.path);
            records.append(
                {r.path,
                 {r.path, r.model, r.size, checked.path.isEmpty() ? tr("Not checked") : checked.smartHealth, checked.lastCheck},
                 {},
                 {},
                 checked.smartHealth.startsWith("FAILED") || (!checked.smartDetail.isEmpty() && checked.smartHealth != "PASSED")});
        }
        publish("smart", {tr("Device"), tr("Model"), tr("Size"), tr("Health"), tr("Last check")}, records, error);
    });
    connect(&smart, &SmartProvider::smartReady, this, [this](const QString& path, const DiskRow& row, const QString& error) {
        auto result = row;
        result.path = path;
        if (!error.isEmpty())
            result.smartDetail = error;
        m_smartHealth[path] = result;
        auto s = snapshot("smart");
        for (auto& r : s.rows)
            if (r.id == path) {
                while (r.cells.size() < 5)
                    r.cells.append("-");
                r.cells[3] = result.smartHealth;
                r.cells[4] = result.lastCheck;
                r.meta["smartDetail"] = result.smartDetail;
                r.attention = !error.isEmpty() || result.smartHealth.startsWith("FAILED");
            }
        publish("smart", s.headers, s.rows, s.error);
    });
    connect(&network, &NetworkProvider::vpnConnectionsReady, this, [this](const auto& rows, const QString& error) {
        QVector<CenterRecord> records;
        for (const auto& r : rows)
            records.append({r.id.isEmpty() ? r.device : r.id, {r.name, r.state, r.type, r.device, r.source}, {}, {}});
        publish("vpn", {tr("Name"), tr("State"), tr("Type"), tr("Interface"), tr("Evidence source")}, records, error);
    });
    connect(&systemd, &SystemdServiceProvider::logsReady, this, &SystemCenter::textReady);
    connect(&docker, &DockerProvider::textReady, this, &SystemCenter::textReady);
}
CenterSnapshot SystemCenter::snapshot(const QString& id) const {
    return m_snapshots.value(id);
}
void SystemCenter::publish(const QString& id, const QStringList& headers, const QVector<CenterRecord>& rows, const QString& error) {
    m_refreshing.remove(id);
    auto& s = m_snapshots[id];
    const bool wasError = !s.error.isEmpty();
    int oldAttention = 0;
    for (const auto& row : s.rows)
        if (row.attention)
            ++oldAttention;
    if (error.isEmpty()) {
        s.headers = headers;
        s.rows = rows;
        s.lastSuccess = QDateTime::currentDateTime();
    } else if (!s.lastSuccess.isValid()) {
        s.headers = headers;
        s.rows = rows;
    }
    s.error = error;
    if (id != "events" && centerModuleEnabled(id)) {
        if (wasError != !error.isEmpty()) {
            m_events.append(id, error.isEmpty() ? "recovered" : "refresh-failed");
            updateEvents();
        }
        int attention = 0;
        for (const auto& r : s.rows)
            if (r.attention)
                ++attention;
        if (attention > oldAttention) {
            m_events.append(id, "attention");
            updateEvents();
        }
    }
    emit changed(id);
    if (id == "systemd") {
        inventory.setServices(s.rows, error);
        refresh("backups");
        refresh("sync");
        refresh("servers");
    }
    if (id == "docker") {
        inventory.setContainers(s.rows, error);
        refresh("servers");
    }
    if (id == "timers") {
        inventory.setTimers(error.isEmpty() ? rows : QVector<CenterRecord>{});
        refresh("backups");
    }
    if (id == "mounts") {
        inventory.setMounts(error.isEmpty() ? rows : QVector<CenterRecord>{});
        refresh("sync");
    }
}
void SystemCenter::updateEvents() {
    auto& s = m_snapshots["events"];
    s.headers = {tr("Time"), tr("Module"), tr("Event")};
    s.rows = m_events.rows();
    s.lastSuccess = QDateTime::currentDateTime();
    emit changed("events");
}
void SystemCenter::refresh(const QString& id) {
    if (!centerModuleEnabled(id))
        return;
    if (id == "overview") {
        refreshScheduled();
        return;
    }
    if (id == "events") {
        updateEvents();
        return;
    }
    if (id == "settings")
        return;
    m_refreshing.insert(id);
    emit changed("activity");
    if (id == "systemd")
        systemd.refreshServices({});
    else if (id == "docker")
        docker.refreshContainers();
    else if (id == "vpn")
        network.refreshVpnStatus();
    else if (id == "mounts")
        mounts.refreshMounts(true);
    else if (id == "sensors")
        sensors.refreshSensors();
    else if (id == "smart")
        smart.refreshDisks();
    else
        emit requestRefresh(id);
}
void SystemCenter::refreshScheduled() {
    for (const auto& m : centerModules()) {
        if (!m.onDemand && m.id != "metrics" && m.id != "overview" && m.id != "settings" && m.id != "events")
            refresh(m.id);
    }
}
CenterSnapshot SystemCenter::search(const QString& text) const {
    CenterSnapshot out;
    out.headers = {tr("Name"), tr("Module"), tr("Details")};
    for (const auto& module : centerModules()) {
        if (!centerModuleEnabled(module.id))
            continue;
        if (module.title.contains(text, Qt::CaseInsensitive))
            out.rows.append({module.id, {module.title, tr("Page"), module.group}, {}, {{"module", module.id}}});
        for (const auto& r : m_snapshots.value(module.id).rows) {
            if (!r.cells.join(' ').contains(text, Qt::CaseInsensitive))
                continue;
            auto row = r;
            row.cells = {r.cells.value(0), module.title, r.cells.mid(1).join(" | ")};
            row.meta["module"] = module.id;
            out.rows.append(row);
        }
    }
    out.lastSuccess = QDateTime::currentDateTime();
    return out;
}
void SystemCenter::toggleFavorite(const QString& module, const QString& id) {
    const auto key = module + ":" + id;
    if (m_favorites.contains(key))
        m_favorites.remove(key);
    else
        m_favorites.insert(key);
    QSettings().setValue("favorites/ids", QStringList(m_favorites.values()));
    emit changed("favorites");
}
bool SystemCenter::isFavorite(const QString& module, const QString& id) const {
    return m_favorites.contains(module + ":" + id);
}
void SystemCenter::open(const CenterRecord& r) {
    if (r.meta.contains("desktop")) {
        inventory.launchDesktop(r.meta.value("desktop").toString());
        return;
    }
    if (r.meta.contains("program")) {
        const auto error = ToolLauncher::program(r.meta.value("program").toString());
        if (!error.isEmpty())
            emit textReady(tr("Open tool"), error);
        return;
    }
    const auto key = "panels/" + QString::fromLatin1(QCryptographicHash::hash(r.id.toUtf8(), QCryptographicHash::Sha256).toHex());
    const QUrl panel(QSettings().value(key).toString());
    if (panel.isValid() && !panel.host().isEmpty() && QStringList{"http", "https"}.contains(panel.scheme()) && panel.userInfo().isEmpty() &&
        !panel.hasQuery() && !panel.hasFragment()) {
        QDesktopServices::openUrl(panel);
        return;
    }

    if (r.meta.contains("path"))
        QDesktopServices::openUrl(QUrl::fromLocalFile(r.meta.value("path").toString()));
}
void SystemCenter::logs(const CenterRecord& r) {
    if (r.meta.contains("unit"))
        systemd.serviceLogs(r.meta.value("unit").toString(), r.meta.value("scope") == "user" ? ServiceScope::User : ServiceScope::System);
    if (r.meta.contains("container"))
        docker.containerLogs(r.meta.value("container").toString());
}
int SystemCenter::attentionCount() const {
    QSet<QString> identities;
    for (auto it = m_snapshots.cbegin(); it != m_snapshots.cend(); ++it) {
        if (!centerModuleEnabled(it.key()))
            continue;
        if (!it.value().error.isEmpty())
            identities.insert("error:" + it.key());
        for (const auto& r : it.value().rows)
            if (r.attention)
                identities.insert(r.meta.contains("unit")        ? r.meta.value("scope").toString() + ":" + r.meta.value("unit").toString()
                                  : r.meta.contains("container") ? r.meta.value("container").toString()
                                                                 : it.key() + ":" + r.id);
    }
    return identities.size();
}
CenterSnapshot SystemCenter::overview() const {
    CenterSnapshot out;
    out.headers = {tr("Name"), tr("Module"), tr("Details")};
    QSet<QString> seen;
    for (const auto& module : centerModules()) {
        if (!centerModuleEnabled(module.id) || module.id == "events")
            continue;
        const auto s = snapshot(module.id);
        if (!s.error.isEmpty())
            out.rows.append({module.id,
                             {tr("Data unavailable"), module.title, s.error.left(1000)},
                             {tr("Needs attention")},
                             {{"module", module.id}},
                             true});
        for (const auto& r : s.rows) {
            const bool favorite = isFavorite(module.id, r.id);
            if (!favorite && (!r.attention || seen.contains(r.id)))
                continue;
            auto row = r;
            row.cells = {r.cells.value(0), module.title, r.cells.mid(1).join(" | ")};
            row.meta["module"] = module.id;
            row.groups = {favorite ? tr("Pinned") : tr("Needs attention")};
            out.rows.append(row);
            seen.insert(r.id);
        }
    }
    out.lastSuccess = QDateTime::currentDateTime();
    return out;
}
QJsonDocument SystemCenter::diagnosticReport() const {
    QJsonArray modules;
    for (const auto& m : centerModules()) {
        const auto s = snapshot(m.id);
        const bool tool = centerModuleToolAvailable(m);
        modules.append(QJsonObject{{"id", m.id},
                                   {"enabled", centerModuleEnabled(m.id)},
                                   {"toolAvailable", tool},
                                   {"entries", s.rows.size()},
                                   {"lastSuccess", s.lastSuccess.toString(Qt::ISODate)},
                                   {"hasError", !s.error.isEmpty()}});
    }
    return QJsonDocument(QJsonObject{{"applicationVersion", QCoreApplication::applicationVersion()},
                                     {"qtVersion", qVersion()},
                                     {"kernel", QSysInfo::kernelVersion()},
                                     {"platform", QGuiApplication::platformName()},
                                     {"generatedAt", QDateTime::currentDateTime().toString(Qt::ISODate)},
                                     {"settingsSchema", QSettings().value("schema/version").toInt()},
                                     {"modules", modules}});
}
