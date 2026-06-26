#include "FstabParser.h"

#include <QFile>
#include <QRegularExpression>

namespace FstabParser {

namespace {

bool isNetworkFilesystem(const QString& type) {
    return type == QStringLiteral("cifs") || type == QStringLiteral("nfs") || type == QStringLiteral("nfs4") ||
           type == QStringLiteral("sshfs") || type == QStringLiteral("fuse.sshfs");
}

QString decodeFstabField(const QString& field) {
    QString decoded;
    decoded.reserve(field.size());
    for (int i = 0; i < field.size(); ++i) {
        if (field.at(i) == QLatin1Char('\\') && i + 1 < field.size()) {
            const QChar next = field.at(i + 1);
            if (next.isDigit()) {
                int j = i + 1;
                QString octal;
                while (j < field.size() && j < i + 4 && field.at(j).isDigit()) {
                    octal += field.at(j);
                    ++j;
                }
                decoded += QChar(octal.toInt(nullptr, 8));
                i = j - 1;
                continue;
            }
            decoded += next;
            ++i;
            continue;
        }
        decoded += field.at(i);
    }
    return decoded;
}

} // namespace

QVector<MountRow> parseFile(const QString& path, QString* error) {
    QVector<MountRow> rows;
    if (error) {
        error->clear();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Could not read %1").arg(path);
        }
        return rows;
    }
    const QString content = QString::fromUtf8(file.readAll());
    const QStringList lines = content.split('\n', Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() < 4) {
            continue;
        }
        const QString type = fields.at(2);
        const QString rawSource = fields.at(0);
        // Legacy SSHFS fstab form: "sshfs#user@host:/path /mnt fuse ...", where the
        // type is the generic "fuse" and the source carries the sshfs# prefix.
        const bool isSshfsFuse = type == QStringLiteral("fuse") && rawSource.startsWith(QStringLiteral("sshfs#"));
        if (!isNetworkFilesystem(type) && !isSshfsFuse) {
            continue;
        }
        MountRow row;
        row.source = decodeFstabField(rawSource);
        row.target = decodeFstabField(fields.at(1));
        row.filesystemType = isSshfsFuse ? QStringLiteral("sshfs") : type;
        row.options = fields.at(3);
        row.status = QStringLiteral("Configured (fstab)");
        rows.append(row);
    }
    return rows;
}

} // namespace FstabParser
