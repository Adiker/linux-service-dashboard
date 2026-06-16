#include "CommandRunner.h"

#include <QProcess>
#include <QTimer>
#include <QUuid>

#include <functional>
#include <memory>

CommandRunner::CommandRunner(QObject *parent)
    : QObject(parent)
{
}

QString CommandRunner::run(const QString &program,
                           const QStringList &arguments,
                           int timeoutMs,
                           const QString &context)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *process = new QProcess(this);
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);

    auto result = std::make_shared<CommandResult>();
    auto completed = std::make_shared<bool>(false);
    result->program = program;
    result->arguments = arguments;

    std::function<void(int)> finishOnce = [this, process, timer, result, completed, id, context](int exitCode) {
        if (*completed) {
            return;
        }
        *completed = true;
        timer->stop();
        result->exitCode = exitCode;
        result->standardOutput = QString::fromLocal8Bit(process->readAllStandardOutput());
        result->standardError = QString::fromLocal8Bit(process->readAllStandardError());
        if (result->errorString.isEmpty()) {
            result->errorString = process->errorString();
        }

        const CommandResult value = *result;
        if (!value.ok()) {
            m_failedCommandLog.append(QStringLiteral("%1: %2").arg(value.commandLine(), value.standardError.trimmed()));
        }
        emit commandFinished(id, value, context);
        process->deleteLater();
    };

    connect(timer, &QTimer::timeout, process, [process, result, finishOnce]() {
        result->timedOut = true;
        if (process->state() == QProcess::NotRunning) {
            finishOnce(-1);
            return;
        }
        process->kill();
    });

    connect(process, &QProcess::errorOccurred, process, [process, result, finishOnce](QProcess::ProcessError error) {
        result->errorString = process->errorString();
        if (error == QProcess::FailedToStart) {
            result->startFailed = true;
            finishOnce(-1);
        }
    });

    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [finishOnce](int exitCode, QProcess::ExitStatus) {
                finishOnce(exitCode);
            });

    process->setProgram(program);
    process->setArguments(arguments);
    process->start();
    timer->start(timeoutMs);
    return id;
}

QStringList CommandRunner::failedCommandLog() const
{
    return m_failedCommandLog;
}
