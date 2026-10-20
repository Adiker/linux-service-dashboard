#include "CommandRunner.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUuid>
#include <memory>

CommandRunner::CommandRunner(QObject* parent) : QObject(parent) {}
CommandRunner::~CommandRunner() {
    for (auto* process : findChildren<QProcess*>()) {
        process->disconnect();
        process->kill();
        process->waitForFinished(100);
    }
}
QString CommandRunner::run(const QString& program, const QStringList& arguments, int timeoutMs, const QString& context) {
    if (!context.isEmpty() && m_pending.contains(context))
        return m_pending.value(context);
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!context.isEmpty())
        m_pending[context] = id;
    auto* process = new QProcess(this);
    auto* timer = new QTimer(process);
    timer->setSingleShot(true);
    auto result = std::make_shared<CommandResult>();
    auto completed = std::make_shared<bool>(false);
    result->program = program;
    result->arguments = arguments;
    auto drain = [process, result]() {
        const auto append = [result](QString& target, const QByteArray& data) {
            constexpr qsizetype limit = 2 * 1024 * 1024;
            const QString value = QString::fromLocal8Bit(data);
            const auto remaining = qMax<qsizetype>(0, limit - target.size());
            target += value.left(remaining);
            if (value.size() > remaining)
                result->outputTruncated = true;
        };
        append(result->standardOutput, process->readAllStandardOutput());
        append(result->standardError, process->readAllStandardError());
    };
    const auto finish = [this, process, timer, result, completed, id, context, drain](int code) {
        if (*completed)
            return;
        *completed = true;
        timer->stop();
        drain();
        result->exitCode = code;
        if (result->errorString.isEmpty() && !result->ok())
            result->errorString = process->errorString();
        if (result->timedOut)
            result->standardError += "\nCommand timed out.";
        if (result->outputTruncated)
            result->standardError += "\nOutput limit exceeded (2 MiB per stream).";
        if (result->startFailed)
            result->standardError += "\n" + result->errorString;
        if (!context.isEmpty())
            m_pending.remove(context);
        if (!result->ok()) {
            // Never retain raw arguments or command output in the diagnostic history.
            m_failedCommandLog.append(result->program + ": exit " + QString::number(code));
            while (m_failedCommandLog.size() > 100)
                m_failedCommandLog.removeFirst();
        }
        emit commandFinished(id, *result, context);
        process->deleteLater();
    };
    connect(process, &QProcess::readyReadStandardOutput, process, drain);
    connect(process, &QProcess::readyReadStandardError, process, drain);
    connect(timer, &QTimer::timeout, process, [process, result]() {
        result->timedOut = true;
        process->kill();
    });
    connect(process, &QProcess::errorOccurred, process, [result, finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            result->startFailed = true;
            finish(-1);
        }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
            [finish](int code, QProcess::ExitStatus) { finish(code); });
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C.UTF-8");
    env.insert("SYSTEMD_PAGER", "");
    env.insert("NO_COLOR", "1");
    process->setProcessEnvironment(env);
    process->setStandardInputFile(QProcess::nullDevice());
    process->setProgram(program);
    process->setArguments(arguments);
    process->start();
    timer->start(qMax(1, timeoutMs));
    return id;
}
QStringList CommandRunner::failedCommandLog() const {
    return m_failedCommandLog;
}
