#include "src/core/CommandRunner.h"

#include <QSignalSpy>
#include <QThread>
#include <QtTest>
#include <cstdio>
#include <cstring>
class RunnerTests : public QObject {
    Q_OBJECT
  private slots:
    void failuresAndDeduplication() {
        CommandRunner runner;
        QSignalSpy spy(&runner, &CommandRunner::commandFinished);
        const auto id = runner.run("/bin/sleep", {"10"}, 25, "same");
        QCOMPARE(runner.run("/bin/sleep", {"10"}, 25, "same"), id);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1500);
        QVERIFY(qvariant_cast<CommandResult>(spy.takeFirst()[1]).timedOut);
        runner.run("/this/program/does/not/exist");
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1500);
        QVERIFY(qvariant_cast<CommandResult>(spy.takeFirst()[1]).startFailed);
    }
    void splitUtf8IsDecodedOnce() {
        CommandRunner runner;
        QSignalSpy spy(&runner, &CommandRunner::commandFinished);
        runner.run(QCoreApplication::applicationFilePath(), {"--emit-split-utf8"});
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1500);
        const auto r = qvariant_cast<CommandResult>(spy.takeFirst()[1]);
        QVERIFY(r.ok());
        QCOMPARE(r.standardOutput, QString::fromUtf8("Ś"));
    }
    void boundedOutput() {
        CommandRunner runner;
        QSignalSpy spy(&runner, &CommandRunner::commandFinished);
        runner.run("/bin/sh", {"-c", "head -c 3000000 /dev/zero"});
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
        const auto r = qvariant_cast<CommandResult>(spy.takeFirst()[1]);
        QVERIFY(r.outputTruncated);
        QVERIFY(!r.ok());
        QVERIFY(r.standardOutput.size() <= 2 * 1024 * 1024);
    }
};
int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--emit-split-utf8") == 0) {
        const char first = char(0xc5), second = char(0x9a);
        std::fwrite(&first, 1, 1, stdout);
        std::fflush(stdout);
        QThread::msleep(150);
        std::fwrite(&second, 1, 1, stdout);
        std::fflush(stdout);
        return 0;
    }
    QCoreApplication app(argc, argv);
    RunnerTests test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_command_runner.moc"
