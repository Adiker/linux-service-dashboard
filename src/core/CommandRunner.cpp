#include "CommandRunner.h"

#include "../utils/TimeUtils.h"

#include <QProcess>
#include <QTimer>
#include <QUuid>
#include <functional>
#include <memory>

CommandRunner::CommandRunner(QObject* parent) : QObject(parent) {}

QString CommandRunner::run(const QString& program, const QStringList& arguments, int timeoutMs, const QString& context,
                           bool deduplicateContext) {
    if (deduplicateContext && !context.isEmpty() && m_activeContexts.contains(context)) {
        return {};
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (deduplicateContext && !context.isEmpty()) {
        m_activeContexts.insert(context);
    }

    auto* process = new QProcess(this);
    auto* timer = new QTimer(process);
    timer->setSingleShot(true);

    auto result = std::make_shared<CommandResult>();
    auto completed = std::make_shared<bool>(false);
    result->program = program;
    result->arguments = arguments;

    std::function<void(int)> finishOnce = [this, process, timer, result, completed, id, context, deduplicateContext](int exitCode) {
        if (*completed) {
            return;
        }
        *completed = true;
        timer->stop();
        if (deduplicateContext && !context.isEmpty()) {
            m_activeContexts.remove(context);
        }

        result->exitCode = exitCode;
        result->standardOutput = QString::fromLocal8Bit(process->readAllStandardOutput());
        result->standardError = QString::fromLocal8Bit(process->readAllStandardError());
        if (result->errorString.isEmpty()) {
            result->errorString = process->errorString();
        }

        const CommandResult value = *result;
        if (!value.ok()) {
            QString detail = value.standardError.trimmed();
            if (detail.isEmpty()) {
                detail = value.timedOut ? QStringLiteral("Command timed out.") : value.errorString.trimmed();
            }
            FailedCommandEntry entry;
            entry.timestamp = currentTimestamp();
            entry.commandLine = value.commandLine();
            entry.context = context;
            entry.message = detail;
            entry.errorKind = value.errorKind();

            m_failedCommands.append(entry);
            while (m_failedCommands.size() > kMaxFailedLogEntries) {
                m_failedCommands.removeFirst();
            }
            emit failedCommandRecorded(entry);
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

    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [finishOnce](int exitCode, QProcess::ExitStatus) { finishOnce(exitCode); });

    process->setProgram(program);
    process->setArguments(arguments);
    process->start();
    timer->start(timeoutMs);
    return id;
}

bool CommandRunner::isContextActive(const QString& context) const {
    return m_activeContexts.contains(context);
}

QStringList CommandRunner::failedCommandLog() const {
    QStringList lines;
    lines.reserve(m_failedCommands.size());
    for (const FailedCommandEntry& entry : m_failedCommands) {
        lines.append(QStringLiteral("[%1] %2: %3").arg(entry.timestamp, entry.commandLine, entry.message));
    }
    return lines;
}

QVector<FailedCommandEntry> CommandRunner::failedCommands() const {
    return m_failedCommands;
}

void CommandRunner::clearFailedCommands() {
    m_failedCommands.clear();
}
