#include "FstabParser.h"

#include "../parsers/ProviderParsers.h"

#include <QFile>

namespace FstabParser {

QVector<MountRow> parseFile(const QString &path, QString *error)
{
    if (error) {
        error->clear();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Could not read %1").arg(path);
        }
        return {};
    }
    const QString content = QString::fromUtf8(file.readAll());
    return ProviderParsers::parseFstabNetworkMounts(content, error);
}

} // namespace FstabParser
