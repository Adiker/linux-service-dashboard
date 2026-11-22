#pragma once

#include "CommandResult.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

struct FailedCommandEntry {
    QString timestamp;
    QString commandLine;
    QString context;
    QString message;
    CommandErrorKind errorKind = CommandErrorKind::ExecutionFailed;
};

class CommandRunner : public QObject {
    Q_OBJECT

public:
    static constexpr int kMaxFailedLogEntries = 200;

    explicit CommandRunner(QObject *parent = nullptr);

    QString run(const QString &program,
                const QStringList &arguments = {},
                int timeoutMs = 15000,
                const QString &context = {},
                bool deduplicateContext = false);

    [[nodiscard]] bool isContextActive(const QString &context) const;
    [[nodiscard]] QStringList failedCommandLog() const;
    [[nodiscard]] QVector<FailedCommandEntry> failedCommands() const;
    void clearFailedCommands();

signals:
    void commandFinished(const QString &id, const CommandResult &result, const QString &context);
    void failedCommandRecorded(const FailedCommandEntry &entry);

private:
    QVector<FailedCommandEntry> m_failedCommands;
    QSet<QString> m_activeContexts;
};
