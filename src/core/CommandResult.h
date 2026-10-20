#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>

struct CommandResult {
    QString program;
    QStringList arguments;
    QString standardOutput;
    QString standardError;
    QString errorString;
    int exitCode = -1;
    bool timedOut = false;
    bool startFailed = false;
    bool outputTruncated = false;

    [[nodiscard]] bool ok() const { return !timedOut && !startFailed && !outputTruncated && exitCode == 0; }

    [[nodiscard]] QString commandLine() const {
        QStringList parts{program};
        parts.append(arguments);
        return parts.join(' ');
    }
};

Q_DECLARE_METATYPE(CommandResult)
