#include "src/parsers/ProviderParsers.h"
#include "src/services/InventoryProvider.h"
#include "src/services/SystemdServiceProvider.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
class CenterProviderTests : public QObject {
    Q_OBJECT
  private slots:
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
