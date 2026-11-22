#include "MountProvider.h"

#include "../core/MountProfileStore.h"
#include "../parsers/ProviderParsers.h"
#include "../utils/FstabParser.h"

#include <QSet>

namespace {

QVector<MountRow> mergeConfiguredMounts(const QVector<MountRow> &liveRows, const QSet<QString> &mountedTargets)
{
    QVector<MountRow> merged = liveRows;
    QString fstabError;
    for (const MountRow &row : FstabParser::parseFile(QStringLiteral("/etc/fstab"), &fstabError)) {
        if (!mountedTargets.contains(row.target)) {
            merged.append(row);
        }
    }
    for (const MountRow &row : MountProfileStore::loadProfiles()) {
        if (!mountedTargets.contains(row.target)) {
            merged.append(row);
        }
    }
    return merged;
}

} // namespace

MountProvider::MountProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("mount-list")) {
            QVector<MountRow> rows;
            QString error;
            if (result.ok()) {
                QSet<QString> mountedTargets;
                ProviderParsers::parseFindmntSnapshot(
                    result.standardOutput,
                    &rows,
                    m_includeConfigured ? &mountedTargets : nullptr,
                    &error);
                if (m_includeConfigured && error.isEmpty()) {
                    rows = mergeConfiguredMounts(rows, mountedTargets);
                }
            } else {
                error = result.startFailed ? QStringLiteral("findmnt not found. Install util-linux.") : result.standardError.trimmed();
            }
            emit mountsReady(rows, error);
        } else if (context.startsWith(QStringLiteral("mount-mount:"))) {
            const QString target = context.mid(QStringLiteral("mount-mount:").size());
            QString details = result.ok() ? result.standardOutput : result.standardError;
            if (!result.ok() && result.errorKind() == CommandErrorKind::PermissionDenied) {
                details += QStringLiteral("\n\nMounting requires root privileges or the 'user'/'users' option in /etc/fstab.");
            }
            emit actionFinished(result.ok() ? QStringLiteral("Mounted %1.").arg(target) : QStringLiteral("Could not mount %1.").arg(target),
                                details.trimmed());
        } else if (context.startsWith(QStringLiteral("mount-unmount:"))) {
            const QString target = context.mid(QStringLiteral("mount-unmount:").size());
            emit actionFinished(result.ok() ? QStringLiteral("Unmounted %1.").arg(target) : QStringLiteral("Could not unmount %1.").arg(target),
                                result.ok() ? result.standardOutput : result.standardError);
        }
    });
}

void MountProvider::refreshMounts(bool includeConfigured)
{
    m_includeConfigured = includeConfigured;
    m_runner.run(QStringLiteral("findmnt"),
                 {QStringLiteral("--json")},
                 15000,
                 QStringLiteral("mount-list"),
                 /*deduplicateContext=*/true);
}

void MountProvider::mount(const QString &target)
{
    m_runner.run(QStringLiteral("mount"), {target}, 30000, QStringLiteral("mount-mount:%1").arg(target));
}

void MountProvider::unmount(const QString &target)
{
    m_runner.run(QStringLiteral("umount"), {target}, 30000, QStringLiteral("mount-unmount:%1").arg(target));
}
