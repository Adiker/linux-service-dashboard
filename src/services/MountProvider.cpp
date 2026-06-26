#include "MountProvider.h"

#include "../core/MountProfileStore.h"
#include "../parsers/ProviderParsers.h"
#include "../utils/FstabParser.h"
#include "../utils/JsonUtils.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

static void collectAllTargets(const QJsonArray& array, QSet<QString>* targets) {
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        const QString target = object.value(QStringLiteral("target")).toString();
        // Ignore autofs placeholders: with x-systemd.automount the target carries an
        // autofs mount until first access, and treating that as "occupied" would hide
        // the configured fstab/profile row for a share that is not actually mounted.
        const bool isAutofs = object.value(QStringLiteral("fstype")).toString() == QStringLiteral("autofs");
        if (!target.isEmpty() && !isAutofs) {
            targets->insert(target);
        }
        if (object.contains(QStringLiteral("children"))) {
            collectAllTargets(object.value(QStringLiteral("children")).toArray(), targets);
        }
    }
}

static QVector<MountRow> mergeConfiguredMounts(const QVector<MountRow>& liveRows, const QSet<QString>& mountedTargets) {
    QVector<MountRow> merged = liveRows;
    QString fstabError;
    for (const MountRow& row : FstabParser::parseFile(QStringLiteral("/etc/fstab"), &fstabError)) {
        if (!mountedTargets.contains(row.target)) {
            merged.append(row);
        }
    }
    for (const MountRow& row : MountProfileStore::loadProfiles()) {
        if (!mountedTargets.contains(row.target)) {
            merged.append(row);
        }
    }
    return merged;
}

MountProvider::MountProvider(QObject* parent) : QObject(parent) {
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString&, const CommandResult& result, const QString& context) {
        if (context == QStringLiteral("mount-list")) {
            QString error;
            QVector<MountRow> rows;
            if (result.ok()) {
                QString parseError;
                rows = ProviderParsers::parseFindmntJson(result.standardOutput, &error);
                const QJsonDocument document = parseJsonDocument(result.standardOutput, &parseError);
                const QJsonArray filesystems = document.object().value(QStringLiteral("filesystems")).toArray();
                if (m_includeConfigured) {
                    // Build the occupancy set from every findmnt target, not just the
                    // network rows above. Otherwise a profile/fstab target currently
                    // mounted with another filesystem type (bind, tmpfs, local disk)
                    // would still be shown as a configured row, and Unmount would run
                    // umount on that unrelated live mount.
                    QSet<QString> mountedTargets;
                    collectAllTargets(filesystems, &mountedTargets);
                    rows = mergeConfiguredMounts(rows, mountedTargets);
                }
                if (!parseError.isEmpty() && error.isEmpty()) {
                    error = QStringLiteral("Could not parse findmnt JSON: %1").arg(parseError);
                }
            } else {
                error = result.startFailed ? QStringLiteral("findmnt not found. Install util-linux.") : result.standardError.trimmed();
            }
            emit mountsReady(rows, error);
        } else if (context.startsWith(QStringLiteral("mount-unmount:"))) {
            const QString target = context.mid(QStringLiteral("mount-unmount:").size());
            emit actionFinished(result.ok() ? QStringLiteral("Unmounted %1.").arg(target)
                                            : QStringLiteral("Could not unmount %1.").arg(target),
                                result.ok() ? result.standardOutput : result.standardError);
        }
    });
}

void MountProvider::refreshMounts(bool includeConfigured) {
    m_includeConfigured = includeConfigured;
    m_runner.run(QStringLiteral("findmnt"), {QStringLiteral("--json")}, 15000, QStringLiteral("mount-list"));
}

void MountProvider::unmount(const QString& target) {
    m_runner.run(QStringLiteral("umount"), {target}, 30000, QStringLiteral("mount-unmount:%1").arg(target));
}
