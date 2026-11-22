#pragma once

#include <QString>
#include <QStringList>

enum class CommandErrorKind {
    None,
    MissingTool,
    PermissionDenied,
    Timeout,
    ExecutionFailed,
};

inline QString commandErrorKindLabel(CommandErrorKind kind)
{
    switch (kind) {
    case CommandErrorKind::None:
        return QStringLiteral("OK");
    case CommandErrorKind::MissingTool:
        return QStringLiteral("Missing tool");
    case CommandErrorKind::PermissionDenied:
        return QStringLiteral("Permission denied");
    case CommandErrorKind::Timeout:
        return QStringLiteral("Timed out");
    case CommandErrorKind::ExecutionFailed:
        return QStringLiteral("Command failed");
    }
    return QStringLiteral("Unknown");
}

inline bool isPermissionErrorText(const QString &text)
{
    return text.contains(QStringLiteral("permission denied"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("not authorized"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("operation not permitted"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("access denied"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("must be root"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("only root can"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("interactive authentication required"), Qt::CaseInsensitive);
}

struct CommandResult {
    QString program;
    QStringList arguments;
    QString standardOutput;
    QString standardError;
    QString errorString;
    int exitCode = -1;
    bool timedOut = false;
    bool startFailed = false;

    [[nodiscard]] bool ok() const
    {
        return !timedOut && !startFailed && exitCode == 0;
    }

    [[nodiscard]] CommandErrorKind errorKind() const
    {
        if (ok()) {
            return CommandErrorKind::None;
        }
        if (startFailed) {
            return CommandErrorKind::MissingTool;
        }
        if (timedOut) {
            return CommandErrorKind::Timeout;
        }
        const QString combined = QStringList{standardError, errorString}.join(QLatin1Char(' '));
        if (isPermissionErrorText(combined)) {
            return CommandErrorKind::PermissionDenied;
        }
        return CommandErrorKind::ExecutionFailed;
    }

    [[nodiscard]] QString commandLine() const
    {
        QStringList parts{program};
        parts.append(arguments);
        return parts.join(QLatin1Char(' '));
    }
};
