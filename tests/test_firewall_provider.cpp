#include "src/services/FirewallProvider.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusVirtualObject>
#include <QProcess>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

struct AuthorizationResult {
    bool authorized;
    bool challenge;
    QMap<QString, QString> details;
};
Q_DECLARE_METATYPE(AuthorizationResult)
QDBusArgument& operator<<(QDBusArgument& a, const AuthorizationResult& result) {
    a.beginStructure();
    a << result.authorized << result.challenge << result.details;
    a.endStructure();
    return a;
}
const QDBusArgument& operator>>(const QDBusArgument& a, AuthorizationResult& result) {
    a.beginStructure();
    a >> result.authorized >> result.challenge >> result.details;
    a.endStructure();
    return a;
}
class FakeFirewall : public QDBusVirtualObject {
  public:
    bool allowInfo = false, allowConfig = false;
    int reads = 0;
    QList<uint> flags;
    QStringList subjects;
    QString introspect(const QString&) const override { return {}; }
    bool handleMessage(const QDBusMessage& message, const QDBusConnection& bus) override {
        QVariantList result;
        if (message.member() == "CheckAuthorization") {
            flags.append(message.arguments().value(3).toUInt());
            const auto subject = qvariant_cast<QDBusArgument>(message.arguments().value(0));
            QString kind;
            QVariantMap attributes;
            subject.beginStructure();
            subject >> kind >> attributes;
            subject.endStructure();
            subjects.append(kind + ":" + attributes.value("name").toString());
            const bool allowed = message.arguments().value(1).toString().endsWith("config.info") ? allowConfig : allowInfo;
            result << QVariant::fromValue(AuthorizationResult{allowed, !allowed, {}});
        } else if (message.member() == "getZones") {
            ++reads;
            result << QStringList{"public"};
        } else if (message.member() == "getZoneSettings2") {
            ++reads;
            result << QVariantMap{{"services", QStringList{"ssh"}}, {"ports", QVariant::fromValue(QList<QStringList>{{"1234", "tcp"}})}};
        } else
            return false;
        return bus.send(message.createReply(result));
    }
};
class FirewallTests : public QObject {
    Q_OBJECT
  private slots:
    void noninteractiveReads_data() {
        QTest::addColumn<bool>("info");
        QTest::addColumn<bool>("config");
        QTest::addColumn<int>("reads");
        QTest::newRow("denied") << false << false << 0;
        QTest::newRow("configuration-denied") << true << false << 1;
        QTest::newRow("authorized") << true << true << 2;
    }
    void noninteractiveReads() {
        QFETCH(bool, info);
        QFETCH(bool, config);
        QFETCH(int, reads);
        if (QStandardPaths::findExecutable("dbus-daemon").isEmpty())
            QSKIP("dbus-daemon not installed");
        qDBusRegisterMetaType<QMap<QString, QString>>();
        qDBusRegisterMetaType<AuthorizationResult>();
        qDBusRegisterMetaType<QList<QStringList>>();
        QProcess daemon;
        daemon.start("dbus-daemon", {"--session", "--nofork", "--print-address=1"});
        QVERIFY(daemon.waitForStarted());
        QVERIFY(daemon.waitForReadyRead());
        const auto address = QString::fromUtf8(daemon.readLine()).trimmed();
        const auto cleanup = qScopeGuard([&]() {
            QDBusConnection::disconnectFromBus("firewall-test-service");
            QDBusConnection::disconnectFromBus("firewall-test-client");
            daemon.terminate();
            daemon.waitForFinished();
        });
        {
            auto service = QDBusConnection::connectToBus(address, "firewall-test-service");
            auto client = QDBusConnection::connectToBus(address, "firewall-test-client");
            QVERIFY(service.isConnected());
            QVERIFY(client.isConnected());
            FakeFirewall fake;
            fake.allowInfo = info;
            fake.allowConfig = config;
            QVERIFY(service.registerService("org.freedesktop.PolicyKit1"));
            QVERIFY(service.registerService("org.fedoraproject.FirewallD1"));
            QVERIFY(service.registerVirtualObject("/org/freedesktop/PolicyKit1/Authority", &fake));
            QVERIFY(service.registerVirtualObject("/org/fedoraproject/FirewallD1", &fake));
            FirewallProvider provider(nullptr, client);
            QSignalSpy spy(&provider, &FirewallProvider::ready);
            provider.refresh();
            provider.refresh();
            QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 3000);
            QCOMPARE(fake.reads, reads);
            QVERIFY(!fake.flags.isEmpty());
            for (const auto flag : fake.flags)
                QCOMPARE(flag, uint(0));
            for (const auto& subject : fake.subjects)
                QCOMPARE(subject, "system-bus-name:" + client.baseService());
            const auto reply = spy.takeFirst();
            if (config) {
                QVERIFY(reply[1].toString().isEmpty());
                const auto rows = qvariant_cast<QVector<CenterRecord>>(reply[0]);
                QCOMPARE(rows.size(), 2);
                QVERIFY(rows[0].cells.join(' ').contains("1234/tcp"));
            } else
                QVERIFY(reply[1].toString().contains("requires authorization"));
        }
    }
};
QTEST_GUILESS_MAIN(FirewallTests)
#include "test_firewall_provider.moc"
