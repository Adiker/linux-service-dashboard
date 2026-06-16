#pragma once

#include <QJsonDocument>
#include <QString>

QJsonDocument parseJsonDocument(const QString &text, QString *error = nullptr);
