#include "MountProvider.h"

#include "../parsers/ProviderParsers.h"

MountProvider::MountProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("mount-list")) {
            QString error;
            QVector<MountRow> rows;
            if (result.ok()) {
                rows = ProviderParsers::parseFindmntJson(result.standardOutput, &error);
            } else {
                error = result.startFailed ? QStringLiteral("findmnt not found. Install util-linux.") : result.standardError.trimmed();
            }
            emit mountsReady(rows, error);
        } else if (context.startsWith(QStringLiteral("mount-unmount:"))) {
            const QString target = context.mid(QStringLiteral("mount-unmount:").size());
            emit actionFinished(result.ok() ? QStringLiteral("Unmounted %1.").arg(target) : QStringLiteral("Could not unmount %1.").arg(target),
                                result.ok() ? result.standardOutput : result.standardError);
        }
    });
}

void MountProvider::refreshMounts()
{
    m_runner.run(QStringLiteral("findmnt"), {QStringLiteral("--json")}, 15000, QStringLiteral("mount-list"));
}

void MountProvider::unmount(const QString &target)
{
    m_runner.run(QStringLiteral("umount"), {target}, 30000, QStringLiteral("mount-unmount:%1").arg(target));
}
