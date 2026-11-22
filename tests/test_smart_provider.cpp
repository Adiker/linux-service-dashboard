#include "src/services/SmartProvider.h"

#include <QFile>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class SmartProviderTests : public QObject {
    Q_OBJECT
  private slots:
    void healthEvidence_data() {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<QString>("health");
        QTest::addColumn<bool>("error");
        QTest::newRow("passed") << QByteArray(R"({"smart_status":{"passed":true},"temperature":{"current":40}})") << 0 << QString("PASSED")
                                << false;
        QTest::newRow("failed") << QByteArray(R"({"smart_status":{"passed":false}})") << 8 << QString("FAILED/Warning") << false;
        QTest::newRow("usb-attribute-check")
            << QByteArray(
                   R"({"smart_status":{"passed":true},"smartctl":{"messages":[{"string":"Warning: This result is based on an Attribute check."}]}})")
            << 4 << QString("PASSED") << false;
        QTest::newRow("missing-health") << QByteArray(R"({"temperature":{"current":40}})") << 0 << QString("Health unavailable") << true;
        QTest::newRow("null-health") << QByteArray(R"({"smart_status":{"passed":null}})") << 0 << QString("Health unavailable") << true;
        QTest::newRow("string-health") << QByteArray(R"({"smart_status":{"passed":"false"}})") << 0 << QString("Health unavailable")
                                       << true;
        QTest::newRow("unsupported-bridge") << QByteArray(R"({"smartctl":{"messages":[{"string":"Unknown USB bridge"}]}})") << 1
                                            << QString("Check failed") << true;
        QTest::newRow("invalid-json") << QByteArray("broken") << 0 << QString("Check failed") << true;
    }
    void healthEvidence() {
        QFETCH(QByteArray, json);
        QFETCH(int, exitCode);
        QFETCH(QString, health);
        QFETCH(bool, error);
        QTemporaryDir dir;
        QFile script(dir.filePath("smartctl"));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nprintf '%s\\n' '" + json.replace("'", "'\\''") + "'\nexit " + QByteArray::number(exitCode) + "\n");
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const auto originalPath = qgetenv("PATH");
        const auto restore = qScopeGuard([originalPath]() { qputenv("PATH", originalPath); });
        qputenv("PATH", dir.path().toUtf8());
        SmartProvider provider;
        QSignalSpy spy(&provider, &SmartProvider::smartReady);
        DiskRow disk;
        disk.path = "/dev/test-smart-fixture";
        provider.checkSmart(disk);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 2000);
        const auto result = spy.takeFirst();
        const auto row = qvariant_cast<DiskRow>(result[1]);
        QCOMPARE(row.smartHealth, health);
        QCOMPARE(!result[2].toString().isEmpty(), error);
        if (error)
            QCOMPARE(row.smartDetail, result[2].toString());
        else if (exitCode == 4)
            QVERIFY(row.smartDetail.contains("Attribute check"));
        QVERIFY(row.lastCheck != "Never");
        QVERIFY(!provider.isSmartCheckBusy());
    }
};
QTEST_GUILESS_MAIN(SmartProviderTests)
#include "test_smart_provider.moc"
