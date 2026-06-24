#include "../src/parsers/ProviderParsers.h"

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
    void fstab_skipsCommentsAndBlankLines();
    void providerErrors_emptyDockerOutput();
};

void ProviderParserTests::systemdListUnits_parsesServicesAndMissingWatched()
{
    const QString output = QStringLiteral(
        "docker.service loaded active running Docker Application Container Engine\n"
        "sshd.service loaded active running OpenSSH Daemon\n");
    const QStringList watched{QStringLiteral("docker.service"), QStringLiteral("missing.service")};
    const QVector<ServiceRow> rows = ProviderParsers::parseSystemdListUnits(output, watched, QStringLiteral("now"), nullptr);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).unit, QStringLiteral("docker.service"));
    QCOMPARE(rows.at(1).unit, QStringLiteral("missing.service"));
    QCOMPARE(rows.at(1).loadState, QStringLiteral("unavailable"));
}

void ProviderParserTests::systemdFailedCount_ignoresHeader()
{
    const QString output = QStringLiteral("UNIT LOAD ACTIVE SUB DESCRIPTION\nbroken.service loaded failed failed Broken\n");
    QCOMPARE(ProviderParsers::parseSystemdFailedCount(output, nullptr), 1);
}

void ProviderParserTests::dockerList_parsesJsonLines()
{
    const QString output = QStringLiteral(
        "{\"Names\":\"web\",\"Image\":\"nginx\",\"Status\":\"Up\",\"State\":\"running\",\"Ports\":\"\",\"ID\":\"abc\"}");
    const QVector<DockerContainerRow> rows = ProviderParsers::parseDockerContainerLines(output, nullptr);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().name, QStringLiteral("web"));
    QCOMPARE(rows.first().state, QStringLiteral("running"));
}

void ProviderParserTests::nmcliVpn_detectsWireguardConnection()
{
    const QString output = QStringLiteral("home-wifi:802-11-wireless:wlp0s20f3:activated\nvpn-office:wireguard:tun0:activated\n");
    const VpnStatus status = ProviderParsers::parseNmcliActiveVpnConnections(output, QStringLiteral("now"), nullptr);
    QVERIFY(status.connected);
    QCOMPARE(status.connectionName, QStringLiteral("vpn-office"));
    QCOMPARE(status.device, QStringLiteral("tun0"));
}

void ProviderParserTests::findmntJson_collectsNetworkMounts()
{
    const QString output = QStringLiteral(
        "{\"filesystems\":[{\"target\":\"/mnt/share\",\"source\":\"//server/share\",\"fstype\":\"cifs\",\"options\":\"rw\"}]}");
    const QVector<MountRow> rows = ProviderParsers::parseFindmntJson(output, nullptr);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().target, QStringLiteral("/mnt/share"));
    QCOMPARE(rows.first().filesystemType, QStringLiteral("cifs"));
}

void ProviderParserTests::sensorsJson_extractsTemperatureInput()
{
    const QString output = QStringLiteral(
        "{\"coretemp-isa-0000\":{\"Package id 0\":{\"temp1_input\":42.0,\"temp1_max\":90.0,\"temp1_crit\":100.0}}}");
    const QVector<SensorRow> rows = ProviderParsers::parseSensorsJson(output, nullptr);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().chip, QStringLiteral("coretemp-isa-0000"));
    QCOMPARE(rows.first().current, 42.0);
}

void ProviderParserTests::fstab_skipsCommentsAndBlankLines()
{
    const QString content = QStringLiteral(
        "# comment\n"
        "\n"
        "//server/share /mnt/share cifs credentials=/etc/smb creds,uid=1000 0 0\n");
    const QVector<ProviderParsers::FstabEntry> entries = ProviderParsers::parseFstab(content, nullptr);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().mountPoint, QStringLiteral("/mnt/share"));
    QCOMPARE(entries.first().filesystemType, QStringLiteral("cifs"));
}

void ProviderParserTests::providerErrors_emptyDockerOutput()
{
    const QVector<DockerContainerRow> rows = ProviderParsers::parseDockerContainerLines(QString(), nullptr);
    QVERIFY(rows.isEmpty());
}

QTEST_MAIN(ProviderParserTests)
#include "test_provider_parsers.moc"
