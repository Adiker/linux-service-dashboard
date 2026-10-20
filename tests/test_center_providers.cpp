#include "src/parsers/ProviderParsers.h"
#include "src/services/InventoryProvider.h"
#include "src/services/NetworkProvider.h"
#include "src/services/SystemInventory.h"
#include "src/services/SystemdServiceProvider.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
class CenterProviderTests : public QObject {
    Q_OBJECT
  private slots:
    void effectiveDesktopCatalog() {
        QTemporaryDir dir;
        const auto user = dir.filePath("user"), system = dir.filePath("system");
        QVERIFY(QDir().mkpath(user + "/applications/nested"));
        QVERIFY(QDir().mkpath(system + "/applications"));
        auto write = [](const QString& path, const QByteArray& data) {
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly))
                return false;
            return f.write(data) == data.size();
        };
        QVERIFY(write(system + "/applications/same.desktop", "[Desktop Entry]\nType=Application\nName=System\n"));
        QVERIFY(write(user + "/applications/same.desktop", "[Desktop Entry]\nHidden=true\n"));
        QVERIFY(write(user + "/applications/nested/editor.desktop",
                      "[Desktop Entry]\nType=Application\nName=Editor\nName[pl]=Edytor\nCategories=Development;\nOnlyShowIn=KDE;\n"));
        QVERIFY(write(user + "/applications/hidden.desktop", "[Desktop Entry]\nType=Application\nName=Hidden\nNoDisplay=true\n"));
        const auto result = SystemInventory::applications({user, system}, "pl_PL", "KDE");
        QCOMPARE(result.rows.size(), 1);
        QCOMPARE(result.rows[0].id, "nested-editor.desktop");
        QCOMPARE(result.rows[0].cells[0], "Edytor");
        QVERIFY(SystemInventory::applications({user, system}, "en_US", "GNOME").rows.isEmpty());
    }
    void sensorUnitsAndPackageOrigins() {
        QString error;
        auto sensors = ProviderParsers::parseSensorsJson(R"({"chip":{"Fan":{"fan1_input":1234},"CPU":{"temp1_input":42}}})", &error);
        QCOMPARE(sensors.size(), 2);
        for (const auto& row : sensors) {
            if (row.label == "Fan")
                QCOMPARE(row.unit, "RPM");
            else
                QCOMPARE(row.unit, "°C");
        }
        auto packages = SystemInventory::parse("packages", "foreign", "custom-package 1.2.3\n", &error);
        QCOMPARE(packages.size(), 1);
        QVERIFY(packages[0].meta.value("foreign").toBool());
        const auto power = SystemInventory::parse("power", "UPower", "  serial: do-not-retain\n  percentage: 90%\n", &error);
        QCOMPARE(power.size(), 1);
        QVERIFY(!power[0].cells.join(' ').contains("do-not-retain"));
    }
    void composeAndInvalidJson() {
        QString error;
        auto rows = ProviderParsers::parseDockerContainerLines(
            R"json({"ID":"abc","Names":"example","Labels":"com.docker.compose.project=project,com.docker.compose.service=web","Status":"Up (unhealthy)"})json",
            &error);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].project, "project");
        QCOMPARE(rows[0].composeService, "web");
        QCOMPARE(rows[0].health, "unhealthy");
        ProviderParsers::parseDockerContainerLines("broken", &error);
        QVERIFY(!error.isEmpty());
    }
    void timersAndInvalidJson() {
        QString error;
        auto rows = InventoryProvider::parse(
            "timers", "user", R"([{"unit":"backup.timer","next":1792533600000000,"last":0,"activates":"backup.service"}])", &error);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].id, "user:backup.timer");
        QCOMPARE(rows[0].cells[3], "-");
        InventoryProvider::parse("timers", "user", "bad", &error);
        QVERIFY(!error.isEmpty());
    }
    void escapedNamesAndNetworkData() {
        // UUID/type/device/state remain separate when a display name contains colons.
        const auto fields = NetworkProvider::splitNmcli("Office\\: VPN:uuid:wireguard:wg0:activated");
        QCOMPARE(fields.size(), 5);
        QCOMPARE(fields[0], "Office: VPN");
        QString error;
        const auto rows = InventoryProvider::parse(
            "network", "addresses",
            R"([{"ifname":"eth0","operstate":"UP","addr_info":[{"local":"192.0.2.5","prefixlen":24}],"stats64":{"rx":{"bytes":25},"tx":{"bytes":50}}}])",
            &error);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].cells[2], "192.0.2.5/24");
        QCOMPARE(rows[0].cells[4], "25");
    }
    void backupOutcomeIsNotIntegrity() {
        QString error;
        auto rows = InventoryProvider::parse("backups", "system",
                                             "Id=backup.service\nActiveState=inactive\nSubState=dead\nResult=success\nExecMainStatus="
                                             "0\nExecMainExitTimestamp=Tue 2026-10-20 12:00:00 CEST\n",
                                             &error);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].cells[3], "success");
        QCOMPARE(rows[0].cells[7], "Not verified");
        QVERIFY(!rows[0].attention);
        rows = InventoryProvider::parse("backups", "system",
                                        "Id=backup.service\nResult=success\nExecMainStatus=0\nExecMainExitTimestamp=\n", &error);
        QCOMPARE(rows[0].cells[3], "Unknown");
        rows =
            ProviderParsers::parseFindmntJson(R"({"filesystems":[{"target":"/cloud","source":"remote:","fstype":"fuse.rclone"}]})", &error)
                    .isEmpty()
                ? QVector<CenterRecord>{}
                : QVector<CenterRecord>{{}};
        QCOMPARE(rows.size(), 1);
    }
    void scopedFallbackAndUnloadedUnits() {
        QTemporaryDir dir;
        QFile file(dir.filePath("systemctl"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(
            "#!/bin/sh\ncase \"$*\" in\n*list-unit-files*) echo 'idle.service disabled';;\n*list-units*) echo 'example.service loaded "
            "active running Example';;\nesac\n");
        file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const auto path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8());
        qputenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent-lsd-test-bus");
        qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent-lsd-test-bus");
        SystemdServiceProvider provider;
        QSignalSpy spy(&provider, &SystemdServiceProvider::servicesReady);
        provider.refreshServices({});
        provider.refreshServices({});
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 3000);
        const auto result = spy.takeFirst();
        const auto rows = qvariant_cast<QVector<ServiceRow>>(result[0]);
        QVERIFY2(result[1].toString().isEmpty(), qPrintable(result[1].toString()));
        QCOMPARE(rows.size(), 4);
        int user = 0, idle = 0;
        for (const auto& r : rows) {
            if (r.scope == ServiceScope::User)
                ++user;
            if (r.unit == "idle.service")
                ++idle;
        }
        QCOMPARE(user, 2);
        QCOMPARE(idle, 2);
        qputenv("PATH", path);
        QVERIFY(SystemdServiceProvider::isProtectedUnit("plasma-kwin_wayland.service"));
        QVERIFY(!SystemdServiceProvider::isProtectedUnit("example.service"));
    }
};
QTEST_GUILESS_MAIN(CenterProviderTests)
#include "test_center_providers.moc"
