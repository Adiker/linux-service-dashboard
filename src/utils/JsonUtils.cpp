#include "JsonUtils.h"

#include <QJsonParseError>

QJsonDocument parseJsonDocument(const QString &text, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (error && parseError.error != QJsonParseError::NoError) {
        *error = parseError.errorString();
    }
    return document;
}
