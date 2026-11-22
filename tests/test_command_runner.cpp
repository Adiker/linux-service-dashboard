#include "src/core/CommandRunner.h"

#include <QSignalSpy>
#include <QtTest>
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
QTEST_GUILESS_MAIN(RunnerTests)
#include "test_command_runner.moc"
