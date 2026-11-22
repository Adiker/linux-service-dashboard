#include "../src/core/AlertMonitor.h"
#include "../src/core/CommandResult.h"
#include "../src/core/MountProfileStore.h"
#include "../src/core/ServiceGroupSettings.h"
#include "../src/helpers/SmartDeviceRules.h"
#include "../src/parsers/ProviderParsers.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

class ProviderParserTests : public QObject {
    Q_OBJECT

  private slots:
    void systemdListUnits_parsesServicesAndMissingWatched();
    void systemdFailedCount_ignoresHeader();
    void dockerList_parsesJsonLines();
    void nmcliVpn_detectsWireguardConnection();
    void findmntJson_collectsNetworkMountsAndFuseSshfs();
    void findmntSnapshot_ignoresAutofsTargets();
    void sensorsJson_extractsTemperatureInput();
    void sensorsText_parsesPlainFallbackOutput();
    void fstab_skipsCommentsAndBlankLines();
    void fstabNetworkMounts_decodesOctalAndLegacySshfs();
    void providerErrors_emptyDockerOutput();
    void smartDeviceRules_validatesPathsAndArguments();
    void commandResult_classifiesErrors();
    void mountProfileStore_handlesSlashInProfileName();
    void smartctlJson_parsesNvmeAndAta();
    void nmcliVpn_parsesMultipleActiveAndProfiles();
    void serviceGroupSettings_deletesAndGuardsLastGroup();
    void alertMonitor_isEdgeTriggeredWithoutBaselineSpam();
};

void ProviderParserTests::systemdListUnits_parsesServicesAndMissingWatched() {
    const QString output = QStringLiteral("docker.service loaded active running Docker Application Container Engine\n"
                                          "sshd.service loaded active running OpenSSH Daemon\n");
    const QStringList watched{QStringLiteral("docker.service"), QStringLiteral("missing.service")};
    const QVector<ServiceRow> rows = ProviderParsers::parseSystemdListUnits(output, watched, QStringLiteral("now"), nullptr);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).unit, QStringLiteral("docker.service"));
    QCOMPARE(rows.at(1).unit, QStringLiteral("missing.service"));
    QCOMPARE(rows.at(1).loadState, QStringLiteral("unavailable"));
}

void ProviderParserTests::systemdFailedCount_ignoresHeader() {
    const QString output = QStringLiteral("UNIT LOAD ACTIVE SUB DESCRIPTION\nbroken.service loaded failed failed Broken\n");
    QCOMPARE(ProviderParsers::parseSystemdFailedCount(output, nullptr), 1);
}

void ProviderParserTests::dockerList_parsesJsonLines() {
    const QString output =
        QStringLiteral("{\"Names\":\"web\",\"Image\":\"nginx\",\"Status\":\"Up\",\"State\":\"running\",\"Ports\":\"\",\"ID\":\"abc\"}");
    const QVector<DockerContainerRow> rows = ProviderParsers::parseDockerContainerLines(output, nullptr);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().name, QStringLiteral("web"));
    QCOMPARE(rows.first().state, QStringLiteral("running"));
}

void ProviderParserTests::nmcliVpn_detectsWireguardConnection() {
    const QString output = QStringLiteral("home-wifi:802-11-wireless:wlp0s20f3:activated\nvpn-office:wireguard:tun0:activated\n");
    const VpnStatus status = ProviderParsers::parseNmcliActiveVpnConnections(output, QStringLiteral("now"), nullptr);
    QVERIFY(status.connected);
    QCOMPARE(status.connectionName, QStringLiteral("vpn-office"));
    QCOMPARE(status.device, QStringLiteral("tun0"));
}

void ProviderParserTests::findmntJson_collectsNetworkMountsAndFuseSshfs() {
    const QString output = QStringLiteral(
        "{\"filesystems\":[{\"target\":\"/mnt/share\",\"source\":\"//server/share\",\"fstype\":\"cifs\",\"options\":\"rw\","
        "\"children\":[{\"target\":\"/mnt/remote\",\"source\":\"user@host:/data\",\"fstype\":\"fuse.sshfs\",\"options\":\"rw\"}]}]}");
    const QVector<MountRow> rows = ProviderParsers::parseFindmntJson(output, nullptr);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).target, QStringLiteral("/mnt/share"));
    QCOMPARE(rows.at(0).filesystemType, QStringLiteral("cifs"));
    QCOMPARE(rows.at(1).target, QStringLiteral("/mnt/remote"));
    QCOMPARE(rows.at(1).filesystemType, QStringLiteral("fuse.sshfs"));
}

void ProviderParserTests::findmntSnapshot_ignoresAutofsTargets() {
    const QString output =
        QStringLiteral("{\"filesystems\":[{\"target\":\"/mnt/auto\",\"source\":\"systemd-1\",\"fstype\":\"autofs\",\"options\":\"rw\"},"
                       "{\"target\":\"/mnt/real\",\"source\":\"/dev/sda1\",\"fstype\":\"ext4\",\"options\":\"rw\"}]}");
    QVector<MountRow> rows;
    QSet<QString> targets;
    ProviderParsers::parseFindmntSnapshot(output, &rows, &targets, nullptr);
    QVERIFY(rows.isEmpty());
    QVERIFY(!targets.contains(QStringLiteral("/mnt/auto")));
    QVERIFY(targets.contains(QStringLiteral("/mnt/real")));
}

void ProviderParserTests::sensorsJson_extractsTemperatureInput() {
    const QString output =
        QStringLiteral("{\"coretemp-isa-0000\":{\"Package id 0\":{\"temp1_input\":42.0,\"temp1_max\":90.0,\"temp1_crit\":100.0}}}");
    const QVector<SensorRow> rows = ProviderParsers::parseSensorsJson(output, nullptr);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().chip, QStringLiteral("coretemp-isa-0000"));
    QCOMPARE(rows.first().current, 42.0);
}

void ProviderParserTests::sensorsText_parsesPlainFallbackOutput() {
    const QString output = QStringLiteral("coretemp-isa-0000\n"
                                          "Adapter: ISA adapter\n"
                                          "Package id 0:  +85.5°C  (high = +80.0°C, crit = +100.0°C)\n"
                                          "Core 0:        +41.0°C  (high = +80.0°C, crit = +100.0°C)\n");
    const QVector<SensorRow> rows = ProviderParsers::parseSensorsText(output, nullptr);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).chip, QStringLiteral("coretemp-isa-0000"));
    QCOMPARE(rows.at(0).label, QStringLiteral("Package id 0"));
    QCOMPARE(rows.at(0).current, 85.5);
    QCOMPARE(rows.at(0).high, 80.0);
    QCOMPARE(rows.at(0).critical, 100.0);
    QCOMPARE(rows.at(0).status, QStringLiteral("Warning"));
    QCOMPARE(rows.at(1).status, QStringLiteral("OK"));
}

void ProviderParserTests::fstab_skipsCommentsAndBlankLines() {
    const QString content = QStringLiteral("# comment\n"
                                           "\n"
                                           "//server/share /mnt/share cifs credentials=/etc/smb creds,uid=1000 0 0\n");
    const QVector<ProviderParsers::FstabEntry> entries = ProviderParsers::parseFstab(content, nullptr);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().mountPoint, QStringLiteral("/mnt/share"));
    QCOMPARE(entries.first().filesystemType, QStringLiteral("cifs"));
}

void ProviderParserTests::fstabNetworkMounts_decodesOctalAndLegacySshfs() {
    const QString content = QStringLiteral("/dev/sda1 / ext4 defaults 0 1\n"
                                           "sshfs#user@host:/srv /mnt/my\\040share fuse noauto,user 0 0\n");
    const QVector<MountRow> rows = ProviderParsers::parseFstabNetworkMounts(content, nullptr);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().target, QStringLiteral("/mnt/my share"));
    QCOMPARE(rows.first().filesystemType, QStringLiteral("sshfs"));
}

void ProviderParserTests::providerErrors_emptyDockerOutput() {
    const QVector<DockerContainerRow> rows = ProviderParsers::parseDockerContainerLines(QString(), nullptr);
    QVERIFY(rows.isEmpty());
}

void ProviderParserTests::smartDeviceRules_validatesPathsAndArguments() {
    QVERIFY(SmartDeviceRules::isSupportedDevicePath(QStringLiteral("/dev/sda")));
    QVERIFY(SmartDeviceRules::isSupportedDevicePath(QStringLiteral("/dev/nvme0n1")));
    QVERIFY(!SmartDeviceRules::isSupportedDevicePath(QStringLiteral("/dev/sda1")));
    QVERIFY(!SmartDeviceRules::isSupportedDevicePath(QStringLiteral("/tmp/evil")));
    QCOMPARE(SmartDeviceRules::nvmeControllerPath(QStringLiteral("/dev/nvme0n1")), QStringLiteral("/dev/nvme0"));
    const QStringList nvmeArgs = SmartDeviceRules::smartctlArguments(QStringLiteral("/dev/nvme0n1"), QStringLiteral("nvme"));
    QVERIFY(nvmeArgs.contains(QStringLiteral("nvme")));
    QCOMPARE(nvmeArgs.last(), QStringLiteral("/dev/nvme0"));
}

void ProviderParserTests::commandResult_classifiesErrors() {
    CommandResult okRes;
    okRes.exitCode = 0;
    QCOMPARE(okRes.errorKind(), CommandErrorKind::None);

    CommandResult missing;
    missing.startFailed = true;
    QCOMPARE(missing.errorKind(), CommandErrorKind::MissingTool);

    CommandResult timedOut;
    timedOut.timedOut = true;
    QCOMPARE(timedOut.errorKind(), CommandErrorKind::Timeout);

    CommandResult denied;
    denied.exitCode = 1;
    denied.standardError = QStringLiteral("Got permission denied while trying to connect");
    QCOMPARE(denied.errorKind(), CommandErrorKind::PermissionDenied);
}

void ProviderParserTests::mountProfileStore_handlesSlashInProfileName() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("LocalToolsTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MountProfileStoreTest"));

    MountRow row;
    row.source = QStringLiteral("//nas/media");
    row.target = QStringLiteral("/mnt/nas/media");
    row.filesystemType = QStringLiteral("cifs");
    row.options = QStringLiteral("ro");

    const QString profileName = QStringLiteral("/mnt/nas/media");
    MountProfileStore::saveProfile(row, profileName);
    const QVector<MountRow> loaded = MountProfileStore::loadProfiles();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().target, row.target);
    QCOMPARE(loaded.first().source, row.source);

    MountProfileStore::removeProfile(profileName);
    QVERIFY(MountProfileStore::loadProfiles().isEmpty());
}

void ProviderParserTests::smartctlJson_parsesNvmeAndAta() {
    const QByteArray nvmeJson = R"({
        "smart_status": { "passed": true },
        "nvme_smart_health_information_log": {
            "critical_warning": 0,
            "temperature": 38,
            "percentage_used": 4,
            "media_errors": 0,
            "power_on_hours": 1420
        }
    })";
    DiskRow nvmeRow;
    ProviderParsers::parseSmartctlJson(QJsonDocument::fromJson(nvmeJson).object(), &nvmeRow);
    QCOMPARE(nvmeRow.smartHealth, QStringLiteral("PASSED"));
    QCOMPARE(nvmeRow.temperature, QStringLiteral("38 C"));
    QCOMPARE(nvmeRow.reallocated, QStringLiteral("4% wear"));
    QCOMPARE(nvmeRow.pending, QStringLiteral("0 err"));
    QCOMPARE(nvmeRow.powerOnHours, QStringLiteral("1420 h"));

    const QByteArray ataJson = R"({
        "smart_status": { "passed": true },
        "temperature": { "current": 33 },
        "power_on_time": { "hours": 5000 },
        "ata_smart_attributes": {
            "table": [
                { "name": "Reallocated_Sector_Ct", "raw": { "value": 2 } },
                { "name": "Current_Pending_Sector", "raw": { "value": 1 } }
            ]
        }
    })";
    DiskRow ataRow;
    ProviderParsers::parseSmartctlJson(QJsonDocument::fromJson(ataJson).object(), &ataRow);
    QCOMPARE(ataRow.smartHealth, QStringLiteral("PASSED"));
    QCOMPARE(ataRow.temperature, QStringLiteral("33 C"));
    QCOMPARE(ataRow.reallocated, QStringLiteral("2"));
    QCOMPARE(ataRow.pending, QStringLiteral("1"));
    QCOMPARE(ataRow.powerOnHours, QStringLiteral("5000 h"));
}

void ProviderParserTests::nmcliVpn_parsesMultipleActiveAndProfiles() {
    const QString activeOut = QStringLiteral("wifi:802-11-wireless:wlan0:activated\n"
                                             "wg-home:wireguard:wg0:activated\n"
                                             "corp-vpn:vpn:tun0:activated\n");
    const QVector<VpnStatus> active = ProviderParsers::parseNmcliActiveVpnList(activeOut, QStringLiteral("now"), nullptr);
    QCOMPARE(active.size(), 2);
    QCOMPARE(active.at(0).connectionName, QStringLiteral("wg-home"));
    QCOMPARE(active.at(1).connectionName, QStringLiteral("corp-vpn"));

    const QString profilesOut = QStringLiteral("wifi:802-11-wireless:wlan0:activated\n"
                                               "wg-home:wireguard:wg0:activated\n"
                                               "corp-vpn:vpn::\n");
    const QVector<VpnProfile> profiles = ProviderParsers::parseNmcliVpnProfiles(profilesOut, nullptr);
    QCOMPARE(profiles.size(), 2);
    QVERIFY(profiles.at(0).active);
    QVERIFY(!profiles.at(1).active);
    QCOMPARE(profiles.at(1).state, QStringLiteral("disconnected"));
}

void ProviderParserTests::serviceGroupSettings_deletesAndGuardsLastGroup() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("LocalToolsTest"));
    QCoreApplication::setApplicationName(QStringLiteral("ServiceGroupDeleteTest"));

    ServiceGroupSettings::ensureDefaultGroup();
    QVERIFY(!ServiceGroupSettings::deleteGroup(QStringLiteral("Default")));

    ServiceGroupSettings::setServicesForGroup(QStringLiteral("Database"), {QStringLiteral("postgresql.service")});
    ServiceGroupSettings::setActiveGroup(QStringLiteral("Database"));
    QVERIFY(ServiceGroupSettings::deleteGroup(QStringLiteral("Database")));
    QCOMPARE(ServiceGroupSettings::groupNames(), QStringList{QStringLiteral("Default")});
    QCOMPARE(ServiceGroupSettings::activeGroup(), QStringLiteral("Default"));
}

void ProviderParserTests::alertMonitor_isEdgeTriggeredWithoutBaselineSpam() {
    AlertMonitor monitor;
    QStringList raisedTitles;
    QObject::connect(&monitor, &AlertMonitor::alertRaised,
                     [&raisedTitles](const QString& title, const QString&, AlertSeverity) { raisedTitles.append(title); });

    // Baseline observations must not spam alerts
    monitor.processSystemdServices({ServiceRow{QStringLiteral("broken.service"), QStringLiteral("loaded"), QStringLiteral("failed"),
                                               QStringLiteral("failed"), QStringLiteral("desc"), QStringLiteral("t")}});
    monitor.processSensors(
        {SensorRow{QStringLiteral("coretemp"), QStringLiteral("Package"), 85.0, 80.0, 100.0, QStringLiteral("Warning")}});
    VpnStatus connectedVpn;
    connectedVpn.connected = true;
    connectedVpn.connectionName = QStringLiteral("office-vpn");
    monitor.processVpnStatus(connectedVpn);
    QVERIFY(raisedTitles.isEmpty());

    // Transition to worse state raises edge-triggered alerts
    monitor.processSystemdServices({
        ServiceRow{QStringLiteral("broken.service"), QStringLiteral("loaded"), QStringLiteral("failed"), QStringLiteral("failed"),
                   QStringLiteral("desc"), QStringLiteral("t")},
        ServiceRow{QStringLiteral("nginx.service"), QStringLiteral("loaded"), QStringLiteral("failed"), QStringLiteral("failed"),
                   QStringLiteral("desc"), QStringLiteral("t")},
    });
    monitor.processSensors(
        {SensorRow{QStringLiteral("coretemp"), QStringLiteral("Package"), 102.0, 80.0, 100.0, QStringLiteral("Critical")}});
    VpnStatus disconnectedVpn;
    disconnectedVpn.connected = false;
    monitor.processVpnStatus(disconnectedVpn);
    DiskRow failingDisk;
    failingDisk.smartHealth = QStringLiteral("FAILED/Warning");
    monitor.processSmartResult(QStringLiteral("/dev/sda"), failingDisk);
    QCOMPARE(raisedTitles.size(), 4);

    // Repeating identical degraded states must not re-fire alerts
    monitor.processSensors(
        {SensorRow{QStringLiteral("coretemp"), QStringLiteral("Package"), 103.0, 80.0, 100.0, QStringLiteral("Critical")}});
    monitor.processSmartResult(QStringLiteral("/dev/sda"), failingDisk);
    QCOMPARE(raisedTitles.size(), 4);
}

QTEST_MAIN(ProviderParserTests)
#include "test_provider_parsers.moc"
