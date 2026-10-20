#include "SystemCenter.h"

#include "CommandRunner.h"

#include <QDesktopServices>
#include <QSettings>
#include <QStandardPaths>
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
    QSettings settings;
    if (settings.value("schema/version", 1).toInt() < 2)
        settings.setValue("schema/version", 2);
    connect(&docker, &DockerProvider::containersReady, this, [this](const auto& rows, const QString& error) {
        QVector<CenterRecord> records;
        for (const auto& r : rows)
            records.append({r.id,
                            {r.name, r.state, r.health, r.image, r.ports},
                            {r.project.isEmpty() ? tr("Standalone containers") : r.project,
                             r.composeService.isEmpty() ? tr("Containers") : r.composeService},
                            {{"container", r.id}},
                            r.health == "unhealthy"});
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
            records.append({r.chip + "/" + r.label, {r.label, QString::number(r.current), r.chip}, {}, {}});
        publish("sensors", {tr("Sensor"), tr("Value"), tr("Device")}, records, error);
    });
    connect(&smart, &SmartProvider::disksReady, this, [this](const auto& rows, const QString& error) {
        QVector<CenterRecord> records;
        for (const auto& r : rows)
            records.append({r.path, {r.path, r.model, r.size}, {}, {}});
        publish("smart", {tr("Device"), tr("Model"), tr("Size")}, records, error);
    });
    connect(&network, &NetworkProvider::vpnStatusReady, this, [this](const VpnStatus& r, const QString& error) {
        publish("vpn", {tr("Name"), tr("State"), tr("Device")}, {{r.device, {r.connectionName, r.state, r.device}, {}, {}}}, error);
    });
    connect(&systemd, &SystemdServiceProvider::logsReady, this, &SystemCenter::textReady);
    connect(&docker, &DockerProvider::textReady, this, &SystemCenter::textReady);
}
CenterSnapshot SystemCenter::snapshot(const QString& id) const {
    return m_snapshots.value(id);
}
void SystemCenter::publish(const QString& id, const QStringList& headers, const QVector<CenterRecord>& rows, const QString& error) {
    auto& s = m_snapshots[id];
    if (error.isEmpty()) {
        s.headers = headers;
        s.rows = rows;
        s.lastSuccess = QDateTime::currentDateTime();
    } else if (s.headers.isEmpty())
        s.headers = headers;
    s.error = error;
    emit changed(id);
}
void SystemCenter::refresh(const QString& id) {
    if (!centerModuleEnabled(id))
        return;
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
    QSettings s;
    auto ids = s.value("favorites/ids").toStringList();
    const auto key = module + ":" + id;
    if (ids.contains(key))
        ids.removeAll(key);
    else
        ids.append(key);
    s.setValue("favorites/ids", ids);
    emit changed("favorites");
}
bool SystemCenter::isFavorite(const QString& module, const QString& id) const {
    return QSettings().value("favorites/ids").toStringList().contains(module + ":" + id);
}
void SystemCenter::open(const CenterRecord& r) {
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
    int count = 0;
    for (auto it = m_snapshots.cbegin(); it != m_snapshots.cend(); ++it) {
        if (!centerModuleEnabled(it.key()))
            continue;
        if (!it.value().error.isEmpty())
            ++count;
        for (const auto& r : it.value().rows)
            if (r.attention)
                ++count;
    }
    return count;
}
