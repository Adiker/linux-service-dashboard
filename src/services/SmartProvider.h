#pragma once

#include "../core/CommandRunner.h"
#include "../models/DiskModel.h"

#include <QObject>

class SmartProvider : public QObject {
    Q_OBJECT

public:
    explicit SmartProvider(QObject *parent = nullptr);
    void refreshDisks();
    void checkSmart(const QString &devicePath);

signals:
    void disksReady(const QVector<DiskRow> &rows, const QString &error);
    void smartReady(const QString &devicePath, const DiskRow &smartRow, const QString &error);

private:
    CommandRunner m_runner;
};
