#pragma once

#include "../core/CommandRunner.h"
#include "../models/MountModel.h"

#include <QObject>

class MountProvider : public QObject {
    Q_OBJECT

public:
    explicit MountProvider(QObject *parent = nullptr);
    void refreshMounts();
    void unmount(const QString &target);

signals:
    void mountsReady(const QVector<MountRow> &rows, const QString &error);
    void actionFinished(const QString &message, const QString &details);

private:
    CommandRunner m_runner;
};
