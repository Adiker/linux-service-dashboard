#include "BuildInfo.h"
#include "src/MainWindow.h"
#include "src/models/MountModel.h"
#include "src/models/ServiceModel.h"
#include "src/utils/TableTools.h"
#include "src/widgets/SettingsPage.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QtTest>

class CenterUiTests : public QObject {
    Q_OBJECT
  private slots:
    void snapshotsMigrationAndSafeReport() {
        QSettings s;
        s.clear();
        s.setValue("theme/preference", "OLED");
        s.setValue("systemd/activeGroup", "Personal");
        s.setValue("mounts/profileNames", QStringList{"Saved"});
        s.setValue("smart/history/path/example", QStringList{"sentinel"});
        for (const auto& m : centerModules())
            if (m.id != "systemd")
                s.setValue("modules/" + m.id, false);
        SystemCenter center;
        QCOMPARE(s.value("theme/preference").toString(), "OLED");
        QCOMPARE(s.value("systemd/activeGroup").toString(), "Personal");
        QCOMPARE(s.value("mounts/profileNames").toStringList(), QStringList{"Saved"});
        QCOMPARE(s.value("smart/history/path/example").toStringList(), QStringList{"sentinel"});
        center.publish("systemd", {"Unit", "State"},
                       {{"system:example.service",
                         {"example.service", "active", "/home/example secret=do-not-export 198.51.100.9"},
                         {},
                         {{"unit", "example.service"}, {"scope", "system"}}}});
        const auto successful = center.snapshot("systemd");
        center.publish("systemd", {"Unit"}, {}, "token=do-not-export");
        QCOMPARE(center.snapshot("systemd").rows.size(), 1);
        QCOMPARE(center.snapshot("systemd").lastSuccess, successful.lastSuccess);
        center.toggleFavorite("systemd", "system:example.service");
        QVERIFY(center.isFavorite("systemd", "system:example.service"));
        QVERIFY(!center.overview().rows.isEmpty());
        const auto report = center.diagnosticReport().toJson();
        QVERIFY(!report.contains("do-not-export"));
        QVERIFY(!report.contains("/home/example"));
        QVERIFY(!report.contains("198.51.100.9"));
        QSignalSpy spy(&center, &SystemCenter::requestRefresh);
        center.refresh("packages");
        QCOMPARE(spy.size(), 0);
        ServiceModel model;
        ServiceRow system;
        system.unit = "same.service";
        ServiceRow user = system;
        user.scope = ServiceScope::User;
        model.setRows({system, user});
        QVERIFY(model.index(0, 0).data(Qt::UserRole) != model.index(1, 0).data(Qt::UserRole));
    }
    void translatedSettingsKeepStableValues() {
        QSettings s;
        s.clear();
        s.setValue("theme/preference", "OLED");
        s.setValue("ui/language", "pl");
        SettingsPage page;
        QComboBox* theme = nullptr;
        QComboBox* language = nullptr;
        for (auto* combo : page.findChildren<QComboBox*>()) {
            if (combo->findData("Dark") >= 0)
                theme = combo;
            if (combo->findData("en") >= 0)
                language = combo;
        }
        QVERIFY(theme);
        QVERIFY(language);
        QCOMPARE(theme->currentData().toString(), "OLED");
        theme->setCurrentIndex(theme->findData("Dark"));
        language->setCurrentIndex(language->findData("en"));
        QPushButton* save = nullptr;
        for (auto* button : page.findChildren<QPushButton*>())
            if (button->text() == SettingsPage::tr("Save"))
                save = button;
        QVERIFY(save);
        save->click();
        QCOMPARE(s.value("theme/preference").toString(), "Dark");
        QCOMPARE(s.value("ui/language").toString(), "en");
    }
    void evidenceAndLauncherErrors() {
        QSettings s;
        s.clear();
        for (const auto& m : centerModules())
            s.setValue("modules/" + m.id, false);
        s.setValue("modules/docker", true);
        s.setValue("modules/smart", true);
        s.setValue("modules/systemd", true);
        SystemCenter center;
        DockerContainerRow container;
        container.id = "example";
        container.name = "Example";
        container.state = "exited";
        container.health = "unhealthy";
        center.docker.containersReady({container}, {});
        QVERIFY(!center.snapshot("docker").rows[0].attention);
        container.state = "running";
        center.docker.containersReady({container}, {});
        QVERIFY(center.snapshot("docker").rows[0].attention);
        DiskRow old;
        old.path = "/dev/example";
        old.serial = "A";
        old.model = "Disk A";
        center.smart.disksReady({old}, {});
        auto checked = old;
        checked.smartHealth = "FAILED/Warning";
        center.smart.smartReady(old.path, checked, {});
        QVERIFY(center.snapshot("smart").rows[0].attention);
        auto replacement = old;
        replacement.serial = "B";
        replacement.model = "Disk B";
        center.smart.disksReady({replacement}, {});
        QVERIFY(!center.snapshot("smart").rows[0].attention);
        InventoryProvider inventory;
        inventory.setServices({}, "Source unavailable");
        QSignalSpy source(&inventory, &InventoryProvider::ready);
        inventory.refresh("sync");
        QCOMPARE(source.size(), 1);
        QVERIFY(!source.takeFirst()[3].toString().isEmpty());
        QSignalSpy launch(&inventory, &InventoryProvider::launchFailed);
        inventory.launchDesktop("/outside-catalog.desktop");
        QCOMPARE(launch.size(), 1);
    }
    void unreadAndFailedSmartStates() {
        DiskRow disk;
        disk.path = "/dev/example";
        disk.serial = "A";
        disk.model = "Disk A";
        disk.transport = "sata";
        DiskModel model;
        model.setRows({disk});
        QCOMPARE(model.data(model.index(0, 5), Qt::DisplayRole).toString(), "Not checked");
        QVERIFY(model.data(model.index(0, 5), Qt::ToolTipRole).toString().contains("Check SMART"));
        auto failed = disk;
        failed.smartHealth = "Permission denied";
        failed.smartDetail = "Cannot read this device without authorization";
        failed.lastCheck = "2026-10-21T10:00:00";
        model.updateSmart(disk.path, failed);
        model.setRows({disk});
        QCOMPARE(model.data(model.index(0, 5), Qt::DisplayRole).toString(), "Permission denied");
        QCOMPARE(model.data(model.index(0, 5), Qt::ToolTipRole).toString(), failed.smartDetail);
        auto replacement = disk;
        replacement.serial = "B";
        model.setRows({replacement});
        QCOMPARE(model.data(model.index(0, 5), Qt::DisplayRole).toString(), "Not checked");
        QCOMPARE(model.rowAt(0).lastCheck, "Never");
        QSettings settings;
        settings.clear();
        for (const auto& module : centerModules())
            settings.setValue("modules/" + module.id, false);
        SystemCenter center;
        center.smart.disksReady({disk}, {});
        center.smart.smartReady(disk.path, failed, failed.smartDetail);
        const auto snapshot = center.snapshot("smart");
        QCOMPARE(snapshot.rows[0].cells[3], "Permission denied");
        QVERIFY(snapshot.rows[0].attention);
        QCOMPARE(snapshot.rows[0].meta.value("smartDetail").toString(), failed.smartDetail);
        center.smart.disksReady({disk}, {});
        QVERIFY(center.snapshot("smart").rows[0].attention);
    }
    void sortedSelectionAndEventProjection() {
        QWidget owner;
        auto* layout = new QVBoxLayout(&owner);
        auto* table = new QTableView;
        MountModel model;
        model.setRows({{"/alpha", "source", "ext4", "", "Mounted"}, {"/zulu", "source", "ext4", "", "Mounted"}});
        table->setModel(&model);
        layout->addWidget(table);
        TableTools::install(table, layout);
        table->setSortingEnabled(true);
        table->sortByColumn(0, Qt::DescendingOrder);
        table->setCurrentIndex(table->model()->index(0, 0));
        QCOMPARE(model.rowAt(TableTools::sourceRow(table)).target, "/zulu");
        auto* filter = owner.findChild<QLineEdit*>();
        QVERIFY(filter);
        filter->setText("alpha");
        table->setCurrentIndex(table->model()->index(0, 0));
        QCOMPARE(model.rowAt(TableTools::sourceRow(table)).target, "/alpha");
        EventStore events;
        const auto before = events.rows().size();
        events.append("systemd", "unknown-code");
        QCOMPARE(events.rows().size(), before);
        events.append("systemd", "action-succeeded");
        QVERIFY(!events.rows().isEmpty());
        QFile file(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/events.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = file.readAll();
        QVERIFY(!data.contains("standardOutput"));
        QVERIFY(!data.contains("arguments"));
    }
    void desktopSmoke_data() {
        QTest::addColumn<QString>("language");
        QTest::newRow("English") << QString("en");
        QTest::newRow("Polish") << QString("pl");
    }
    void desktopSmoke() {
        QFETCH(QString, language);
        QSettings s;
        s.clear();
        s.setValue("ui/language", language);
        s.setValue("smart/scheduledRefreshEnabled", false);
        QTranslator translator;
        if (language == "pl") {
            QVERIFY(translator.load(QStringLiteral(LSD_TEST_QM_PATH)));
            qApp->installTranslator(&translator);
            QLocale::setDefault(QLocale::Polish);
        } else
            QLocale::setDefault(QLocale::English);
        const auto originalPalette = qApp->palette();
        MainWindow window;
        auto* smartHint = window.findChild<QLabel*>("smartHint");
        QVERIFY(smartHint);
        QVERIFY(smartHint->text().contains(language == "pl" ? "odczytywany ręcznie" : "read manually"));
        window.show();
        auto* center = window.findChild<SystemCenter*>();
        QVERIFY(center);
        auto* nav = window.findChild<QTreeWidget*>("sidebar");
        QVERIFY(nav);
        int ticks = 0;
        QTimer heartbeat;
        heartbeat.setInterval(10);
        connect(&heartbeat, &QTimer::timeout, this, [&ticks]() { ++ticks; });
        heartbeat.start();
        auto select = [&](const QString& module) {
            QTreeWidgetItemIterator it(nav);
            while (*it) {
                if ((*it)->data(0, Qt::UserRole).toString() == module) {
                    nav->setCurrentItem(*it);
                    QCoreApplication::processEvents();
                    return;
                }
                ++it;
            }
            QFAIL("Module missing from sidebar");
        };
        // Exercise all pages without invoking control, launch, restore or SMART actions.
        for (const auto& module : centerModules()) {
            select(module.id);
            QTest::qWait(10);
        }
        QTRY_VERIFY_WITH_TIMEOUT(center->snapshot("apps").lastSuccess.isValid(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(center->snapshot("packages").lastSuccess.isValid() || !center->snapshot("packages").error.isEmpty(),
                                 15000);
        QVERIFY(!center->snapshot("apps").rows.isEmpty());
        select("apps");
        auto* catalogTable = window.findChild<QTreeWidget*>("centerTable_apps");
        QVERIFY(catalogTable);
        QVERIFY(catalogTable->columnWidth(0) >= 200);
        QVERIFY(ticks >= 10);
        QCOMPARE(centerTitle("systemd"), language == "pl" ? QString("Usługi") : QString("Services"));
        select("metrics");
        QTRY_VERIFY_WITH_TIMEOUT(center->snapshot("metrics").lastSuccess.isValid(), 3000);
        const auto capture = qEnvironmentVariable("LSD_CAPTURE_DIR");
        if (!capture.isEmpty()) {
            QDir().mkpath(capture);
            for (const auto& module : {QString("overview"), QString("apps"), QString("metrics"), QString("settings")}) {
                select(module);
                QTest::qWait(module == "metrics" ? 2300 : 100);
                QVERIFY(window.grab().save(capture + "/" + module + "-" + language + "-" + QGuiApplication::platformName() + "-" +
                                           qEnvironmentVariable("QT_SCALE_FACTOR", "1") + ".png"));
            }
        }
        auto* search = window.findChild<QLineEdit*>("globalSearch");
        QVERIFY(search);
        search->setText("systemd");
        QTest::qWait(300);
        const auto results = center->search("systemd");
        QVERIFY(!results.rows.isEmpty());
        qInfo().noquote() << "Inventory counts:" << "applications" << center->snapshot("apps").rows.size() << "packages"
                          << center->snapshot("packages").rows.size() << "services" << center->snapshot("systemd").rows.size();
        if (!capture.isEmpty()) {
            auto* settings = window.findChild<SettingsPage*>();
            QVERIFY(settings);
            select("overview");
            for (const auto& theme : {QString("Light"), QString("Dark"), QString("OLED")}) {
                s.setValue("theme/preference", theme);
                QVERIFY(QMetaObject::invokeMethod(settings, "settingsChanged"));
                QTest::qWait(80);
                QVERIFY(window.grab().save(capture + "/theme-" + theme + "-" + language + "-" + QGuiApplication::platformName() + ".png"));
            }
        }
        qApp->setPalette(originalPalette);
        qApp->removeTranslator(&translator);
    }
};
int main(int argc, char** argv) {
    QTemporaryDir directory;
    const auto originalData = qEnvironmentVariable("XDG_DATA_HOME", QDir::homePath() + "/.local/share");
    qputenv("VBOX_USER_HOME", (directory.path() + "/vbox").toUtf8());
    qputenv("XDG_CONFIG_HOME", (directory.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (directory.path() + "/data").toUtf8());
    qputenv("XDG_DATA_DIRS", (originalData + ":" + qEnvironmentVariable("XDG_DATA_DIRS", "/usr/local/share:/usr/share")).toUtf8());
    QApplication app(argc, argv);
    const QPalette systemPalette = QApplication::palette();
    QApplication::setStyle("Fusion");
    QApplication::setPalette(systemPalette);
    app.setOrganizationName("LocalToolsTests");
    app.setApplicationName("SystemCenterTests");
    app.setApplicationVersion(LSD_VERSION);
    QGuiApplication::setDesktopFileName("io.github.Adiker.LinuxServiceDashboard");
    CenterUiTests test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_center_ui.moc"
