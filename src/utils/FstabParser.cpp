#include "FstabParser.h"

#include <QFile>
#include <QRegularExpression>

namespace FstabParser {

namespace {

bool isNetworkFilesystem(const QString &type)
{
    return type == QStringLiteral("cifs")
        || type == QStringLiteral("nfs")
        || type == QStringLiteral("nfs4")
        || type == QStringLiteral("sshfs");
}

QString decodeFstabField(const QString &field)
{
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

QVector<MountRow> parseFile(const QString &path, QString *error)
{
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
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() < 4) {
            continue;
        }
        const QString type = fields.at(2);
        if (!isNetworkFilesystem(type)) {
            continue;
        }
        MountRow row;
        row.source = decodeFstabField(fields.at(0));
        row.target = decodeFstabField(fields.at(1));
        row.filesystemType = type;
        row.options = fields.at(3);
        row.status = QStringLiteral("Configured (fstab)");
        rows.append(row);
    }
    return rows;
}

} // namespace FstabParser
