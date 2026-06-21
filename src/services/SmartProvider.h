#pragma once

#include "../core/CommandRunner.h"
#include "../models/DiskModel.h"

#include <QHash>
#include <QObject>
#include <QQueue>

class SmartProvider : public QObject {
    Q_OBJECT

public:
    explicit SmartProvider(QObject *parent = nullptr);
    void refreshDisks();
    void checkSmart(const QString &devicePath);
    void checkSmart(const DiskRow &row);
    void checkSmart(const QVector<DiskRow> &rows);

signals:
    void disksReady(const QVector<DiskRow> &rows, const QString &error);
    void smartReady(const QString &devicePath, const DiskRow &smartRow, const QString &error);

private:
    void startNextSmartCheck();
    void runDirectSmartCheck(const DiskRow &row);
    bool runPrivilegedSmartChecks(const DiskRow &firstRow);
    void finishPrivilegedSmartChecks(const CommandResult &result);

    CommandRunner m_runner;
    QQueue<DiskRow> m_pendingSmartChecks;
    QHash<QString, DiskRow> m_activeSmartChecks;
    QVector<DiskRow> m_privilegedSmartChecks;
    bool m_smartCheckRunning = false;
};
