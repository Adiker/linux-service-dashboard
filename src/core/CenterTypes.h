#pragma once
#include <QDateTime>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

struct CenterRecord {
    QString id;
    QStringList cells;
    QStringList groups;
    QVariantMap meta;
    bool attention = false;
};
struct CenterSnapshot {
    QStringList headers;
    QVector<CenterRecord> rows;
    QDateTime lastSuccess;
    QString error;
};
struct ModuleDescriptor {
    QString id, title, group, tool;
    bool onDemand = false;
};
QVector<ModuleDescriptor> centerModules();
QString centerTitle(const QString& id);
bool centerModuleEnabled(const QString& id);
