#pragma once

#include "../core/CommandRunner.h"
#include "../models/DockerContainerModel.h"

#include <QObject>

class DockerProvider : public QObject {
    Q_OBJECT

public:
    explicit DockerProvider(QObject *parent = nullptr);
    void refreshContainers();
    void containerLogs(const QString &container);
    void inspectContainer(const QString &container);
    void controlContainer(const QString &container, const QString &action);

signals:
    void containersReady(const QVector<DockerContainerRow> &rows, const QString &error);
    void textReady(const QString &title, const QString &text);
    void actionFinished(const QString &message, const QString &details);

private:
    CommandRunner m_runner;
};
