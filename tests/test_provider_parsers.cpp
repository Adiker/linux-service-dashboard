#include "../src/core/AlertMonitor.h"
#include "../src/core/MountProfileStore.h"
#include "../src/core/ServiceGroupSettings.h"
#include "../src/helpers/SmartDeviceRules.h"
#include "../src/parsers/ProviderParsers.h"

#include <QCoreApplication>
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
    void findmntJson_collectsNetworkMounts();
    void sensorsJson_extractsTemperatureInput();
    void sensorsText_parsesPlainFallbackOutput();
    void fstab_skipsCommentsAndBlankLines();
    void providerErrors_emptyDockerOutput();
    void smartDeviceRules_validatesPathsAndArguments();
    void mountProfileStore_handlesSlashAndStripsPasswords();
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

void ProviderParserTests::findmntJson_collectsNetworkMounts() {
    const QString output = QStringLiteral(
        "{\"filesystems\":[{\"target\":\"/mnt/share\",\"source\":\"//server/share\",\"fstype\":\"cifs\",\"options\":\"rw\"}]}");
    const QVector<MountRow> rows = ProviderParsers::parseFindmntJson(output, nullptr);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().target, QStringLiteral("/mnt/share"));
    QCOMPARE(rows.first().filesystemType, QStringLiteral("cifs"));
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
    QCOMPARE(rows.at(0).status, QStringLiteral("Warning"));
    QCOMPARE(rows.at(0).unit, QStringLiteral("°C"));
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

void ProviderParserTests::mountProfileStore_handlesSlashAndStripsPasswords() {
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
    row.options = QStringLiteral("ro,password=secret,uid=1000");

    const QString profileName = QStringLiteral("/mnt/nas/media");
    MountProfileStore::saveProfile(row, profileName);
    const QVector<MountRow> loaded = MountProfileStore::loadProfiles();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().target, row.target);
    QVERIFY(!loaded.first().options.contains(QStringLiteral("secret")));
    QVERIFY(loaded.first().options.contains(QStringLiteral("uid=1000")));

    MountProfileStore::removeProfile(profileName);
    QVERIFY(MountProfileStore::loadProfiles().isEmpty());
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

    ServiceRow broken;
    broken.unit = QStringLiteral("broken.service");
    broken.activeState = QStringLiteral("failed");
    SensorRow warnSensor{QStringLiteral("coretemp"), QStringLiteral("Package"), 85.0, 80.0, 100.0,
                         QStringLiteral("Warning"),  QStringLiteral("°C")};
    VpnStatus connectedVpn;
    connectedVpn.connected = true;
    connectedVpn.connectionName = QStringLiteral("office-vpn");

    monitor.processSystemdServices({broken});
    monitor.processSensors({warnSensor});
    monitor.processVpnStatus(connectedVpn);
    QVERIFY(raisedTitles.isEmpty());

    ServiceRow nginx;
    nginx.unit = QStringLiteral("nginx.service");
    nginx.activeState = QStringLiteral("failed");
    SensorRow critSensor{QStringLiteral("coretemp"), QStringLiteral("Package"), 102.0, 80.0, 100.0,
                         QStringLiteral("Critical"), QStringLiteral("°C")};
    VpnStatus disconnectedVpn;
    disconnectedVpn.connected = false;
    DiskRow failingDisk;
    failingDisk.smartHealth = QStringLiteral("FAILED/Warning");

    monitor.processSystemdServices({broken, nginx});
    monitor.processSensors({critSensor});
    monitor.processVpnStatus(disconnectedVpn);
    monitor.processSmartResult(QStringLiteral("/dev/sda"), failingDisk);
    QCOMPARE(raisedTitles.size(), 4);

    monitor.processSensors({critSensor});
    monitor.processSmartResult(QStringLiteral("/dev/sda"), failingDisk);
    QCOMPARE(raisedTitles.size(), 4);
}

QTEST_MAIN(ProviderParserTests)
#include "test_provider_parsers.moc"
