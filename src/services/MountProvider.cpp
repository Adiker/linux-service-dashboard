#include "MountProvider.h"

#include "../core/MountProfileStore.h"
#include "../utils/FstabParser.h"
#include "../utils/JsonUtils.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

static void collectMounts(const QJsonArray &array, QVector<MountRow> *rows)
{
    static const QSet<QString> interestingTypes{QStringLiteral("cifs"), QStringLiteral("nfs"), QStringLiteral("nfs4"), QStringLiteral("sshfs")};
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        const QString type = object.value(QStringLiteral("fstype")).toString();
        if (interestingTypes.contains(type)) {
            MountRow row;
            row.target = object.value(QStringLiteral("target")).toString();
            row.source = object.value(QStringLiteral("source")).toString();
            row.filesystemType = type;
            row.options = object.value(QStringLiteral("options")).toString();
            row.status = QStringLiteral("Mounted");
            rows->append(row);
        }
        if (object.contains(QStringLiteral("children"))) {
            collectMounts(object.value(QStringLiteral("children")).toArray(), rows);
        }
    }
}

static void collectAllTargets(const QJsonArray &array, QSet<QString> *targets)
{
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        const QString target = object.value(QStringLiteral("target")).toString();
        if (!target.isEmpty()) {
            targets->insert(target);
        }
        if (object.contains(QStringLiteral("children"))) {
            collectAllTargets(object.value(QStringLiteral("children")).toArray(), targets);
        }
    }
}

static QVector<MountRow> mergeConfiguredMounts(const QVector<MountRow> &liveRows, const QSet<QString> &mountedTargets)
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

MountProvider::MountProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_runner, &CommandRunner::commandFinished, this, [this](const QString &, const CommandResult &result, const QString &context) {
        if (context == QStringLiteral("mount-list")) {
            QVector<MountRow> rows;
            QString error;
            if (result.ok()) {
                QString parseError;
                const QJsonDocument document = parseJsonDocument(result.standardOutput, &parseError);
                const QJsonArray filesystems = document.object().value(QStringLiteral("filesystems")).toArray();
                collectMounts(filesystems, &rows);
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
                if (!parseError.isEmpty()) {
                    error = QStringLiteral("Could not parse findmnt JSON: %1").arg(parseError);
                }
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

void MountProvider::refreshMounts(bool includeConfigured)
{
    m_includeConfigured = includeConfigured;
    m_runner.run(QStringLiteral("findmnt"), {QStringLiteral("--json")}, 15000, QStringLiteral("mount-list"));
}

void MountProvider::unmount(const QString &target)
{
    m_runner.run(QStringLiteral("umount"), {target}, 30000, QStringLiteral("mount-unmount:%1").arg(target));
}
